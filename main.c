#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void sigHandler(int sig) { printf("Hey! Caught signla %d\n", sig); }

int main_sig() {
  /*Entry point for signal handling.*/
  if (signal(SIGINT, sigHandler) == SIG_ERR) {
    printf("ERROR! hej\n");
    exit(1);
  }

  while (1) {
    printf("....");
    sleep(1);
    printf("\n");
  }
}

// TODO: implement memory deallocation
/* Loads the the contents of the agrs array with the command line given
    by the user.
  */
int getcmd(char *prompt, char *args[], int *background) {
  
  int length, i = 0;
  char *token, *loc;
  char *line = NULL;
  size_t linecap = 0;

  // display prompt mark & rea user input
  printf("%s", prompt);
  length = getline(&line, &linecap, stdin);

  // check if any actual argument was passed in this command line
  if (length <= 0) {
    exit(-1);
  }

  // check if '&' was provided in input and it in input str by whitespace
  // TODO: check if & was well specified at the end of the line (trailing)
  if ((loc = index(line, '&')) != NULL) {
    *background = 1;
    *loc = ' ';
  } else {
    *background = 0;
  }

  // parse line to retrieve each token delimited by ' ', \t or \n
  while ((token = strsep(&line, " \t\n")) != NULL) {
    // filter for only ASCII char values of 32 and above
    for (int j = 0; j < strlen(token); j++) {
      if (token[j] <= 32) {
        token[j] = '\0';
      }
    }
    // if token exists - add it to the arguments 
    if (strlen(token) > 0) {
      args[i++] = token;
    }
  }

  //TODO: explain fix for execvp to work 
  args[i++] = NULL;

  return i;
}

// TODO: implement exit: necessary to quit the shell
// TODO: implement pwd: getcwd library routine
// TODO: implement cd: chdir system call 
// TODO: implement fg: called with a number and pick specified pid and put it in foreground
// TODO: implement jobs: list all jobs running in the background at any given time (like ps)

int getcmdstatus(char *cmd, int *builtincmd_idx, int *builtincmds_count, char *builtincmds[]) {
    // default - command is not builtin
    int cmd_status = 1;

    for (int i = 0; i < *builtincmds_count; i++) {
        if (strcmp(cmd, builtincmds[i]) == 0) {
           cmd_status = 0;
           *builtincmd_idx = i;
           break;
        }
    }

    return cmd_status;
}

/*Execute command built-in to this shell.*/
int execbuiltin(int *builtincmd_idx, char *args[]) {
    switch (*builtincmd_idx) {
        // run "pwd"
        case 1:
            return 1;
        // run "cd"
        case 2:
            chdir(args[1]);
            return 1;
        // run fg
        case 3:
            
            return 1;
        // run jobs
        case 4:
        default:
            break;
    } 

    return 0;
}

/*Retrieves data from user input and forks a child process to execute command
    specified by user.
*/
int main(void) {    
    char *args[20];
    int bg;
    int builtincmd_idx;

    // reference all builtin commands
    int builtincmds_count = 5;
    char *builtincmds[builtincmds_count];
    builtincmds[0] = "exit";
    builtincmds[1] = "pwd";
    builtincmds[2] = "cd";
    builtincmds[3] = "fg";
    builtincmds[4] = "jobs";

    while (1) {
        bg = 0;
        int cnt = getcmd(">>> ", args, &bg);

        // check whether is a built-in (0) or system cmd (1)
        int cmd_status = getcmdstatus(args[0], &builtincmd_idx, &builtincmds_count, builtincmds);

        // create child process
        pid_t child_pid;
        int child_status;

        // directly exit without calling a child process
        if (builtincmd_idx == 0) {
            exit(0);
        }

        // fork current process, both parent and child processes now have a child_pid var
        child_pid = fork();

        // check if the pid is less than 0, something happened
        if (child_pid < 0) {
          exit(-1);
        }

        // child process running, execute stuff there
        else if (child_pid == 0) {
          // is a system command
          if (cmd_status) {
            int status_code = execvp(args[0], args);
            exit(status_code);
          }
          // is a built-in command
          else {
            int status_code = execbuiltin(&builtincmd_idx, args);
            exit(status_code);
          }
        }

        // here from the parent process - can wait for the child to return
        // or continue if supposed to let the child run in the background
        else {
            int exit_status = waitpid(child_pid, &child_status, 0);
            printf("%d", child_status);
        }
      }
}