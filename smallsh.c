#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXINPUTLENGTH 2048
#define MAXARGUMENTS 512

int isBG = 1;

void getUserInput (char* command[], int pid, int* background, char inputFile[], char outputFile[]) {
    char currInput[MAXINPUTLENGTH];

    printf(": ");
    fflush(stdout);
    fgets(currInput, MAXINPUTLENGTH, stdin);
    
    int i;
    for (i = 0; i < MAXINPUTLENGTH; i++) {
        if (currInput[i] == '\0') {
            currInput[i - 1] = '\0';
        }
    }

    if (!strcmp(currInput, "")) {
        command[0] = strdup("");
        return;
    }

    char* tok = strtok(currInput, " ");

    int done = 1;
    int j, k = 0;
    while (done) {
        if(!strcmp(tok, ">")) {
            tok = strtok(NULL, " ");
            strcpy(outputFile, tok);
        } else if (!strcmp(tok, "<")) {
            tok = strtok(NULL, " ");
            strcpy(inputFile, tok);
        } else if (!strcmp(tok, "&")) {
            *background = 1;
        } else {
            command[j] = strdup(tok);
            for (k = 0; k < command[j][k]; k++) {
                if (command[j][k] == '$' && command[j][k + 1] == '$') {
                    command[j][k] = '\0';
                    snprintf(command[j], 256, "%s%d", command[j], pid);
                }
            }
        }
        printf("%s\n", tok);
        tok = strtok(NULL, " ");
        if (!tok) {
            done = 0;
        }
        j++;
    }
}

int main (int argc, char* argv[]) {

    // struct sigaction sa_sigtstp = {0};
    // sa_sigtstp.sa_handler = catchSIGTSTP;
    // sigfillset(&sa_sigtstp.sa_mask);
    // sa_sigtstp.sa_flags = 0;
    // sigaction(SIGTSTP, &sa_sigtstp, NULL);

    

    char* userInput[MAXARGUMENTS];
    int i;
    for (i = 0; i < MAXARGUMENTS; i++) {
        userInput[i] = NULL;
    }
    char inputFile[256] = "";
    char outputFile[256] = "";
    int pid = getpid();
    int background = 0;

    getUserInput(userInput, pid, &background, inputFile, outputFile);
    
    return 0;
}