#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

// Boolean definition.
typedef enum
{
  false,
  true
} bool;

// Functions
void initSignalHandlers();
void checkChildProcessStatus();
int getInput(char *);
void findAndReplaceDollars(char *, char **);
bool commentOrEmptyCheck(int, char *);
void handleSignal(int);
void createArgs(char *, char **, char *, char *);
void openIOFiles(char *, char *);
void executeSystemCall(char *);
void backgroundNullOut();
void catchSIGINT(int);
void catchSIGTSTP(int);

// Global variables.
pid_t backgroundThreads[100];
int numberOfBackgroundThreads = 0;
int backgroundThreadsStatus[100];
bool isForegroundMode = false;
size_t BUFFER_SIZE = 2049;
pid_t childThread;
char *sigMessage;
int sigLength;
pid_t backgroundThreads[100];
int backgroundStatuses[100];
int childStatus = -99;
int parentStatus = 0;

int main()
{
  initSignalHandlers();
  int as;
  for (as = 0; as < 100; as++)
  {
    backgroundThreadsStatus[as] = -99;
  }

  // Main prompt loop
  while (true)
  {
    char *input = calloc(BUFFER_SIZE, sizeof(char));
    // Check for child process termination.
    waitpid(-1, &childStatus, WNOHANG);
    if (childStatus != -99)
    {
      if (WIFEXITED(childStatus))
      {
        printf("background pid %d is done: exit value %d\n", childStatus, WEXITSTATUS(childStatus));
      }
      else
      {
        printf("background pid %d is done: terminated by signal %d\n", childStatus, WTERMSIG(childStatus));
      }
      childStatus = -99;
      fflush(stdout);
    }

    // Print out the command line symbol and read input from the user.
    int numCharEntered = getInput(input);

    // Check if the input is a comment or just a newline.
    if (!commentOrEmptyCheck(numCharEntered, input))
    {
      printf("parent status: %d\n", parentStatus);
      fflush(stdout);
      // Store the input pointer in tmp and search and replace all instances of "$$".
      findAndReplaceDollars(input, &input);

      char inputTmp[BUFFER_SIZE];
      memset(inputTmp, '\0', sizeof(inputTmp));
      strcpy(inputTmp, input);

      char *token = strtok(inputTmp, " \n");

      // Check for the supported built in commands.
      if (strcmp(token, "exit") == 0)
      {
        free(input);
        return 0;
      }
      else if (strcmp(token, "status") == 0)
      {
        if (WIFEXITED(parentStatus) != 0)
        {
          printf("exit value %d\n", WEXITSTATUS(parentStatus));
        }
        else
        {
          printf("exit signal %d\n", WTERMSIG(parentStatus));
        }
        fflush(stdout);
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
            perror(token);
          }
        }
        parentStatus = 0;
      }
      // ******************************************************************************
      // The command is not one of the ones I handle.
      else
      {
        bool isBackgroundProcess = false;
        int inputLen = strlen(input);

        // Look for if the process needs to run in the background.
        if (input[inputLen - 2] == '&')
        {
          input[inputLen - 2] = '\0';
          // Check if we are runnning in foreground mode. If we aren't
          // switch the flag to true. In any case, remove the ampersand.
          if (!isForegroundMode)
            isBackgroundProcess = true;
        }
        // Remove the newline character ifi
        else
          input[inputLen - 1] = '\0';

        // Create a child thread.
        childThread = fork();
        // If the creation of the thread is corrupted somehow,
        // shoot out an error.
        if (childThread == -1)
        {
          perror("Fork is bork!");
          parentStatus = -1;
        }
        // If the process is the child process.
        else if (childThread == 0)
        {
          // Check if the child process is a background process.
          // Redirect to /dev/null if true.
          if (isBackgroundProcess)
          {
            // Store background process in array.
            // int index;
            // for (index = 0; index < 100; index++)
            // {
            // }
            backgroundNullOut();
          }

          // Attempt to do a system call.
          executeSystemCall(input);
        }
        // Else the process is the parent process.
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
  }

  return 0;
}

void initSignalHandlers()
{
  // Clear out struct.
  struct sigaction action_SIGINT = {0};
  struct sigaction action_SIGTSTP = {0};

  // Give it the catchSIGINT function
  action_SIGINT.sa_handler = catchSIGINT;
  action_SIGINT.sa_flags = 0;
  // Tell it to ignore all other signals.
  sigfillset(&action_SIGINT.sa_mask);
  // Tell it to do my action when you see sigint.
  sigaction(SIGINT, &action_SIGINT, NULL);

  action_SIGTSTP.sa_handler = catchSIGTSTP;
  action_SIGTSTP.sa_flags = 0;
  sigfillset(&action_SIGTSTP.sa_mask);
  sigaction(SIGTSTP, &action_SIGTSTP, NULL);
}

