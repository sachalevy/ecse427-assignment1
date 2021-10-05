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

  // indicate no arguments found in command line
  if (i==0) {
    return -1;
  }

  //TODO: explain fix for execvp to work 
  args[i++] = NULL;

  return (i-1);
}

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

int runcwd() {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
    return 1;
}

int rediroutput(char *args[], int *arg_count, char *output_filename[]) {
    for(int i = 0; i < *arg_count; i++) {
        if(args[i][0]== '>') {
            if (args[i+1] != NULL) {
                *output_filename = args[i+1];
            } else {
                return -1;
            }
            // replace all upcoming args to account for redirected output on this command
            for(int j = i; args[j-1] != NULL; j++) {
                args[j] = args[j+2];
            }
            return 1;
        }
    }
    return 0;
}


int updatepids(int *pid_count, pid_t pids[]) {
    pid_t newpids[256];
    int newpid_count = 0;
    int pid_status;
    int child_status;

    for (int i=0; i < *pid_count; i++) {
        // check that the current pid is still active
        pid_status = waitpid(pids[i], &child_status, WNOHANG);
        // still valid pid, running process
        if (pid_status == 0) {
            newpids[newpid_count++] = pids[i];
        }
    }

    // update vals for pids
    *pid_count = newpid_count;
    pids = newpids;

    return 1;
}

int fg(int *selected_pid, int *pid_count, pid_t pids[]) {
    updatepids(pid_count, pids);

    return 1;
}

int jobs(int *pid_count, pid_t pids[]) {
    // check that the list is still valid 
    updatepids(pid_count, pids);

    printf("PID\n");
    for (int i = 0; i < *pid_count; i++) {
        printf("%d\n", pids[i]);
    }
    return 1;
}

/*Execute command built-in to this shell.*/
int execbuiltin(int *builtincmd_idx, char *args[], pid_t pids[], int *pid_count) {
    int selected_pid = -1;

    switch (*builtincmd_idx) {
        // run "exit"
        case 0:
            exit(0);
        // run "pwd"
        case 1:
            runcwd();
            return 1;
        // run "cd"
        case 2:
            // accounting for fix with execvp and appended NULL value
            if (args[1] == NULL) {
                // print current directory if no arguments shown
                runcwd();
            } else {
                // change dir to first command line argument
                chdir(args[1]);
            }
            return 1;
        // run fg
        case 3:
            // if arg 1 exists (i.e. pid to bring to foreground)
            // otherwise bring the first process in the pid list (oldest child running)
            // if not do not do anythign
            if (args[1] != NULL) {
                selected_pid = atoi(args[1]);
            }
            fg(&selected_pid, pid_count, pids);
            return 1;
        // run jobs
        case 4:
            jobs(pid_count, pids);
            return 1;
        default:
            break;
    } 

    return 0;
}

/*Retrieves data from user input and forks a child process to execute command
    specified by user.
*/
int main(void) {    
    // parsed command line arguments
    char *args[20];
    // boolean for background process
    int bg;
    // record the index of the builtin command to execute
    int builtincmd_idx;
    // store background pid processes
    pid_t pids[256];
    int pid_count = 0;
    int child_status;

    // reference all builtin commands
    int builtincmds_count = 5;
    char *builtincmds[builtincmds_count];
    builtincmds[0] = "exit";
    builtincmds[1] = "pwd";
    builtincmds[2] = "cd";
    builtincmds[3] = "fg";
    builtincmds[4] = "jobs";

    while (1) {
        char *output_filename[1];
        bg = 0;
        int cnt = getcmd(">>> ", args, &bg);
        if (cnt == -1) {
            continue;
        }

        // check whether is a built-in (0) or system cmd (1)
        int cmd_status = getcmdstatus(args[0], &builtincmd_idx, &builtincmds_count, builtincmds);
        // TODO: modify the output filename from an array of strings to a string
        int redirected = rediroutput(args, &cnt, output_filename);
        if(redirected == 1) {
            freopen(output_filename[0], "w+", stdout);
        }

        if (cmd_status == 0) {
            // execute directly in parent process
            int status_code = execbuiltin(&builtincmd_idx, args, pids, &pid_count);
        } else {
            // fork current process, both parent and child processes now have a child_pid var
            pid_t child_pid = fork();
            //printf("pid count %d pid %d \n", pid_count, *pids[pid_count]);
            pids[pid_count++] = child_pid;

            // check if the pid is less than 0, something happened
            if (child_pid < 0) {
              exit(-1);
            }

            // child process running, execute stuff there
            else if (child_pid == 0) {
                int status_code = execvp(args[0], args);
                exit(status_code);
            }

            // here from the parent process - can wait for the child to return
            // or continue if supposed to let the child run in the background
            else if (bg == 0){
                int exit_status = waitpid(child_pid, &child_status, 0);
            }
            else {
                continue;
            }
        }
    }        
}
