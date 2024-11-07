#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// We need to define the NULL value here for the entire file
#define NULL 0

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    printf(">>>");  // Prompt
    memset(buf, 0, nbuf);
    gets(buf, nbuf);

    printf("Input command: %s\n", buf);  // Debug: show user input

    if (buf[0] == 0) {
        return -1;  // Return -1 if the input is empty
    }

    // Remove the newline character if present
    for (int i = 0; i < nbuf; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            break;
        }
    }

    return 0;
}

/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pfd) {
    char *args[15];
    int numArgs = 0;
    int IsPipeCommand = 0;
    int IsSequence = 0;
    int redirectionLeft = 0;
    int redirectionRight = 0;
    int toRedirectionRight = 0;
    char *fileNameLeft = NULL;
    char *fileNameRight = NULL;
    char *splitCommand = NULL;
    int ws = 0;

    printf("run_command called with: %s\n", buf);  // Debug

    for (int i = 0; i < nbuf; i++) {
        if (buf[i] == '|' || buf[i] == ';') {
            if (buf[i] == '|') {
                IsPipeCommand = 1;
            } else {
                IsSequence = 1;
            }
            buf[i] = '\0';
            i++;
            while (buf[i] == ' ') i++;
            int secondCommandLength = strlen(&buf[i]) + 1;
            splitCommand = (char *)malloc(secondCommandLength);
            if (splitCommand == NULL) {
                printf("Memory allocation failed\n");
                exit(1);
            }
            strcpy(splitCommand, &buf[i]);
            printf("Split command: %s\n", splitCommand);  // Debug
            break;
        }

        if (buf[i] == '<') {
            buf[i] = '\0';
            redirectionLeft = 1;
            i++;
            while (buf[i] == ' ') i++;
            fileNameLeft = &buf[i];
            printf("Input redirection to file: %s\n", fileNameLeft);  // Debug
            continue;
        }

        if (buf[i] == '>') {
            buf[i] = '\0';
            if (buf[i + 1] == '>') {
                toRedirectionRight = 1;
                i++;
            } else {
                redirectionRight = 1;
            }
            i++;
            while (buf[i] == ' ') i++;
            fileNameRight = &buf[i];
            printf("Output redirection to file: %s\n", fileNameRight);  // Debug
            continue;
        }

        if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\0') {
            if (ws != i) {
                buf[i] = '\0';
                if (numArgs < 15) {
                    args[numArgs++] = &buf[ws];
                    printf("Arg %d: %s\n", numArgs - 1, args[numArgs - 1]);  // Debug
                }
            }
            ws = i + 1;
        }
    }

    if (numArgs < 15 && ws < nbuf && buf[ws] != '\0') {
        args[numArgs++] = &buf[ws];
        printf("Final Arg %d: %s\n", numArgs - 1, args[numArgs - 1]);  // Debug
    }
    args[numArgs] = NULL;

    if (IsPipeCommand) {
        // Debug pipe execution
        printf("Executing pipe command...\n");
    } else if (IsSequence) {
        // Debug sequence execution
        printf("Executing sequence command...\n");
    } else {
        if (fork() == 0) {
            printf("Executing command: %s\n", args[0]);  // Debug
            exec(args[0], args);
            printf("Execution failed for command: %s\n", args[0]);  // Debug failure
            exit(1);
        } else {
            wait(0);
        }
    }
    exit(0);
}

int main(void) {
    static char buf[100];
    int pcp[2];
    pipe(pcp);

    while (getcmd(buf, sizeof(buf)) >= 0) {
        printf("Main loop command: %s\n", buf);  // Debug
        if (fork() == 0) {
            run_command(buf, 100, pcp);
        }
        wait(0);
    }

    exit(0);
}