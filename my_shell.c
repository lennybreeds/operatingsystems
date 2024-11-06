#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

//Declare null
#define NULL 0

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
  //Printing the >>> and then clearing the buffer
  printf(">>> ");
  memset(buf, 0, nbuf);
  // Get the input from the user and put it into the buffer
    gets(buf, nbuf);
  //If the input is empty it will return -1
  if (buf[0] == 0) {
      return -1;
  }
  //To make the rest of the code work we need to remove any newlines
  for (int i = 0; i < nbuf; i++) {
    if (buf[i] == '\n') {
        buf[i] = '\0'; //This replaces the newline with a null terminator
        break;//This exits the loop
    }
  }
  return 0;
}

/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {

  /* Useful data structures and flags. */
  char *arguments[10];
  int num_args = 0;
  /* Word start/end */
  int ws = 0;

  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = NULL;
  char *file_name_r = NULL;

  int pipe_cmd = 0;
  int sequence_cmd = 0;
  int sequential = 0;
  char *second_command = 0;
  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf; i++) {
    while (buf[i] == ' ') i++; //Gets rid of any spaces at the beginning of the command
    if (buf[i] == '\0') break; //If the command is empty it will break
    if (buf[i] == '>') {
      buf[i] = '\0';  // End current argument
      if (buf[i + 1] == '>') {  // Check for `>>`
          two_redirection_right = 1;
          i++;
      } else {
          redirection_right = 1;
      }
      i++;
      while (buf[i] == ' ') i++;  // Skip spaces after `>`
      file_name_r = &buf[i];
      continue;
    }

    if (buf[i] == '<') {
      buf[i] = '\0';  // End current argument
      redirection_left = 1;
      i++;
      while (buf[i] == ' ') i++;  // Skip spaces after `<`
      file_name_l = &buf[i];
      continue;
    }
    if (buf[i] == '|' || buf[i] == ';') {
      if (buf[i] == '|') {
          pipe_cmd = 1;
      } else {
          sequential = 1;
      }
      buf[i] = '\0';  // End the first command
      i++;  // Skip delimiter
      while (buf[i] == ' ') i++;  // Skip spaces

      // Store the remaining command
      int second_command_length = strlen(&buf[i]) + 1;
      second_command = (char *)malloc(second_command_length);
      if (second_command == NULL) {
          printf("Memory allocation failed\n");
          exit(1);
      }
      strcpy(second_command, &buf[i]);
      break;
    }

    if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\0') {
      if (ws != i) {
        buf[i] = '\0';
        if (num_args < 10) {
            arguments[num_args++] = &buf[ws];
        }
      }
      ws = i + 1;
    }
  }
  if (pipe_cmd){
    int pfd[2] //Holds the pipe data
    //Create the pipe that will be used
    if (pipe(pfd) == -1){
      printf("Pipe has not been constructed\n");
      exit(1);
    }


    if (fork==0){
      //We need to redirect the stdout to the write end of this pipe
      close(pfd[0]); //Close the read end of the pipe
      close(1); //Close the stdout
      dup(pfd[1]); //Duplicate the write end of the pipe to stdout
      close(pfd[1]); //Close the write end of the pipe after duplication

      //Execute what is on the left side of the pipe
      if (exec(arguments[0], arguments) == -1){
        printf("Execution failed\n");
        exit(1);
      }
    } else {
      //This is the parent process rather than the child process
      close(pfd[1]); //Close the write end of the pipe as it is only needed for reading
      if (second_command != 0){
        if (fork() == 0){
          //This is the right side of the command
          close(0); //Close the stdin
          dup(pfd[0]); //Duplicate the read end of the pipe to stdin
          close(pfd[0]); //Close the read end of the pipe
          run_command(second_command, strlen(second_command), pfd);
          free(second_command);
        }
      }

      //close the read end of the pipe that is being run in the parent process
      close(pfd[0]);
      wait(0);
      if (second_command != 0){
        wait(0);
      }
    }
  } else if (strcmp(arguments[0], "cd") == 0) {
    if (num_args < 2) {
      printf("cd: missing argument\n");
    } else if (chdir(arguments[1]) != 0) {
      printf("cd: %s: No such directory\n", arguments[1]);
    }
  } else {
    /* For other commands, fork a child process to execute */
    if (fork() == 0) {
      // check redirection
      if (redirection_right){
        
        // open the file
        int fd = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd < 0){
          printf("Failed to open %s for writing\n", file_name_r);
          exit(1);
        }
        // close the standard output, then duplicate output to file
        close(1);
        dup(fd);
        close(fd);
        
      }(two_redirection_right){){
        // open the file for writing
        int fd = open(file_name_r, O_RDWR);
        if (fd >= 0){
          // read to the end of the file to then append
          char buffer[1024];
          int n;
          while ((n=read(fd, buffer, sizeof(buf))) > 0); // keep reading until the end of fole
          // now at the end write to the file
          close(1);
          dup(fd);
          close(fd);
        }
      }
      if (redirection_left) {
        // Open the file for reading
        //printf("%s\n",file_name_l);
        int fd = open(file_name_l, O_RDONLY);
        if (fd < 0) {
            printf("Failed to open %s for reading\n", file_name_l);
            exit(1);
        }
        
        // Close stdin and redirect it to the file
        close(0); // Close the original stdin
        
        dup(fd);
          // Duplicate fd onto stdin (0)
        close(fd); // Close the file descriptor as it's no longer needed
        // need to work out how many args before '<'
        arguments[num_args - 1] = 0;
      }

      // Child process
      //
      if (exec(arguments[0], arguments) < 0){
        // If exec fails
        printf("Unknown command: %s\n", arguments[0]);
        exit(1);
      }
    } else {
      // Parent process
      wait(0);
      if (sequential){
        run_command(second_command, strlen(second_command), pfd);
        free(second_command);
      }
      //fprintf(1, "Executing command:arg0 %s, arg1 %s \n",arguments[0], arguments[1]);
    }
  }
  }
  /* Exit after command execution */
  exit(0);
}

int main(void) {

  static char buf[100];
  int pcp[2];
  if(pipe(pcp)  < 0){
    printf("Pipe failed\n");
    exit(1);
  }

  /* Read and run input commands. */
  while(getcmd(buf, sizeof(buf)) >= 0){
    run_command(buf,100, pcp);
    int child_status;
    wait(&child_status);
  }


  exit(0);
}