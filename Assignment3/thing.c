/* Small Shell - Program 3 - By Jack Woods (woodjack@oregonstate.edu) */
#define BUF_SIZE 2049

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

int foregroundOnly = 0;

void sigHandler(int sigNum)
{
  switch (sigNum)
  {
  case 2:
    signal(SIGINT, sigHandler);
    printf("terminated by signal %d\n", sigNum);
    fflush(stdout);
    break;
  case 20:
    signal(SIGTSTP, sigHandler);
    if (foregroundOnly)
    {
      printf("Exiting foreground-only mode\n");
      foregroundOnly = 0;
    }
    else
    {
      printf("Entering foreground-only mode (& is now ignored)\n");
      foregroundOnly = 1;
    }
    fflush(stdout);
    break;
  }
}

/* Linear search for space character */
int findSpace(char *str, int pos)
{
  int i;
  for (i = pos; i < strlen(str) - 1; i++)
  {
    if (str[i] == ' ')
    {
      return i;
    }
  }
  return -1;
}

/* Create an arguments array from a string */
void buildArgv(char **current, char *str)
{
  /* Parse command args */
  int max = strlen(str);

  int arg = 0;
  int prevPos = 0;
  int pos = findSpace(str, 0);
  while (prevPos != -1)
  {
    int end = pos != -1 ? pos : max;
    if (prevPos != 0)
      prevPos++;
    char *s = (char *)malloc((end - prevPos) * sizeof(char));
    int i;
    for (i = 0; i < end - prevPos; i++)
    {
      s[i] = str[prevPos + i];
    }
    current[arg] = s;
    arg++;
    prevPos = pos;
    pos = findSpace(str, pos + 1);
  }
}

int commandLen(char *cmd)
{

  /* Get command length */
  /* Count the number of arguments */
  int n = 0;
  int i = 0;
  for (i = 0; i < strlen(cmd); i++)
  {
    if ((int)cmd[i] == 32)
    {
      n++;
    }
  }
  n += 2; /* There is always 1 NULL argument that is appended, and 1 uncounted
             argument */
  return n;
}

int searchPipe(int c, char *cmd)
{
  /* SearchPipe can be used to find either pipe */
  /* I could have made a struct with both boolean "pipe found" variables in
     it, but I found this easier to implement */
  char pipe = c == 0 ? '>' : '<';
  int i = 0;
  while (i < strlen(cmd) - 1)
  {
    if (cmd[i] == pipe)
      return i;
    i++;
  }
  return -1;
}

int findMoney(char *cmd)
{
  int i = 0;
  while (i < strlen(cmd))
  {
    if (cmd[i] == '$' && cmd[i + 1] == '$')
      return i;
    i++;
  }
  return -1;
}

void setInOut(char *cmd, int reDirOut, int reDirIn)
{

  int endCmd = INT_MAX;

  if (reDirIn != -1)
  {
    endCmd = reDirIn;
    reDirIn += 2;
    /* Save the name of the new input */
    int end = findSpace(cmd, reDirIn) != -1 ? findSpace(cmd, reDirIn) : strlen(cmd) - 1;
    char *name = (char *)malloc((end - reDirIn) * sizeof(char));
    memcpy(name, &cmd[reDirIn], (end - reDirIn));

    /* Create FD and re-assign input */
    int targetFD1 = open(name, O_RDONLY);
    if (targetFD1 < 0)
    {
      fprintf(stderr, "cannot open %s for input\n", name);
      exit(1);
    }
    if (dup2(targetFD1, STDIN_FILENO) == -1)
    {
      printf("Error!");
    }

    free(name);
  }

  if (reDirOut != -1)
  {
    endCmd = reDirOut < endCmd ? reDirOut : endCmd;
    reDirOut += 2;
    /* Save the name of the new output */
    int end = findSpace(cmd, reDirOut) != -1 ? findSpace(cmd, reDirOut) : strlen(cmd) - 1;
    char *name = (char *)malloc((end - reDirOut) * sizeof(char));
    memcpy(name, &cmd[reDirOut], (end - reDirOut));

    /* Create FD and re-assign input */
    int targetFD2 = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (targetFD2 < 0)
    {
      fprintf(stderr, "cannot open %s for input\n", name);
      exit(1);
    }
    if (dup2(targetFD2, STDOUT_FILENO) == -1)
    {
      printf("Error!");
    }

    free(name);
  }

  /* Format command */
  endCmd = endCmd < INT_MAX ? endCmd - 1 : strlen(cmd) - 1;
  char *command = (char *)malloc(endCmd * sizeof(char));
  memcpy(command, cmd, endCmd);

  /* Execute command */
  /* Build char* const* array for command */
  int n = commandLen(command);
  char *argv[n];
  buildArgv(argv, command); /* Manipulates argv */
  argv[n - 1] = NULL;       /* Last arg is always 0 */

  /* Execute command */
  if (execvp(argv[0], argv) == -1)
  {
    printf("Error! %s\n", strerror(errno));
    exit(1);
  }
  exit(1);
}

