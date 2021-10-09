#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// empirical active kid tracker for signal handling
pid_t current_block;

void
sigz_handler (int signum)
{
}

void
sigc_handler (int signum)
{
  if (current_block > 0)
    {
      kill (current_block, signum);
    }
}

/* Loads the the contents of the agrs array with the command line given
    by the user.
  */
int
getcmd (char *prompt, char *args[], int *background)
{
  int length, i = 0;
  char *token, *loc;
  char *line = NULL;
  size_t linecap = 0;

  // display prompt mark & rea user input
  printf ("%s", prompt);
  length = getline (&line, &linecap, stdin);

  // check if any actual argument was passed in this command line
  if (length <= 0)
    {
      exit (-1);
    }

  // check if '&' was provided in input and it in input str by whitespace
  if ((loc = index (line, '&')) != NULL)
    {
      *background = 1;
      *loc = ' ';
    }
  else
    {
      *background = 0;
    }

  // parse line to retrieve each token delimited by ' ', \t or \n
  while ((token = strsep (&line, " \t\n")) != NULL)
    {
      // filter for only ASCII char values of 32 and above
      for (int j = 0; j < strlen (token); j++)
        {
          if (token[j] <= 32)
            {
              token[j] = '\0';
            }
        }
      // if token exists - add it to the arguments
      if (strlen (token) > 0)
        {
          args[i++] = token;
        }
    }

  // indicate no arguments found in command line
  if (i == 0)
    {
      return -1;
    }

  // TODO: explain fix for execvp to work
  args[i++] = NULL;

  return (i - 1);
}

// TODO: implement fg: called with a number and pick specified pid and put it
// in foreground

int
getcmdstatus (char *cmd, int *builtincmd_idx, int *builtincmds_count,
              char *builtincmds[])
{
  // default - command is not builtin
  int cmd_status = 0;

  for (int i = 0; i < *builtincmds_count; i++)
    {
      if (strcmp (cmd, builtincmds[i]) == 0)
        {
          cmd_status = 1;
          *builtincmd_idx = i;
          break;
        }
    }

  return cmd_status;
}

int
runcwd ()
{
  char cwd[256];
  getcwd (cwd, sizeof (cwd));
  printf ("%s\n", cwd);
  return 1;
}

int
rediroutput (char *args[], int *arg_count, char *output_filename[])
{
  for (int i = 0; i < *arg_count; i++)
    {
      if (args[i][0] == '>')
        {
          // if no argument is provided after pipe, error
          if (args[i + 1] == NULL)
            {
              return -1;
            }
          else
            {
              *output_filename = args[i + 1];
              return i;
            }
        }
    }
  return 0;
}

int
pipedoutput (char *args[], char *pipedargs[], int *arg_count)
{
  for (int i = 0; i < *arg_count; i++)
    {
      if (args[i][0] == '|')
        {
          // if no argument is provided after pipe, error
          if (args[i + 1] == NULL)
            {
              // handle the | character otherwise passed to input
              return -1;
            }
          else
            {
              // init piped args & remove from args
              for (int j = 0; j < *arg_count; j++)
                {
                  pipedargs[j] = args[i + j + 1];
                  args[i + j + 1] = NULL;
                  if (args[i + j + 2] == NULL)
                    {
                      pipedargs[j + 1] = NULL;
                      break;
                    }
                }
              args[i] = NULL;
            }
          return 1;
        }
    }
  return 0;
}

int
updatepids (int *pid_count, pid_t pids[])
{
  pid_t newpids[256];
  int newpid_count = 0;
  int pid_status;
  int child_status;

  for (int i = 0; i < *pid_count; i++)
    {
      // check that the current pid is still active
      pid_status = waitpid (pids[i], &child_status, WNOHANG);
      // still valid pid, running process
      if (pid_status == 0)
        {
          newpids[newpid_count++] = pids[i];
        }
    }

  // update vals for pids
  *pid_count = newpid_count;
  pids = newpids;

  return 1;
}

int
fg (int *selected_pid, int *pid_count, pid_t pids[])
{
  updatepids (pid_count, pids);

  int process_found = 0;
  // check if the selected pid is available
  for (int i = 0; i < *pid_count; i++)
    {
      if (pids[i] == *selected_pid)
        {
          process_found = 1;
        }
    }
  if (process_found == 0)
    {
      printf ("error: process with pid %d was not found\n", *selected_pid);
    }

  // otherwise wait for child to complete
  int child_status;
  int pid_status = waitpid (*selected_pid, &child_status, 0);

  return 1;
}

