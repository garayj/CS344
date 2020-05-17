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

// Boolean definition.
typedef enum
{
  false,
  true
} bool;

// Functions
bool commentOrEmptyCheck(int);

int getInput();

void catchSIGINT(int);
void catchSIGTSTP(int);
void checkChildProcessStatus();
void createArgs(char *, char *);
void executeSystemCall();
void findAndReplaceDollars();
void freeInputAndArgs();
void handleSignal(int);
void initSignalHandlers();
void nullOutput();
void openFile(char *, int);

// Global variables.
bool isForegroundMode = false;

char *args[512];
char *input;
char *sigMessage;

int argSize = 0;
int childStatus = -99;
int numberOfBackgroundThreads = 0;
int parentStatus = 0;
int sigLength;

pid_t childThread;

size_t BUFFER_SIZE = 2049;

int main()
{

  // Initialize signal handlers.
  initSignalHandlers();

  // Main prompt loop
  while (true)
  {
    // Clear out input.
    input = calloc(BUFFER_SIZE, sizeof(char));

    // Check for child process termination.
    checkChildProcessStatus();

    // Print out the command line symbol and get input from the user.
    int numCharEntered = getInput();

    // Check if the input is a comment, newline or a signal.
    if (!commentOrEmptyCheck(numCharEntered))
    {
      // Store the input pointer in tmp and search and replace all instances of "$$".
      findAndReplaceDollars();

      // Create a copy of input to manipulate.
      char inputTmp[strlen(input)];
      memset(inputTmp, '\0', sizeof(inputTmp));
      strcpy(inputTmp, input);

      // Get the first word delimited by either a space or newline.
      char *token = strtok(inputTmp, " \n");

      // Check for the supported built in commands.

      // Check for exit.
      if (strcmp(token, "exit") == 0)
      {
        free(input);
        return 0;
      }

      // Check for status.
      else if (strcmp(token, "status") == 0)
      {
        // Check if the last command exited or terminated and print
        // the correct string.
        WIFEXITED(parentStatus) ? printf("exit value %d\n", WEXITSTATUS(parentStatus)) : printf("terminated by signal %d\n", WTERMSIG(parentStatus));
        fflush(stdout);
        parentStatus = 0;
      }

      // Check for cd.
      else if (strcmp(token, "cd") == 0)
      {
        // Get the next argument.
        token = strtok(NULL, " \n");

        // If there is no next argument, move to "HOME".
        if (token == 0)
          chdir(getenv("HOME"));

        // Try to move to the specified directory. Error out
        // if not possible.
        else
        {
          if (chdir(token) == -1)
          {
            perror(token);
          }
        }
        parentStatus = 0;
      }

      // The command is not one of the ones I handle.
      else
      {
        bool isBackgroundProcess = false;

        // Look for if the process needs to run in the background.
        if (input[strlen(input) - 2] == '&')
        {
          input[strlen(input) - 2] = '\0';

          // Check if we are runnning in foreground mode. If we aren't
          // switch the flag to true. In any case, remove the ampersand.
          if (!isForegroundMode)
          {
            isBackgroundProcess = true;
          }
        }

        // Remove the newline character.
        else
          input[strlen(input) - 1] = '\0';

        // Create a child thread.
        childThread = fork();

        switch (childThread)
        {

        // If the creation of the thread is corrupted somehow,
        // shoot out an error.
        case -1:
          perror("Fork is bork!");
          parentStatus = -1;
          break;

        // If the process is the child process.
        case 0:

          // Check if the child process is a background process.
          if (isBackgroundProcess)
            openFile("/dev/null", 2);
          executeSystemCall();

        // Else the process is the parent process.
        default:

          // Check if the process is a background process, print out
          // its process id and don't wait for it to finish.
          if (isBackgroundProcess)
          {
            printf("background pid is %i\n", (int)childThread);
            fflush(stdout);
            waitpid(childThread, &childStatus, WNOHANG);
          }

          // Else, wait for the process to finish.
          else
            waitpid(childThread, &parentStatus, 0);
          break;
        }
      }
    }

    // Free input.
    free(input);
  }

  return 0;
}

