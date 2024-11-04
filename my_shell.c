#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
  printf(">>> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if (buf[0] == 0) // EOF
    return -1;
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
  int numargs = 0;
  /* Word start/end */
  int ws = 1;
  int we = 0;

  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;

  int p[2];
  int pipe_cmd = 0;

  int sequence_cmd = 0;

  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf; i++) {
    /* Parse the current character and set-up various flags:
       sequence_cmd, redirection, pipe_cmd and similar. */

    if (buf[i] == '>') {
      redirection_right = 1;
      file_name_r = &buf[i + 1];
    } else if (buf[i] == '<') {
      redirection_left = 1;
      file_name_l = &buf[i + 1];
    } else if (buf[i] == '|') {
      pipe_cmd = 1;
      buf[i] = 0;
    } else if (buf[i] == ';') {
      sequence_cmd = 1;
      buf[i] = 0;
    } else if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == 0) {
      if (we > ws) {
        arguments[numargs++] = &buf[ws];
        buf[we] = 0;
      }
      ws = we + 1;
    } else {
      we = i;
    }
  }
  arguments[numargs] = 0;

  if (sequence_cmd) {
    sequence_cmd = 0;
    if (fork() != 0) {
      wait(0);
      run_command(&buf[i + 1], nbuf - i - 1, pcp);
    }
  }

  if (redirection_left) {
  int fd = open(file_name_l, O_RDONLY);
  if (fd >= 0) {
    close(0); // Close stdin
    dup(fd);  // Duplicates fd to stdin (file descriptor 0)
    close(fd);
  }
}

if (redirection_right) {
  int fd = open(file_name_r, O_WRONLY | O_CREATE);
  if (fd >= 0) {
    close(1); // Close stdout
    dup(fd);  // Duplicates fd to stdout (file descriptor 1)
    close(fd);
  }
}

  if (strcmp(arguments[0], "cd") == 0) {
    if (chdir(arguments[1]) < 0)
      printf("cd: cannot change directory to %s\n", arguments[1]);
    exit(2);
  } else {
    if (pipe_cmd) {
      pipe(p);
      if (fork() == 0) {
        close(p[0]);
        dup(p[1], 1); // Redirect stdout to pipe
        exec(arguments[0], arguments);
        exit(0);
      } else {
        close(p[1]);
        dup(p[0], 0); // Redirect stdin from pipe
        run_command(&buf[i + 1], nbuf - i - 1, pcp);
      }
    } else {
      if (fork() == 0) {
        exec(arguments[0], arguments);
        printf("exec: command not found: %s\n", arguments[0]);
        exit(0);
      }
    }
  }
  exit(0);
}

int main(void) {

  static char buf[100];

  int pcp[2];
  pipe(pcp);

  /* Read and run input commands. */
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(fork() == 0)
      run_command(buf, 100, pcp);

    /*
      Check if run_command found this is
      a CD command and run it if required.
    */
    int child_status;
    wait(&child_status);
    if (child_status == 2) {
      char dir[100];
      read(pcp[0], dir, sizeof(dir));
      chdir(dir);
    }
  }
  exit(0);
}
