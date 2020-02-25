#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

/*Global variables*/
bool isBG = false;
bool allowBG = true;
char inputFileName[256];
char outputFileName[256];
char* arguments[512];
char inputLine[2048];

/*Function declarations*/
void catchCtrlZ ();
void callCD ();
void callStatus (int);
void clearArguments ();
void deleteNewLine ();
void callOtherCommand (struct sigaction, int*);
void getUserInput ();

/*****************************************************************************************************************
 * NAME: main
 * SYNOPSIS: Facilitates the user entering commands and calls various functions to excecute them
 * DESCRIPTION: Sets signal handlers for C + Z, loops getting user input and calls commands until exit is entered
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
int main (int argc, char* argv[]) {
    /*Initialize variables*/
    int exitStatus = 0;
    int i = 0;
    for (i = 0; i < 512; i++) {
        arguments[i] = NULL;
    }

    /*Handle ctrl C*/
    struct sigaction ctrlC = {0};
    ctrlC.sa_handler = SIG_IGN;
    sigfillset(&ctrlC.sa_mask);
    ctrlC.sa_flags = 0;
    sigaction(SIGINT, &ctrlC, NULL);

    /*Handle and redirect ctrl Z*/
    struct sigaction ctrlZ = {0};
    ctrlZ.sa_handler = catchCtrlZ;
    sigfillset(&ctrlZ.sa_mask);
    ctrlZ.sa_flags = 0;
    sigaction(SIGTSTP, &ctrlZ, NULL);

    while (true) { //Loops until exit is called
        getUserInput(); //Gets user input and tokenizes the string into different arguments

        if(!strcmp(arguments[0], "status")) { //Triggers if status command is entered
            callStatus(exitStatus);
            fflush(stdout);

        } else if (!strcmp(arguments[0], "cd")) { //Triggers if cd command is entered
            callCD();

        } else if (!strcmp(arguments[0], "exit")) { //Breaks from the while loop if exit command is entered
            break;

        } else if (arguments[0][0] == '\0' || arguments[0][0] == '#') { //Continues to ignore comments or blank entries
            continue;

        } else {
            callOtherCommand(ctrlC, &exitStatus); //Handles any other command entered
        }

        clearArguments(); //Resets variables to original states
    }

    return 0;
}

