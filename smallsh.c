#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

bool isBG = false;
bool allowBG = true;
char inputFileName[256];
char outputFileName[256];
char* arguments[512];
char inputLine[2048];

void catchCtrlZ ();
void callCD ();
void callStatus (int);
void clearArguments ();
void deleteNewLine ();
void callOtherCommand (struct sigaction, int*);
void getUserInput ();

int main (int argc, char* argv[]) {
    int exitStatus = 0;
    int i = 0;
    for (i = 0; i < 512; i++) {
        arguments[i] = NULL;
    }

    struct sigaction ctrlC = {0};
    ctrlC.sa_handler = SIG_IGN;
    sigfillset(&ctrlC.sa_mask);
    ctrlC.sa_flags = 0;
    sigaction(SIGINT, &ctrlC, NULL);

    struct sigaction ctrlZ = {0};
    ctrlZ.sa_handler = catchCtrlZ;
    sigfillset(&ctrlZ.sa_mask);
    ctrlZ.sa_flags = 0;
    sigaction(SIGTSTP, &ctrlZ, NULL);

    while (true) {
        getUserInput();

        if(!strcmp(arguments[0], "status")) {
            callStatus(exitStatus);
            fflush(stdout);
        } else if (!strcmp(arguments[0], "cd")) {
            callCD();
        } else if (!strcmp(arguments[0], "exit")) {
            break;
        } else if (arguments[0][0] == '\0' || arguments[0][0] == '#') {
            continue;
        } else {
            callOtherCommand(ctrlC, &exitStatus);
        }
        clearArguments();
    }
    return 0;
}

void catchCtrlZ () {
    if (allowBG) {
        write(1, "Entering foreground-only mode (& is now ignored).\n", 50);
        fflush(stdout);
        allowBG = false;
    } else {
        write(1, "Exiting foreground-only mode.\n", 30);
        fflush(stdout);
        allowBG = true;
    }
}

void callCD () {
    if (arguments[1]) {
        if (chdir(arguments[1]) == -1) {
            printf("Directory not found.\n");
            fflush(stdout);
        }
    } else {
        chdir(getenv("HOME"));
    }
}

void callStatus (int exitStatus) {
    if (WIFEXITED(exitStatus)) {
		printf("exit value %d\n", WEXITSTATUS(exitStatus));
	} else {
		printf("terminated by signal %d\n", WTERMSIG(exitStatus));
	}
}


void clearArguments () {
    int i = 0;
    for (i = 0; arguments[i]; i++) {
        arguments[i] = NULL;
    }
    isBG = false;
    inputFileName[0] = '\0';
    outputFileName[0] = '\0';
}

void deleteNewLine () {
    int i = 0;
    int isNewLine = false;
    for (i = 0; i < 2048 && !isNewLine; i++) {
        if (inputLine[i] == '\n') {
            inputLine[i] = '\0';
            isNewLine = true;
        }
    }
}

void callOtherCommand (struct sigaction sigAct, int* exitStatus) {
    pid_t childPid = -5;
    int final, inputFile, outputFile = 0;

    childPid = fork();
    switch (childPid) {
    case -1:
        perror("Forking error.\n");
        exit(1);
        break;
    case 0:
        sigAct.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sigAct, NULL);

        if (strcmp(inputFileName, "")) {
            inputFile = open(inputFileName, O_RDONLY);
            if (inputFile == -1) {
                perror("Error opening file.\n");
                exit(1);
            }
            final = dup2(inputFile, 0);
            if (final == -1) {
                perror("Error assigning input file.\n");
                exit(2);
            }
            fcntl(inputFile, F_SETFD, FD_CLOEXEC);
        }

        if (strcmp(outputFileName, "")) {
            outputFile = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (outputFile == -1) {
                perror("Error opening file.\n");
                exit(1);
            }
            final = dup2(outputFile, 1);
            if (final == -1) {
                perror("Error assigning input file.\n");
                exit(2);
            }
            fcntl(outputFile, F_SETFD, FD_CLOEXEC);
        }

        if (execvp(arguments[0], (char* const*)arguments)) {
            printf("%s: No such file or directory.\n", arguments[0]);
            fflush(stdout);
            exit(2);
        }
        break;
    default:
        if (isBG && allowBG) {
            pid_t pid = waitpid(childPid, exitStatus, WNOHANG);
            printf("Background pid is: %d\n", childPid);
            fflush(stdout);
        } else {
            pid_t pid = waitpid(childPid, exitStatus, 0);
        }

        while ((childPid = waitpid(-1, exitStatus, WNOHANG)) > 0) {
            printf("Child %d terminated.\n", childPid);
            callStatus(*exitStatus);
            fflush(stdout);
        }
    }
}

void getUserInput () {
    int i, j = 0;
    bool notDone = true;
    char* argument = NULL;

    printf(": ");
    fflush(stdout);
    fgets(inputLine, 2048, stdin);
    deleteNewLine();

    if (!strcmp(inputLine, "")) {
        arguments[0] = strdup("");
        return;
    }

    argument = strtok(inputLine, " ");

    for (i = 0; argument; i++) {
        if(!strcmp(argument, ">")) {
            argument = strtok(NULL, " ");
            strcpy(outputFileName, argument);
            printf("Output file name: %s\n", outputFileName);
            fflush(stdout);
        } else if (!strcmp(argument, "<")) {
            argument = strtok(NULL, " ");
            strcpy(inputFileName, argument);
            printf("Input file name: %s\n", inputFileName);
            fflush(stdout);
        } else if (!strcmp(argument, "&")) {
            isBG = true;
        } else {
            arguments[i] = strdup(argument);
            for (j = 0; arguments[i][j]; j++) {
                if (arguments[i][j] == '$' && arguments[i][j + 1] == '$') {
                    arguments[i][j] = '\0';
                    snprintf(arguments[i], 256, "%s%d", arguments[i], getpid());
                }
            }
        }
        argument = strtok(NULL, " ");
    }
}