int
jobs (int *pid_count, pid_t pids[])
{
  // check that the list is still valid
  updatepids (pid_count, pids);

  printf ("Proc PID\n");
  for (int i = 0; i < *pid_count; i++)
    {
      printf ("%d %d\n", (i + 1), pids[i]);
    }
  return 1;
}

/*Execute command built-in to this shell.*/
int
execbuiltin (int *builtincmd_idx, char *args[], pid_t pids[], int *pid_count)
{
  int selected_pid = -1;

  switch (*builtincmd_idx)
    {
    // run "exit"
    case 0:
      exit (0);
    // run "pwd"
    case 1:
      runcwd ();
      return 1;
    // run "cd"
    case 2:
      // accounting for fix with execvp and appended NULL value
      if (args[1] == NULL)
        {
          // print current directory if no arguments shown
          runcwd ();
        }
      else
        {
          // change dir to first command line argument
          chdir (args[1]);
        }
      return 1;
    // run fg
    case 3:
      // if arg 1 exists (i.e. pid to bring to foreground)
      // otherwise bring the first process in the pid list (oldest child
      // running) if not do not do anythign
      if (args[1] != NULL)
        {
          selected_pid = atoi (args[1]);
        }
      fg (&selected_pid, pid_count, pids);
      return 1;
    // run jobs
    case 4:
      jobs (pid_count, pids);
      return 1;
    default:
      break;
    }

  return 0;
}

int
bepiped (char *args[], pid_t pids[], int *pid_count, int *background,
         char *builtincmds[], int *builtincmds_count, int pipefd[])
{
  int builtincmd_idx = 0;

  if (getcmdstatus (args[0], &builtincmd_idx, builtincmds_count, builtincmds))
    {
      dup2 (pipefd[1], 1);
      close (pipefd[0]);
      close (pipefd[1]);

      int status_code = execbuiltin (&builtincmd_idx, args, pids, pid_count);
    }
  else
    {
      int child_status;
      pid_t child_pid = fork ();

      if (child_pid < 0)
        {
          // log error message in case of issue
          printf ("error: could not fork child\n");
          exit (-1);
        }
      else if (child_pid == 0)
        {
          // setup child to write to pipe for other kid to read
          dup2 (pipefd[1], 1);
          close (pipefd[0]);
          close (pipefd[1]);

          int status_code = execvp (args[0], args);
          exit (status_code);
        }
      else if (*background == 0)
        {
          current_block = child_pid;
          pids[(*pid_count)++] = child_pid;

          // always assume the first part of the pipe will be awaited for
          int exit_status = waitpid (child_pid, &child_status, 0);
          current_block = -1;
        }
    }

  return 1;
}

int
runpiped (char *args[], char *pipedargs[], pid_t pids[], int *pid_count,
          int *background, char *builtincmds[], int *builtincmds_count)
{
  // create pipe for input/output tunnelling
  int pipefd[2];
  pipe (pipefd);

  if (pipe (pipefd) < 0)
    {
      printf ("error: could not initialize pipe\n");
      exit (-1);
    }

  int builtincmd_idx = 0;
  if (getcmdstatus (pipedargs[0], &builtincmd_idx, builtincmds_count,
                    builtincmds))
    {
      // run second part of the pipe, pipedargs
      int status_code = bepiped (args, pids, pid_count, background,
                                 builtincmds, builtincmds_count, pipefd);

      dup2 (pipefd[0], 0);
      close (pipefd[1]);
      close (pipefd[0]);

      status_code = execbuiltin (&builtincmd_idx, pipedargs, pids, pid_count);
    }
  else
    {
      // fork current process to run command in a child
      int child_status;
      pid_t child_pid = fork ();

      if (child_pid < 0)
        {
          // log error message in case of issue
          printf ("error: could not fork child\n");
          exit (-1);
        }
      else if (child_pid == 0)
        {
          // setup pipefd such that can read from it
          dup2 (pipefd[0], 0);
          close (pipefd[1]);
          close (pipefd[0]);

          int s = execvp (pipedargs[0], pipedargs);
          exit (1);
        }
      else
        {
          current_block = child_pid;
          pids[(*pid_count)++] = child_pid;

          // run second part of the pipe, pipedargs
          int status_code = bepiped (args, pids, pid_count, background,
                                     builtincmds, builtincmds_count, pipefd);

          // need to close the pipes for the parent at the end
          close (pipefd[0]);
          close (pipefd[1]);

          // always assume the first part of the pipe will be awaited for
          int exit_status = waitpid (child_pid, &child_status, 0);
          current_block = -1;
        }
    }

  return 1;
}

