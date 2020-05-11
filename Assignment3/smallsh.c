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
#include <math.h>

typedef enum
{
  false,
  true
} bool;

// Functions
void checkChildProcessStatus();
void getInput(char *);
void findAndReplaceDollars(char *, char **);
bool commentOrEmptyCheck(char *);
void handleSignal(int);
int findSpace(char *, int);
void buildArgv(char **, char *);
int commandLen(char *);
int searchPipe(int, char *);
void setInOut(char *, int, int);
void nullOutput();

// Global variables.
bool isForegroundMode = false;
size_t BUFFER_SIZE = 2049;
pid_t childThread;
int childStatus = -99;
int parentStatus = 0;

int main()
{
  // Signal Handlers for cmd-C and cmd-Z
  signal(SIGINT, handleSignal);
  signal(SIGTSTP, handleSignal);

  // Main prompt loop
  while (true)
  {
    char *input = calloc(BUFFER_SIZE, sizeof(char));
    // Check for child process termination.
    checkChildProcessStatus();

    // Print out the command line symbol and read input from the user.
    getInput(input);

    // Store the input pointer in tmp and search and replace all instances of "$$".
    findAndReplaceDollars(input, &input);

    // Check if the input is a comment or just a newline.
    if (!commentOrEmptyCheck(input))
    {
      // Make a copy of input just in case I need an un messed with string.
      char inputTmp[BUFFER_SIZE];
      memset(inputTmp, '\0', sizeof(inputTmp));
      strcpy(inputTmp, input);

      char newLine = '\n';
      char *token = strtok(inputTmp, " \n");

      // Check for the supported built in commands.
      if (strcmp(token, "exit") == 0)
      {
        free(input);
        return 0;
      }
      else if (strcmp(token, "status") == 0)
      {
        printf("exit value %d\n", parentStatus / 256);
        fflush(stdout);
        parentStatus = 0;
      }
      else if (strcmp(token, "cd") == 0)
      {
        token = strtok(NULL, " \n");
        if (token == 0)
          chdir(getenv("HOME"));
        else
        {
          if (chdir(token) == -1)
          {
            printf("%s: %s\n", token, strerror(errno));
            fflush(stdout);
          }
        }
        parentStatus = 0;
      }
      else
      {
        // Make a system call.
        bool isBackgroundProcess = false;
        int lastIndex = strlen(input) - 2;
        // Look for if the process needs to run in the background.
        if (input[lastIndex] == '&')
        {
          if (!isForegroundMode)
            isBackgroundProcess = true;
          input[lastIndex] = '\0';
        }
        childThread = fork();
        if (childThread == -1)
        {
          perror("Fork is bork!");
          parentStatus = -1;
        }
        else if (childThread == 0)
        {
          if (isBackgroundProcess)
          {
            nullOutput();
            signal(SIGINT, SIG_IGN);
            signal(SIGTSTP, SIG_IGN);
          }
          int reDirOut = searchPipe(0, input);
          int reDirIn = searchPipe(1, input);
          setInOut(input, reDirOut, reDirIn);
        }
        else
        {
          if (isBackgroundProcess)
          {
            printf("background pid is %i\n", (int)childThread);
            fflush(stdout);
            waitpid(childThread, &childStatus, WNOHANG);
          }
          else
          {
            waitpid(childThread, &parentStatus, 0);
          }
        }
      }
    }

    free(input);
    if (parentStatus < 0)
    {
      printf("Exiting...\n");
      return parentStatus;
    }
  }

  return 0;
}

void checkChildProcessStatus()
{
  waitpid(-1, &childStatus, WNOHANG);
  if (childStatus != -99)
  {
    if (WIFEXITED(childStatus))
    {
      printf("background pid %d is done: exit value %d\n", childThread, WEXITSTATUS(childStatus));
    }
    else
    {

      printf("background pid %d is done: terminated by signal %d\n", childThread, WTERMSIG(childStatus));
    }
    childStatus = -99;
    fflush(stdout);
  }
}

void getInput(char *input)
{
  printf(": ");
  fflush(stdout);
  getline(&input, &BUFFER_SIZE, stdin);
}

void findAndReplaceDollars(char *input, char **inputAddress)
{
  int pidInt = (int)getpid();
  int nDigits = floor(log10(abs(pidInt))) + 1;
  char pid[nDigits];
  char *dollars = "$$";
  snprintf(pid, nDigits + 1, "%d", pidInt);

  char *dest = malloc(strlen(input) - strlen(dollars) + strlen(pid) + 1);
  char *destptr = dest;

  *dest = 0;

  while (*input)
  {
    if (!strncmp(input, dollars, strlen(dollars)))
    {
      strcat(destptr, pid);
      input += strlen(dollars);
      destptr += strlen(pid);
    }
    else
    {
      *destptr = *input;
      destptr++;
      input++;
    }
  }
  *destptr = 0;
  free(*inputAddress);
  *inputAddress = dest;
}

bool commentOrEmptyCheck(char *input)
{
  if (input[0] == '\n' || input[0] == '#')
  {
    parentStatus = 0;
    return true;
  }
  return false;
}

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
      fflush(stdout);
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
      fflush(stdout);
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
    fflush(stdout);
  }
}

void handleSignal(int bigSig)
{
  if (bigSig == 2)
  {
    signal(SIGINT, handleSignal);
    printf("terminated by signal %d\n", bigSig);
    parentStatus = bigSig;
  }
  else if (bigSig == 20)
  {
    signal(SIGTSTP, handleSignal);
    isForegroundMode ? printf("Exiting foreground-only mode\n") : printf("Entering foreground-only mode (& is now ignored)\n");
    isForegroundMode = !isForegroundMode;
  }
  fflush(stdout);
}