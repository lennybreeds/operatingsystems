#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


//We need to define the NULL value here for the entire file
#define NULL 0

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    // We will print the prompt that the users will see and are clearing the buffer
    printf(">>>");

    memset(buf, 0, nbuf);

    // Read the user's input into buf
    gets(buf, nbuf);
    if (buf[0] == 0) {

        return -1;  // We return -1 if the input is empty

    }

    // This code is then needed to remove the newline character if present
    for (int i = 0; i < nbuf; i++) {
        if (buf[i] == '\n') {

          buf[i] = '\0';  // Replace \n with null terminator to end the string
          
          break;  // Exit the loop after replacing
        }
    }

    return 0;  // Return 0 if it is successful
}


/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pfd) {
  

  /* Useful data structures and flags. */
    char *args[15];
    int numArgs = 0;

    //These flags determine the future logic of the function
    int IsPipeCommand = 0;
    int IsSequence = 0;

    //These flags determine if we need to redirect the output or input and if we need to append
    int redirectionLeft = 0;
    int redirectionRight = 0;
    int toRedirectionRight = 0;

    //These are the file names that we will be using for redirection
    char *fileNameLeft = NULL;
    char *fileNameRight = NULL;
    char *splitCommand = NULL;


    int ws = 0;
    
    //This loop will parse the command character by character comparing to the delimiters and setting flags
    //The loop will stop when it reaches the end of the buffer or the end of the command
    for (int i = 0; i < nbuf; i++) {
      //Our first if statement will check for ; and | and set the flags accordingly
      if (buf[i] == ';' || buf[i] == '|' ) {
        if (buf[i] == ';') {
            IsSequence = 1;
        } else {
            IsPipeCommand = 1;
        }
        buf[i] = '\0';  // End the first command 
        i++;  // Skip delimiter
        while (buf[i] == ' ') i++;  // Skip spaces

        // We create a new string in memory to store the remaining command


        int splitCommandLength = strlen(&buf[i]) + 1;
        splitCommand = (char *)malloc(splitCommandLength);
        
        if (splitCommand == NULL) {
            printf("Allocation for second command has failed\n");
            exit(1);
        }
        // Copy the remaining command into splitCommand
        strcpy(splitCommand, &buf[i]);
        //We need to break the loop to avoid parsing the rest of the command as it will be done in the next recursive call
        break;
      }

      if (buf[i] == '>') {
          buf[i] = '\0';  // End current argument
          if (buf[i + 1] == '>') {  // Check for `>>`
            if (buf[i + 2] == ' ') {
                toRedirectionRight = 1;
             } else {
                printf("Invalid syntax\n");
                exit(1);
              }
              i++;
          } else {
              redirectionRight = 1;
          }
          i++;
          //This will skip all the spaces after the > or >>
          while (buf[i] == ' ') i++;
          //This sets the pointer to the file name
          fileNameRight = &buf[i];
          //This will skip all the characters until the next space
          continue;
      }
      //We need to then check for spaces and newlines to determine the end of the argument and add it to the args array
      if ((buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\0' ) && ws != i) {
        // Null-terminate the current argument
        buf[i] = '\0';
        // Add the argument to the list if there is space
        if (numArgs < 15) {
            args[numArgs++] = &buf[ws];
        }
        // Update the word start to the next character after the space to be used later in the loop
        ws = i + 1;
      }
    }
    //Making sure that the last argument is added to the args array
    if (numArgs < 15 && ws < nbuf && buf[ws] != '\0') {
      args[numArgs++] = &buf[ws];
    }
    args[numArgs] = NULL;


  //We first have to check if the command is a pipe command
  if (IsPipeCommand){
    // create a pipe to pass data between
    int pfd[2]; // holds pipe's read and write ends
    // create the pipe
    if (pipe(pfd) == -1){
      printf("Pipe not constructed\n");

      exit(1);
    }


    //We need to create a new process for the left side of the pipe
    if (fork() == 0){
      // redirect the stdout to the write end of the pipe
      close(pfd[0]); // read the edn of pipe
      close(1); // close stdout
      dup(pfd[1]); //this then duplicates the write end of the pipe to stdout
      // We then close the write end of the pipe
      close(pfd[1]); 


      //We then need to use exec to execute the command
      if (exec(args[0], args) == -1){
        printf("Execution has failed\n");
        exit(1);
      }
    } else {
      // This is the parent process and the previous was the child process, we first close the write end of the pipe
      close(pfd[1]);
      //We now check if there is a second command on the right side of the pipe
      if (splitCommand != 0){
        // We create a new process here for the recursive part of this function
        if (fork() == 0){
          // in the right side command now

          close(0); // close stding
          dup(pfd[0]);
          close(pfd[0]);

          // recursively run this command
          run_command(splitCommand, strlen(splitCommand), pfd);
          //We free the memory that is allocated after the function is complete for good memory management
          free(splitCommand);
        }
      }
      // close read end of pipe in parent
      close(pfd[0]);


      //We then wait for the child process to finish 
      wait(0);
      //We then check if there is a second command on the right side of the pipe
      if (splitCommand != 0){
        wait(0);
      }
    }
  
  } else {
    if (fork() == 0) {
      // check redirection of >>
      if (toRedirectionRight){
        // open the file for writing
        int fileName_right = open(fileNameRight, O_RDWR);
        //check if we have opened the file successfully
        if (fileName_right >= 0){
          // read to the end of the file to then append to the buffer
          char buffer[2048];
          int n;
          while ((n=read(fileName_right, buffer, sizeof(buf))) > 0); // keep reading until the end of fole
          //we now need to write to the file
          close(1);
          // close the standard output, then duplicate output to file
          dup(fileName_right);

          //We need to close the file descriptor as it is no longer needed
          close(fileName_right);
        }
      }
      // check redirection of >
      if (redirectionRight){
        
        // open the file
        int fileName_right = open(fileNameRight, O_WRONLY | O_CREATE | O_TRUNC);

        // check if we have opened the file successfully

        if (fileName_right < 0){
          printf("File %s has failed to open\n", fileNameRight);
          exit(1);
        }

        // close the standard output, then duplicate output to file
        close(1);

        dup(fileName_right);

        //We need to close the file descriptor as it is no longer needed
        close(fileName_right);
        
      }

      // check redirection of <
      if (redirectionLeft) {
        // Open the file for reading
        int fileName_left = open(fileNameLeft, O_RDONLY);

        if (fileName_left < 0) {
            printf("File %s has failed to open \n", fileNameLeft);
            exit(1);
        }
        
        // Close stdin and redirect it to the file
        close(0); // Close the original stdin
        
        dup(fileName_left);
          // Duplicate fileName_left onto stdin (0)
        // Close the file descriptor as it's no longer needed
        close(fileName_left);
        // this is to ensure that the args are null terminated
        args[numArgs - 1] = 0;

      }

      //This is the process that will execute the command
      if (exec(args[0], args) < 0){
        //If the exec fails we print an error message
        printf("Unknown command: %s\n", args[0]);
        exit(1);
      }
      
    } else {
      // This is the parent process and we wait for the child process to finish
      wait(0);
      //We then check if there is a second command on the right side of the ;
      if (IsSequence){
        //We create a new process here for the recursive part of this function
        int length = strlen(splitCommand);
        run_command(splitCommand, length, pfd);
        //We free the memory that is allocated after the function is complete for good memory management
        free(splitCommand);
      }
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
    // This code repeats the process of reading the command to ensure cd runs on the parent process 
    //The loop will continue until the user exits the shell
    while (getcmd(buf, sizeof(buf)) >= 0) {
       // Array to hold command args
        char *args[15]; 
        int numArgs = 0;
        int ws = 0;

        // Loop through the input buffer to extract args and run
        for (int i = 0; i < strlen(buf); i++) {
            if (buf[i] == ' ' || buf[i] == '\0' || buf[i] == '\n') {
                if (ws != i) {  // Ensure we aren't capturing empty args
                    buf[i] = '\0';  // Null-terminate the current argument
                    if (numArgs < 15) {


                        args[numArgs] = &buf[i + 1];  // Add the argument to the list

                        numArgs++;
                    }

                }
                // Update the word start to the next character
                ws = i + 1;  
            }
        }


        // Make sure we properly null-terminate the argument list
        args[numArgs] = 0;



        //This code checks to see if the command is cd and runs it in the parent process
        if (numArgs > 0 && strcmp(buf, "cd") == 0) {
            if (numArgs < 1) {

                printf("cd: need an argument\n");

              //If the argument is not valid we print an error messagehh
            } else if (chdir(args[0]) != 0) {

                printf("cd: %s: No such directory\n", args[0]);
            }
            continue;  // Skip the rest of the loop to avoid forking for "cd"
        }
        
        //For the rest we now fork a new process
        if (fork() == 0) {
            // In the child process, execute the command
            run_command(buf, 100, pcp);
        }

        int child_status;
        // Wait for the child process to complete
        wait(&child_status);
    }

    exit(0);
}