/*****************************************************************************************************************
 * NAME: catchCtrlZ
 * SYNOPSIS: The function that catches the signal thrown when ctrl Z is entered into the command line
 * DESCRIPTION: Toggles allow background function to true or false based on its previous value
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void catchCtrlZ () {
    if (allowBG) { //If background processes are currently allowed
        write(1, "Entering foreground-only mode (& is now ignored).\n", 50);
        fflush(stdout);
        allowBG = false; //Sets allow background to false for future use
    } else { //If background processes aren't allowed
        write(1, "Exiting foreground-only mode.\n", 30);
        fflush(stdout);
        allowBG = true; //Sets allow background to true for future use
    }
}

/*****************************************************************************************************************
 * NAME: callCD
 * SYNOPSIS: Implements the cd command 
 * DESCRIPTION: Opens the directory (if one is given) and throws an error if it doesn't exist, otherwise defaults
 * to the home directory
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void callCD () {
    if (arguments[1]) { //Corresponds to the name of the directory (if one is given)
        if (chdir(arguments[1]) == -1) { //Checks to see if the given directory exists
            printf("Directory not found.\n");
            fflush(stdout);
        }
    } else {
        chdir(getenv("HOME")); //Defaults to home directory if one isn't given
    }
}

/*****************************************************************************************************************
 * NAME: callStatus
 * SYNOPSIS: Implements the status command
 * DESCRIPTION: Checks if the process was exited or terminated and returns the appropriate value based on the exit
 * status variable
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void callStatus (int exitStatus) {
    if (WIFEXITED(exitStatus)) { //Checks to see if it was exited or terminated
		printf("Exit value %d\n", WEXITSTATUS(exitStatus)); //Returns exit value
	} else {
		printf("Terminated by signal %d\n", WTERMSIG(exitStatus)); //Returns termination value
	}
}

/*****************************************************************************************************************
 * NAME: clearArguments
 * SYNOPSIS: Resets the variables that are used in every loop in main
 * DESCRIPTION: Sets the arguments 2D array back to null, sets is background to false, and points input and output
 * file names back to null
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void clearArguments () {
    int i = 0;
    for (i = 0; arguments[i]; i++) { //Points the array of strings that holds each argument back to NULL
        arguments[i] = NULL;
    }
    isBG = false; //Reset background to false
    inputFileName[0] = '\0'; //Reset input file name to NULL
    outputFileName[0] = '\0'; //Reset output file name to NULL
}

/*****************************************************************************************************************
 * NAME: deleteNewLine
 * SYNOPSIS: Deletes the first newline found in the user input line
 * DESCRIPTION: Loops through line of user input, finds the first character equal to newline and sets it to null
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void deleteNewLine () {
    int i = 0;
    int isNewLine = false;
    for (i = 0; i < 2048 && !isNewLine; i++) { //Loops through the user input and deletes the first newline found
        if (inputLine[i] == '\n') {
            inputLine[i] = '\0'; //Replace newline with NULL terminator
            isNewLine = true;
        }
    }
}

/*****************************************************************************************************************
 * NAME: callOtherCommand   
 * SYNOPSIS: Calls one of the not built-in commands
 * DESCRIPTION: Forks off a child process and executes it, also waits for children to be destroyed and cleans up
 * zombie children
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void callOtherCommand (struct sigaction sigAct, int* exitStatus) {
    pid_t childPid = -99; //Set the child's pid to something random
    int final, inputFile, outputFile = 0; //Initialize values

    childPid = fork(); //Fork it!
    switch (childPid) {
    case -1: //If there was an error forking, return exit status 1
        perror("Forking error.\n");
        exit(1);
        break;
    case 0: //Child process
        sigAct.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sigAct, NULL); //Set ^C signal as default

        if (strcmp(inputFileName, "")) { //If an input file is needed
            inputFile = open(inputFileName, O_RDONLY); //Open the file as read only
            if (inputFile == -1) { //Return error if there's an issue opening the file
                perror("Error opening file.\n");
                exit(1);
            }
            final = dup2(inputFile, 0); //Assign the file
            if (final == -1) {
                perror("Error assigning input file.\n");
                exit(2);
            }
            fcntl(inputFile, F_SETFD, FD_CLOEXEC); //Close the file
        }

        if (strcmp(outputFileName, "")) { //If an output file is needed
            outputFile = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666); //Open the file as write only
            if (outputFile == -1) { //Return error is there's an issue opening the file
                perror("Error opening file.\n");
                exit(1);
            }
            final = dup2(outputFile, 1); //Assign the file
            if (final == -1) {
                perror("Error assigning input file.\n");
                exit(2);
            }
            fcntl(outputFile, F_SETFD, FD_CLOEXEC); //Close the file
        }

        if (execvp(arguments[0], (char* const*)arguments)) { //Execute the command 
            printf("%s: No such file or directory.\n", arguments[0]);
            fflush(stdout);
            exit(2);
        }
        break;
    default:
        if (isBG && allowBG) { //Only runs if is background and allow background are both true
            pid_t pid = waitpid(childPid, exitStatus, WNOHANG); //Child's actual pid
            printf("Background pid is: %d\n", childPid);
            fflush(stdout);
        } else {
            pid_t pid = waitpid(childPid, exitStatus, 0); //Wait for the child
        }

        while ((childPid = waitpid(-1, exitStatus, WNOHANG)) > 0) { //Terminate any zombie kiddos
            printf("Child %d terminated.\n", childPid);
            callStatus(*exitStatus);
            fflush(stdout);
        }
    }
}

/*****************************************************************************************************************
 * NAME: getUserInput
 * SYNOPSIS: Gets user input and tokenizes the string into individual arguments
 * DESCRIPTION: Gets user input as one long string, tokenizes it based on spaces, loops through and matches each
 * individual argument to a certain character (>, <, &, etc.)
 * AUTHOR: Jett Seale (sealee@oregonstate.edu)
 * **************************************************************************************************************/
void getUserInput () {
    /*Initialize values*/
    int i, j = 0;
    bool notDone = true;
    char* argument = NULL;

    printf(": ");
    fflush(stdout);
    fgets(inputLine, 2048, stdin); //Gets user input
    deleteNewLine(); //Gets rid of the last newline at the end of the input

    if (!strcmp(inputLine, "")) { //Just return if the input is empty
        arguments[0] = strdup("");
        return;
    }

    argument = strtok(inputLine, " "); //Tokenize the line into the different arguments

    for (i = 0; argument; i++) { //Loop through the arguments until you hit NULL

        if(!strcmp(argument, ">")) { //Triggers if a > character is found, copies the next argument to output file name for later use
            argument = strtok(NULL, " ");
            strcpy(outputFileName, argument);

        } else if (!strcmp(argument, "<")) { //Triggers if a < character is found, copies the next argument to input file name for later use
            argument = strtok(NULL, " ");
            strcpy(inputFileName, argument);

        } else if (!strcmp(argument, "&")) { //Triggers if & character is found, sets the is background variable to true for later use
            isBG = true;

        } else { //Otherwise, the argument is an actual command
            arguments[i] = strdup(argument); //Duplicate the arguments up until that point 

            for (j = 0; arguments[i][j]; j++) { //Loops through the arguments to find any $$
                if (arguments[i][j] == '$' && arguments[i][j + 1] == '$') {
                    arguments[i][j] = '\0';
                    snprintf(arguments[i], 256, "%s%d", arguments[i], getpid()); //If it finds one, replace with the pid
                }
            }
        }
        argument = strtok(NULL, " "); //Continue with the next argument
    }
}