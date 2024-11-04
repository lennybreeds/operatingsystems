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
    /* Parse the current character and set up flags */
    if (buf[i] == '|') {
      pipe_cmd = 1;
      buf[i] = '\0';
      break;
    } else if (buf[i] == '>') {
      redirection_right = 1;
      buf[i] = '\0';
      file_name_r = buf + i + 1;
    } else if (buf[i] == '<') {
      redirection_left = 1;
      buf[i] = '\0';
      file_name_l = buf + i + 1;
    } else if (buf[i] == ';') {
      sequence_cmd = 1;
      buf[i] = '\0';
      break;
    }
  }

  if (!(redirection_left || redirection_right)) {
    // No redirection, continue parsing command
    arguments[numargs++] = buf;
  } else {
    /* Redirection command. Capture the file names */
    while (*file_name_l == ' ') file_name_l++; // Skip spaces
    while (*file_name_r == ' ') file_name_r++; // Skip spaces
  }

  /* Sequence command */
  if (sequence_cmd) {
    sequence_cmd = 0;
    if (fork() != 0) {
      wait(0);
      run_command(buf + i + 1, nbuf - i - 1, pcp);
    }
  }

  /* Handle redirection commands */
  if (redirection_left) {
    int fd = open(file_name_l, O_RDONLY);
    if (fd >= 0) {
      close(0);    // Close stdin
      dup(fd);     // Duplicate fd to stdin (0)
      close(fd);
    }
  }
  if (redirection_right) {
    int fd = open(file_name_r, O_WRONLY | O_CREATE);
    if (fd >= 0) {
      close(1);    // Close stdout
      dup(fd);     // Duplicate fd to stdout (1)
      close(fd);
    }
  }

  /* Execute the command */
  if (strcmp(arguments[0], "cd") == 0) {
    if (chdir(arguments[1]) < 0) {
      printf("cd failed\n");
    }
    exit(2);  // Exit with 2 to indicate 'cd' command
  } else {
    if (pipe_cmd) {
      if (pipe(p) < 0) exit(1);

      if (fork() == 0) {
        close(p[0]);   // Close unused read end
        close(1);      // Close stdout
        dup(p[1]);     // Duplicate p[1] to stdout
        close(p[1]);
        exec(arguments[0], arguments);
        exit(0);
      }

      if (fork() == 0) {
        close(p[1]);   // Close unused write end
        close(0);      // Close stdin
        dup(p[0]);     // Duplicate p[0] to stdin
        close(p[0]);
        run_command(buf + i + 1, nbuf - i - 1, pcp);
      }

      close(p[0]);
      close(p[1]);
      wait(0);
      wait(0);
    } else {
      exec(arguments[0], arguments);
    }
  }
  exit(0);
}

int main(void) {
  static char buf[100];
  int pcp[2];
  pipe(pcp);

  /* Read and run input commands */
  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (fork() == 0)
      run_command(buf, 100, pcp);

    int child_status;
    wait(&child_status);

    /* Check if run_command found this is a CD command */
    if (child_status == 2) {
      // Handle 'cd' in the parent process, if needed
      if (fork() == 0)
        exec("cd", arguments);  // Pass arguments to `cd`
      wait(0);
    }
  }
  exit(0);
}