// Set up for signal handlers for SIGINT and SIGTSTP.
void initSignalHandlers()
{
  // Taken from the 3.3 notes.
  struct sigaction action_SIGINT = {0};
  struct sigaction action_SIGTSTP = {0};

  action_SIGINT.sa_handler = catchSIGINT;
  action_SIGINT.sa_flags = SA_RESTART;
  sigfillset(&action_SIGINT.sa_mask);
  sigaction(SIGINT, &action_SIGINT, NULL);

  action_SIGTSTP.sa_handler = catchSIGTSTP;
  action_SIGTSTP.sa_flags = SA_RESTART;
  sigfillset(&action_SIGTSTP.sa_mask);
  sigaction(SIGTSTP, &action_SIGTSTP, NULL);
}

// Check if the child process was terminated or exited and print how that process
// terminated or exited.
void checkChildProcessStatus()
{
  // Get a terminated child.
  pid_t terminatedChild = waitpid(-1, &childStatus, WNOHANG);

  // If that child isn't a garbage value, the check if it was exited
  // or terminated by a signal.
  if (childStatus != -99)
  {

    // Print the appropriate message depending on how the process finished.
    if (WIFEXITED(childStatus) != 0)
    {
      printf("background pid %d is done: exit value %d\n", (int)terminatedChild, WEXITSTATUS(childStatus));
    }
    else
    {
      printf("background pid %d is done: terminated by signal %d\n", (int)terminatedChild, WTERMSIG(childStatus));
    }

    // Reset the status to a known invalid value.
    childStatus = -99;
    fflush(stdout);
  }
}

// Print prompt and get line from stdin.
int getInput()
{
  printf(": ");
  fflush(stdout);
  return getline(&input, &BUFFER_SIZE, stdin);
}

// Find all instances of "$$" and expand them out to the pid.
void findAndReplaceDollars()
{
  char *dollars = "$$";

  // Convert the pid from int to string.
  int pidInt = (int)getpid();
  int length = snprintf(NULL, 0, "%d", pidInt);
  char pid[length + 1];
  snprintf(pid, length + 1, "%d", pidInt);

  // Taken and modified from stack overflow:
  // https://stackoverflow.com/questions/27160073/replacing-words-inside-string-in-c

  // Allocate enough memory for expand the "$$" to pid.
  char *dest = calloc(BUFFER_SIZE * 4, sizeof(char));

  // Set temp pointers to point to the beginning of input and the current location
  // in dest.
  char *destptr = dest;
  char *tmp = input;

  *dest = 0;

  // Loop through input until a null terminator is reached.
  while (*input)
  {

    // Check if the next two characters are "$$" and concat
    // both destptr and the pid.
    if (!strncmp(input, dollars, strlen(dollars)))
    {
      strcat(destptr, pid);
      input += strlen(dollars);
      destptr += strlen(pid);
    }

    // Copy over the character from input to destptr and
    // continue moving along the input.
    else
    {
      *destptr = *input;
      destptr++;
      input++;
    }
  }

  // Clear out destptr, free the memory that was at input, and
  // move input to point to dest.
  *destptr = 0;
  free(tmp);
  input = dest;
}

// Check if the input is valid text.
bool commentOrEmptyCheck(int numChars)
{

  // Check is ^C was inputted.
  if (numChars == -1)
  {
    clearerr(stdin);
    return true;
  }

  // Check is a comment or a newline character was received.
  if (input[0] == '#' || input[0] == '\n')
  {
    parentStatus = 0;
    return true;
  }

  // Return false if it passes validation.
  return false;
}

// Attempts execute the command entered. Parses arguments, and reads files
// necessary for the command.
void executeSystemCall()
{

  // Init and clear data for the input file name if one is specified.
  char inputFile[40];
  memset(inputFile, '\0', sizeof(inputFile));

  // Init and clear data for the output file name if one is specified.
  char outputFile[40];
  memset(outputFile, '\0', sizeof(outputFile));

  // Create and store our arguments into the args array.
  createArgs(inputFile, outputFile);

  // Attempts to open the input file if there was one identified in
  // createArgs.
  if (inputFile[0] != '\0')
    openFile(inputFile, 0);
  if (outputFile[0] != '\0')
    openFile(outputFile, 1);

  // Execute the functions.
  if (execvp(args[0], args) == -1)
  {
    printf("%s\n", strerror(errno));
    fflush(stdout);
    freeInputAndArgs();
    exit(14);
  }
}