void nullOutput()
{
  /* Create FD and re-assign input */
  int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (targetFD < 0)
  {
    fprintf(stderr, "cannot open /dev/null for input\n");
    exit(1);
  }
  if (dup2(targetFD, STDOUT_FILENO) == -1)
  {
    printf("Error!");
  }
}

int main()
{
  signal(SIGINT, sigHandler);
  signal(SIGTSTP, sigHandler);

  /* Create command buffer */
  char *cmd;
  size_t bufSize = BUF_SIZE;
  cmd = (char *)malloc(BUF_SIZE * sizeof(char));

  pid_t child;

  /* Other useful variables */
  int status = 0;
  int childStatus = -5;

  /* Main application loop */
  int quit = 0;
  while (!quit)
  {
    /* Check to see if child exited */
    waitpid(-1, &childStatus, WNOHANG);
    if (childStatus != -5 && WIFEXITED(childStatus))
    {
      printf("background pid %d is done: exit value %d\n", child, WEXITSTATUS(childStatus));
      childStatus = -5;
    }
    if (childStatus != -5 && WIFSIGNALED(childStatus))
    {
      printf("background pid %d is done: terminated by signal %d", child, WTERMSIG(childStatus));
      childStatus = -5;
    }

    /* Prompt user for input */
    printf(": ");
    int bytesRead = getline(&cmd, &bufSize, stdin);
    fflush(stdout);

    /* Parse command */
    int moneyLoc = findMoney(cmd);
    if (moneyLoc != -1)
    {
      char buff[moneyLoc];
      buff[moneyLoc] = '\0';
      memcpy(&buff, cmd, moneyLoc);
      sprintf(cmd, "%s%d\n", &buff, (int)getpid());
    }

    /* Check for comment first */
    if (cmd[0] == '#' || strlen(cmd) == 1)
      status = 0;

    /* Check for built-in commands */
    if (strncmp(cmd, "exit", 4) == 0)
      return 0;
    else if (strncmp(cmd, "status", 6) == 0)
    {
      printf("exit value %d\n", status / 256);
    }
    else if (strncmp(cmd, "cd", 2) == 0)
    {
      char token = '\n';
      strtok(&cmd[3], &token);
      if (strlen(&cmd[3]) == 0)
        chdir(getenv("HOME"));
      else
      {
        if (chdir(&cmd[3]) == -1)
        {
          printf("Error! %s\n", strerror(errno));
          status = -1;
        }
      }
    }
    else
    {
      /* Call another process and manage it */
      int bg = 0;
      if (cmd[strlen(cmd) - 2] == '&')
      {
        if (!foregroundOnly)
          bg = 1;
        cmd[strlen(cmd) - 2] = '\0';
      }
      child = fork();
      if (child == -1)
      {
        perror("Hull Breach!");
        status = -1;
      }
      else if (child == 0)
      {
        if (bg)
        {
          nullOutput();
          signal(SIGINT, SIG_IGN);
          signal(SIGTSTP, SIG_IGN);
        }
        int reDirOut = searchPipe(0, cmd);
        int reDirIn = searchPipe(1, cmd);
        setInOut(cmd, reDirOut, reDirIn);
      }
      else
      {
        if (bg)
        {
          printf("background pid is %i\n", (int)child);
          waitpid(child, &childStatus, WNOHANG);
        }
        else
        {
          waitpid(child, &status, 0);
        }
      }
    }

    if (status < 0)
    {
      printf("Exiting...\n");
      return status;
    }
  }

  free(cmd);
  return 0;
}