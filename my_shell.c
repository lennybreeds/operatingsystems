#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define NULL 0

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    //This prints the start line for the user
    printf(">>> ");

    //This resets the buffer to 0's to start from scratch
    memset(buf, 0, nbuf);
    // This gets the users new input
    gets(buf, nbuf);
    //If the buffer is empty we return the function
    if (buf[0] == 0) {
        return -1;
    }

    // If there is a new line then it is replaced with a null terminator as it is easier to do so now for later on in the code
    for (int i = 0; i < nbuf; i++) {
        if (buf[i] == '\n') {
          
            buf[i] = '\0'; 
          // Exit the loop after replacing as we are at the end of the command
            break;  
        }
    }
    // Return 0 when the function is complete
    return 0;  
}


/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *Pfd) {
  



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

            //Copies across the command
            strcpy(splitCommand, &buf[i]);
            break;
        }
        //Check if the redirection is to the left
        if (buf[i] == '<') {
          //End the current arguemnt
            buf[i] = '\0'; 
            //Change the flag to true for redirectionleft for later on in the code
            RedirectionLeft = 1;
            //Skip past the < to find the file
            i++;
            // Skip spaces after `<`
            while (buf[i] == ' ') i++;  
            //Writes the file name after the <
            fileNameL = &buf[i];
            continue;
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
        

        if (buf[i] == ' ' || buf[i] == '\0' || buf[i] == '\n') {
          //Ensure that we aren't adding a command multiple times
          if (ws != i) {
              //End the command in the list
              buf[i] = '\0';
              //Check to see if there is less than 10 commands in the array
              if (nArgs < 10) {
                  args[nArgs++] = &buf[ws];
              }
          }
          //Start the next argument at 1 plus where the last argument finished
          ws = i + 1;
        }
    }
    //Ending the arguments list making sure to add the last argument if there is one and can be added to the arguments list
    if (buf[ws] != '\0' && ws < nbuf  && nArgs < 10) {
        //Add the argument to the end of the arugments array
        args[nArgs++] = &buf[ws];
    }
    args[nArgs] = NULL;


  // handle pipe commands here
  if (IsPipeCommand){
    // create a pipe to pass data between each of the processes
    int Pfd[2]; // holds pipe's read and write ends

    // create the pipe
    if (pipe(Pfd) == -1){
      printf("Pipe wasn't constructed\n");
      //We need to exit the function
      exit(1);
    }

    //Creating a process for the left side of the pipe
    if (fork() == 0){
      //Redirecting end of the pipe to stdout to allow the output to transfer to the right side
      // read end of pipe
      close(Pfd[0]); 
      // closing the stdout
      close(1); 
      // duplicate pipes write end to stdout
      dup(Pfd[1]); 
      // close the write end after duplication.
      close(Pfd[1]); 

      // execute left hand side
      if (exec(args[0], args) == -1){
        printf("Execution has failed\n");
        exit(1);
      }
    } else {

      //This is now the parent process which will handle the right side of the pipe
      // close write end of pipe as only needs to be read now
      close(Pfd[1]);
      // This will now check if splitcommand isn't empty or ended
      if (splitCommand!= 0){
        //Creating a new process for the right side to run a recurisve call
        if (fork() == 0){
          // in the right side command now
          // close stding
          close(0); 
          //Duplicating the read end
          dup(Pfd[0]);
          //Reading the read end
          close(Pfd[0]);

          // recursively run this command
          int length = strlen(splitCommand);
          run_command(splitCommand,length , Pfd);
          //We are freeing up the memory we aren't using anymore
          free(splitCommand);
        }
      }
      //closing the read end pipe in the parent
      close(Pfd[0]);
      //We are waiting for the child processes to finish
      wait(0);

      if (splitCommand!= 0){

        wait(0);

      }
      //Once they have both finished the function will continue
    }

  } else {
    if (fork() == 0) {
      // check redirection
      if (RedirectionRight){
        
        // we open the file correctly
        int file = open(fileNameR, O_WRONLY | O_CREATE | O_TRUNC);
        //Check if it failed to open the file
        if (file < 0){
          //File has not opened properly and we report the error
          printf("Failed to open %s for writing\n", fileNameR);
          //Exit the function
          exit(1);
        }
        // closing the standard output
        close(1);
        //Duplicating 
        dup(file);
        close(file);
        
      }
      if (toRedirectionRight){
        // open the file for writing
        int file = open(fileNameR, O_RDWR);
        if (file >= 0){
          // read to the end of the file to then append
          char buff[1024];
          int n;
          while ((n=read(file, buff, sizeof(buf))) > 0); // keep reading until the end of fole
          // now at the edn write to the file
          close(1);
          dup(file);
          close(file);
        }
      }
      if (RedirectionLeft) {
        // Opening the file for reading only
        int file = open(fileNameL, O_RDONLY);
        //Check if the file has opened properly
        if (file < 0) {

            printf("Failed to open %s for reading\n", fileNameL);
            exit(1);

        }
        
        // Closing the stdin
        close(0);
        //Duplicating the file across
        dup(file);
        // Close the file descriptor as it's no longer needed
        close(file); 


        args[nArgs - 1] = 0;
        
      }

      //Execution of command in the child process
      //Checks if the execution fails
      if (exec(args[0], args) < 0){
        printf("Unknown command: %s\n", args[0]);
        exit(1);
      }
      // If exec fails
    } else {
      // Parent process
      wait(0);
      if (IsSequential){
        run_command(splitCommand, strlen(splitCommand), Pfd);
        free(splitCommand);
      }
      //fprintf(1, "Executing command:arg0 %s, arg1 %s \n",args[0], args[1]);
    }
  }
  
  /* Exit after command execution */
  exit(0);
}