// Loop through all the arguments and free them. Also free the input line.
void freeInputAndArgs()
{
  int k;
  for (k = 0; k < 512; k++)
  {
    if (args[k] == NULL)
      continue;
    free(args[k]);
  }
  free(input);
}

// Open a file for reading or writing.
// Type 1 is reading an input file, type 2 is writing to
//  an output file, and type 3 is writing to /dev/null.
void openFile(char *file, int type)
{
  // Set up variables for error values,
  // file descriptor and the dup result.
  int openError[] = {11, 12, 13};
  int dupError[] = {21, 22, 23};
  int fd;
  int result;

  // If type is 0 (read), open file for reading
  // else open file for writing.
  if (type == 0)
    fd = open(file, O_RDONLY);
  else
    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0777);

  // Check if there were errors with opening the file.
  if (fd == -1)
  {
    perror(file);
    // Clean up memory
    freeInputAndArgs();
    // Exit
    exit(openError[type]);
  }

  // If type is 3 (/dev/null), redirect to /dev/null
  // else redirect to either stdin or stdout.
  if (type == 2)
    result = dup2(fd, STDOUT_FILENO);
  else
    result = dup2(fd, type);

  // Check if there were errors with dup.
  if (result == -1)
  {
    perror("dup2");

    // Clean up memory
    freeInputAndArgs();

    // Exit
    exit(dupError[type]);
  }

  // Have execvp know to close the file.
  fcntl(fd, F_SETFD, FD_CLOEXEC);
}

// Create arguments from input. Pass the input, arguments array, input and
// output files as arguments.
void createArgs(char *inputFile, char *outputFile)
{

  // in and out flags to let me know if there are any files to read.
  bool in = false;
  bool out = false;

  // Init and clear data for arguments.
  // Reset argument size array.
  int j;
  argSize = 0;
  for (j = 0; j < 512; j++)
  {
    args[j] = calloc(50, sizeof(char));
  }

  // Get the first argument and store it in teh array.
  char *arg = strtok(input, " ");
  strcpy(args[argSize], arg);
  argSize++;

  // Continue pulling out arguments so long as there are
  // arguemnts to pull.
  while (arg)
  {
    arg = strtok(NULL, " ");

    if (arg)
    {

      // If the in flag was set, copy the argument
      // into inputFile variable.
      if (in)
      {
        strcpy(inputFile, arg);
        in = false;
      }

      // If the out flag was set, copy the argument
      // into outputFile variable.
      else if (out)
      {
        strcpy(outputFile, arg);
        out = false;
      }

      // Set the in flag.
      else if (strcmp(arg, "<") == 0)
      {
        in = true;
      }

      // Set the out flag.
      else if (strcmp(arg, ">") == 0)
      {
        out = true;
      }

      // Copy the argument into the args array.
      else
      {
        strcpy(args[argSize], arg);
        argSize++;
      }
    }

    // The argument pulled was NULL. Free the memory at
    // that location and set it to NULL.
    else
    {
      free(args[argSize]);
      args[argSize] = NULL;
      argSize++;
    }
  }
}

// Catch INT signal and do nothing but print a new line.
void catchSIGINT(int signo)
{
  sigMessage = "\n";
  write(STDOUT_FILENO, sigMessage, 1);
}

// Catch TSTP signal and switch foreground mode.
void catchSIGTSTP(int signo)
{

  // If we are in foreground mode, print exit message.
  if (isForegroundMode)
  {
    sigMessage = "Exiting foreground-only mode\n";
    sigLength = 29;
  }

  // If we aren't in foreground mode, print enterance message.
  else
  {
    sigMessage = "Entering foreground-only mode (& is now ignored)\n";
    sigLength = 49;
  }

  // Switch states.
  isForegroundMode = !isForegroundMode;
  write(STDOUT_FILENO, sigMessage, sigLength);
}
