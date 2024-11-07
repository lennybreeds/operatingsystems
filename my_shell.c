#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define NULL 0

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    // Print >>> then clear buffer
    printf(">>> ");
    memset(buf, 0, nbuf);

    // Read the user's input into buf
    gets(buf, nbuf);
    if (buf[0] == 0) {
        return -1;  // Return -1 if input is empty
    }

    // Remove the newline character if present
    for (int i = 0; i < nbuf; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';  // Replace \n with null terminator
            break;  // Exit the loop after replacing
        }
    }

    return 0;  // Return 0 on success
}


/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pipefd) {
  
  // check for a ; and make a new string after the ;
  // then can recursively call this function with that string

  //printf("%s command 2\n", commands[1]);
  /* Useful data structures and flags. */
    char *args[10];
    int nArgs = 0;
    int IsPipeCommand = 0;
    int IsSequential = 0;
    int RedirectionLeft = 0;
    int RedirectionRight = 0;
    int toRedirectionRight = 0;
    char *fileNameL = NULL;
    char *fileNameR = NULL;
    char *splitCommand= NULL;
    int ws = 0;
    

    for (int i = 0; i < nbuf; i++) {
      //First parsing our pipes and sequence
        if (buf[i] == ';' || buf[i] == '|' ) {
          //Checking whether the the character is a pipe or semicolon
            if (buf[i] == ';') {
                IsSequential = 1;
            } else {
                IsPipeCommand = 1;
            }
            //Ends the first command with a null pointer
            buf[i] = '\0';  
            //This then skips the delimiter
            i++;  
            //We need to skip all the spaces inetween the next command
            while (buf[i] == ' ') i++; 

            // Store the remaining command
            int secondCommandlen = strlen(&buf[i]) + 1;
            //This then creates space in memory for us to copy over the second half of the command
            //This uses safe memory allocation
            splitCommand= (char *)malloc(secondCommandlen);


            if (splitCommand== NULL) {
                printf("Allocation in memory bas failed\n");
                exit(1);
            }

            
            strcpy(splitCommand, &buf[i]);
            break;
        }

        if (buf[i] == '>') {
          //This tests whether it is a redirection into a file
          // End the argument off
            buf[i] = '\0';  
            //We check to see if it is a redirection with two > as it behaves differently
            if (buf[i + 1] == '>') { 

                toRedirectionRight = 1;
                //We need to go past the second > for it to skip the >>
                i++;
            } else {

                RedirectionRight = 1;
            }

            i++;
            // We need to skip the spaces
            while (buf[i] == ' ') i++;  
            //This is the file name on the right side
            fileNameR = &buf[i];
            continue;
        }

        if (buf[i] == '<') {
            buf[i] = '\0';  // End current argument
            RedirectionLeft = 1;
            i++;
            while (buf[i] == ' ') i++;  // Skip spaces after `<`
            fileNameL = &buf[i];
            continue;
        }

        if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\0') {
            if (ws != i) {
                buf[i] = '\0';
                if (nArgs < 10) {
                    args[nArgs++] = &buf[ws];
                }
            }
            ws = i + 1;
        }
    }

    if (ws < nbuf && buf[ws] != '\0' && nArgs < 10) {
        args[nArgs++] = &buf[ws];
    }
    args[nArgs] = NULL;

  /* Handle sequence commands (e.g., commands separated by ';') */


  // handle pipe commands here
  if (IsPipeCommand){
    // create a pipe to pass data between
    int pipefd[2]; // holds pipe's read and write ends

    // create the pipe
    if (pipe(pipefd) == -1){
      printf("Pipe failed\n");
      exit(1);
    }

    //printf("Executing command: %s\n", args[0]);

    // fork for the left hand side of the command
    if (fork() == 0){
      // redirect the stdout to the write end of the pipe
      close(pipefd[0]); // read end of pipe
      close(1); // close stdout
      dup(pipefd[1]); // duplicate pipes write end to stdout
      close(pipefd[1]); // close the write end after duplication.

      // execute left hand side
      if (exec(args[0], args) == -1){
        printf("Exec failed\n");
        exit(1);
      }
    } else {
      // in parent process after forking left command

      // close write end of pipe as only needs to be read now
      close(pipefd[1]);
      if (splitCommand!= 0){
        if (fork() == 0){
          // in the right side command now
          close(0); // close stding
          dup(pipefd[0]);
          close(pipefd[0]);

          // recursively run this command
          run_command(splitCommand, strlen(splitCommand), pipefd);
          free(splitCommand);
        }
      }
      // close read end of pipe in parent
      close(pipefd[0]);

      wait(0);
      if (splitCommand!= 0){
        wait(0);
      }
    }



  }

  /* If this is a CD command, handle it separately */
  else if (strcmp(args[0], "cd") == 0) {
    if (nArgs < 2) {
      printf("cd: missing argument\n");
    } else if (chdir(args[1]) != 0) {
      printf("cd: %s: No such directory\n", args[1]);
    }
  } else {
    /* For other commands, fork a child process to execute */
    if (fork() == 0) {
      // check redirection
      if (RedirectionRight){
        
        // open the file
        int fd = open(fileNameR, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd < 0){
          printf("Failed to open %s for writing\n", fileNameR);
          exit(1);
        }
        // close the standard output, then duplicate output to file
        close(1);
        dup(fd);
        close(fd);
        
      }
      if (toRedirectionRight){
        // open the file for writing
        int fd = open(fileNameR, O_RDWR);
        if (fd >= 0){
          // read to the end of the file to then append
          char buffer[1024];
          int n;
          while ((n=read(fd, buffer, sizeof(buf))) > 0); // keep reading until the end of fole
          // now at the edn write to the file
          close(1);
          dup(fd);
          close(fd);
        }
      }
      if (RedirectionLeft) {
        // Open the file for reading
        //printf("%s\n",fileNameL);
        int fd = open(fileNameL, O_RDONLY);
        if (fd < 0) {
            printf("Failed to open %s for reading\n", fileNameL);
            exit(1);
        }
        
        // Close stdin and redirect it to the file
        close(0); // Close the original stdin
        
        dup(fd);
          // Duplicate fd onto stdin (0)
        close(fd); // Close the file descriptor as it's no longer needed
        // need to work out how many args before '<'
        args[nArgs - 1] = 0;


        
    }

      // Child process
      //
      if (exec(args[0], args) < 0);
      // If exec fails
      printf("Unknown command: %s\n", args[0]);
      exit(1);
    } else {
      // Parent process
      wait(0);
      if (IsSequential){
        run_command(splitCommand, strlen(splitCommand), pipefd);
        free(splitCommand);
      }
      //fprintf(1, "Executing command:arg0 %s, arg1 %s \n",args[0], args[1]);
    }
  }
  
  /* Exit after command execution */
  exit(0);
}