int main(void) {

  //Declared some variables used in the code
  static char buf[1000];
  int pcpmain[2];
  pipe(pcpmain);

  int nArgs = 0;
  int ws = 0;

  while (getcmd(buf, sizeof(buf)) >= 0) {
    // Array to hold command args
    char *args[10];  
    nArgs = 0;
    ws = 0;

    //Parsing the buffer just to check for cd command
    for (int i = 0; i < strlen(buf); i++) {
      //Checking for a space to mark a new argument
        if (buf[i] == ' ' ) {
           // Ensure we aren't capturing empty arguments
            if (ws != i) {  // Ensure we aren't capturing empty args
                buf[i] = '\0';  // Null-terminate the current argument
                if (nArgs < 10) {
                    //Add the argument to the list
                    args[nArgs] = &buf[i + 1];  
                    nArgs++;
                }

            }
            // Update the word start to character after i
            ws = i + 1;  
        }else if (buf[i] == '\0' ||  buf[i] == '\n'){
          // Ensure we aren't capturing empty arguments
          if (ws != i) {  
            // End the current argument with a null
            buf[i] = '\0';  
            //Check if the number of arguments is less than 10
            if (nArgs < 10) {
                // Add the argument to the list
                args[nArgs] = &buf[i + 1]; 

                nArgs++;
            }

          }
          // Update the word start to character after i
            ws = i + 1;  
        }
    }

    // Make sure we properly null-terminate the argument list
    args[nArgs] = 0;



    //To make sure the cd command runs in the parent process we run it here outside of the other function 
    if (nArgs > 0 && strcmp(buf, "cd") == 0) {
        if (nArgs < 1) {
          //There is only one argument that has been passed into the function
            printf("cd: argument is missing\n");
        } else if (chdir(args[0]) != 0) {
            printf("cd: %s: No such directory\n", args[0]);
        }
        continue;  // Skip the rest of the loop to avoid forking for "cd"
    }
      
      // We will create a new child process here
      if (fork() == 0) {
          //The run command will be run in the child process here
          run_command(buf, 1000, pcpmain);
      }

      int child_status;
      // Wait for the child process to complete here
      wait(&child_status);  
  }
  exit(0);
}