int
runredirected (char *args[], pid_t pids[], int *pid_count, int *background,
               char *output_filename[], char *builtincmds[],
               int *builtincmds_count)
{
  /* Redirect output from executed command to output filename.*/
  int builtincmd_idx = 0;
  if (getcmdstatus (args[0], &builtincmd_idx, builtincmds_count, builtincmds))
    {

      int status_code = execbuiltin (&builtincmd_idx, args, pids, pid_count);
    }
  else
    {
      // fork current process to run command in a child
      int child_status;
      pid_t child_pid = fork ();

      if (child_pid < 0)
        {
          // log error message in case of issue
          printf ("error: could not fork child\n");
          exit (-1);
        }
      else if (child_pid == 0)
        {
          int fd1 = creat (*output_filename, 0644);
          dup2 (fd1, STDOUT_FILENO);
          close (fd1);

          int status_code = execvp (args[0], args);
          exit (status_code);
        }
      else if (background == 0)
        {
          current_block = child_pid;
          pids[(*pid_count)++] = child_pid;

          int exit_status = waitpid (child_pid, &child_status, 0);
          current_block = -1;
        }
    }

  return 1;
}

/*Retrieves data from user input and forks a child process to execute command
    specified by user.
*/
int
main (void)
{
  // ignore any CTRL+Z signals coming up
  signal (SIGTSTP, sigz_handler);
  signal (SIGINT, sigc_handler);

  // parsed command line arguments
  char *args[20];
  char *pipedargs[20];
  // boolean for background process
  int bg;
  // store background pid processes
  pid_t pids[256];
  int pid_count = 0;

  // reference all builtin commands
  int builtincmds_count = 5;
  char *builtincmds[builtincmds_count];
  builtincmds[0] = "exit";
  builtincmds[1] = "pwd";
  builtincmds[2] = "cd";
  builtincmds[3] = "fg";
  builtincmds[4] = "jobs";

  while (1)
    {
      char *output_filename[1];
      bg = 0;
      int builtincmd_idx = 0;

      int cnt = getcmd (">>> ", args, &bg);
      if (cnt == -1)
        {
          continue;
        }

      int redirected = rediroutput (args, &cnt, output_filename);
      // printf("is redirected %d\n", redirected);
      int piped = pipedoutput (args, pipedargs, &cnt);
      // printf("is piped %d\n", piped);

      if (piped && (redirected > 0))
        {
          printf ("error: do not support pipe and redirection\n");
          exit (-1);
        }
      else if (piped)
        {
          int status_code = runpiped (args, pipedargs, pids, &pid_count, &bg,
                                      builtincmds, &builtincmds_count);
        }
      else if (redirected > 0)
        {
          // remove ">" and "filename" args from array
          args[redirected] = NULL;
          args[redirected + 1] = NULL;

          int status_code
              = runredirected (args, pids, &pid_count, &bg, output_filename,
                               builtincmds, &builtincmds_count);
        }
      else if (getcmdstatus (args[0], &builtincmd_idx, &builtincmds_count,
                             builtincmds))
        {
          int status_code
              = execbuiltin (&builtincmd_idx, args, pids, &pid_count);
        }
      else
        {
          // fork current process to run command in a child
          int child_status;
          pid_t child_pid = fork ();

          if (child_pid < 0)
            {
              // log error message in case of issue
              printf ("error: could not fork child\n");
              exit (-1);
            }
          else if (child_pid == 0)
            {
              int status_code = execvp (args[0], args);
              exit (status_code);
            }

          // here from the parent process - can wait for the child to return
          // or continue if supposed to let the child run in the background
          else if (bg == 0)
            {
              current_block = child_pid;
              pids[pid_count++] = child_pid;
              int exit_status = waitpid (child_pid, &child_status, 0);
              current_block = -1;
            }
          else
            {
              pids[pid_count++] = child_pid;
              // not wait for child process to complete
              continue;
            }
        }
    }
}