int main(void) {
    static char buf[100];
    int pcp[2];
    pipe(pcp);

    // Read and run input commands
    // this main function has been made just to ensure cd works, as it needs to be executed outside fork
    
    while (getcmd(buf, sizeof(buf)) >= 0) {
        char *args[10];  // Array to hold command args
        int nArgs = 0;
        int ws = 0;

        // Loop through the input buffer to extract args and run
        for (int i = 0; i < strlen(buf); i++) {
            if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\0') {
                if (ws != i) {  // Ensure we aren't capturing empty args
                    buf[i] = '\0';  // Null-terminate the current argument
                    if (nArgs < 10) {
                        
                        args[nArgs] = &buf[i + 1];  // Add the argument to the list
                        nArgs++;
                    }

                }
                ws = i + 1;  // Update the word start to the next character
            }
        }

        // Make sure we properly null-terminate the argument list
        args[nArgs] = 0;



        // Handle the "cd" command in the parent process
        if (nArgs > 0 && strcmp(buf, "cd") == 0) {
            if (nArgs < 1) {
                printf("cd: missing argument\n");
            } else if (chdir(args[0]) != 0) {
                printf("cd: %s: No such directory\n", args[0]);
            }
            continue;  // Skip the rest of the loop to avoid forking for "cd"
        }
        
        // For all other commands, fork a new process
        if (fork() == 0) {
            // In the child process, execute the command
            run_command(buf, 100, pcp);
        }

        int child_status;
        wait(&child_status);  // Wait for the child process to complete
    }

    exit(0);
}