void checkChildProcessStatus()
{
  int i;
  for (i = 0; i < numberOfBackgroundThreads; i++)
  {
    waitpid(-1, &backgroundThreadsStatus[i], WNOHANG);
    if (backgroundThreadsStatus[i] != -99)
    {
      if (WIFEXITED(backgroundThreadsStatus[i]))
      {
        printf("background pid %d is done: exit value %d\n", backgroundThreads[i], WEXITSTATUS(backgroundThreadsStatus[i]));
      }
      else
      {
        printf("background pid %d is done: terminated by signal %d\n", backgroundThreads[i], WTERMSIG(backgroundThreadsStatus[i]));
      }
      backgroundThreadsStatus[i] = -99;
      fflush(stdout);
    }
  }
}

int getInput(char *input)
{
  printf(": ");
  fflush(stdout);
  return getline(&input, &BUFFER_SIZE, stdin);
}

void findAndReplaceDollars(char *input, char **inputAddress)
{
  int pidInt = (int)getpid();
  int length = snprintf(NULL, 0, "%d", pidInt);
  char pid[length + 1];
  char *dollars = "$$";
  snprintf(pid, length + 1, "%d", pidInt);

  char *dest = calloc(BUFFER_SIZE * 4, sizeof(char));
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

bool commentOrEmptyCheck(int numChars, char *input)
{
  if (numChars == -1)
  {
    clearerr(stdin);
    return true;
  }
  if (input[0] == '#' || input[0] == '\n')
  {
    return true;
  }
  return false;
}

void executeSystemCall(char *input)
{
  // Init and clear data for arguments.
  char *args[512];
  int j;
  for (j = 0; j < 512; j++)
  {
    args[j] = calloc(50, sizeof(char));
  }

  // Init and clear data for the input file name if one is specified.
  char inputFile[40];
  memset(inputFile, '\0', sizeof(inputFile));

  // Init and clear data for the output file name if one is specified.
  char outputFile[40];
  memset(outputFile, '\0', sizeof(outputFile));

  // Create and store our arguments into the args array.
  createArgs(input, args, inputFile, outputFile);

  // Attempts to open the input file if there was one identified in
  // createArgs.
  openIOFiles(inputFile, outputFile);

  // Execute the functions.
  if (execvp(args[0], args) == -1)
  {
    printf("Error!! %s\n", strerror(errno));
    fflush(stdout);
    int k;
    for (k = 0; k < 512; k++)
    {
      if (args[k] == NULL)
        continue;
      free(args[k]);
    }
    free(input);
    exit(14);
  }
}

void openIOFiles(char *in, char *out)
{
  // If there is an input file specified, try to read from it.
  if (in[0] != '\0')
  {
    int fdIn = open(in, O_RDONLY);
    if (fdIn == -1)
    {
      perror(in);
      // Clean up memory
      exit(17);
    }
    int inResult = dup2(fdIn, 0);
    if (inResult == -1)
    {
      perror("dup2");
      // Clean up memory
      exit(11);
    }
    fcntl(fdIn, F_SETFD, FD_CLOEXEC);
  }
  if (out[0] != '\0')
  {
    int fdOut = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdOut == -1)
    {
      perror("open");
      // Clean up memory
      exit(12);
    }
    int outResult = dup2(fdOut, 1);
    if (outResult == -1)
    {
      perror("dup2");
      // Clean up memory
      exit(13);
    }
    fcntl(fdOut, F_SETFD, FD_CLOEXEC);
  }
}

void createArgs(char *input, char **args, char *inputFile, char *outputFile)
{
  bool in = false;
  bool out = false;
  int size = 0;

  char *arg = strtok(input, " ");

  strcpy(args[size], arg);
  size++;
  while (arg)
  {
    arg = strtok(NULL, " ");
    if (arg)
    {
      if (in)
      {
        strcpy(inputFile, arg);
        in = false;
      }
      else if (out)
      {
        strcpy(outputFile, arg);
        out = false;
      }
      else if (strcmp(arg, "<") == 0)
      {
        in = true;
      }
      else if (strcmp(arg, ">") == 0)
      {
        out = true;
      }
      else
      {
        strcpy(args[size], arg);
        size++;
      }
    }
    else
    {
      free(args[size]);
      args[size] = NULL;
      size++;
    }
  }
}

void backgroundNullOut()
{
  /* Create FD and re-assign input */
  int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (targetFD < 0)
  {
    perror("/dev/null");
    // Clean up memory
    exit(21);
  }
  if (dup2(targetFD, STDOUT_FILENO) == -1)
  {
    perror("dup2");
    // Clean up memory
    exit(22);
  }
}

void catchSIGINT(int signo)
{

  char *sigMessage = "\n";
  write(STDOUT_FILENO, sigMessage, 1);
}

void catchSIGTSTP(int signo)
{
  if (isForegroundMode)
  {
    sigMessage = "Exiting foreground-only mode\n";
    sigLength = 29;
  }
  else
  {
    sigMessage = "Entering foreground-only mode (& is now ignored)\n";
    sigLength = 49;
  }
  isForegroundMode = !isForegroundMode;
  write(STDOUT_FILENO, sigMessage, sigLength);
}
