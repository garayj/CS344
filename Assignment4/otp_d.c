#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>

// Boolean definition.
typedef enum
{
    false,
    true
} bool;

// Global vars.
int childStatus = -99, children = 0;
struct sigaction action_SIGCHILD = {0};
char *ciphertext = NULL;

// Functions
int acceptConnection(int);
int getMessageLength();
int getPidLength();
int sendAll(int, char *);
int setupSocketAndListen(char *);

void catchSIGCHLD(int);
void error(const char *);
void getOldestFile(char *, char *);
void getMetaData(char *, char *, char *);
void handleGET(int, char *);
void handleIncomingRequest(int);
void handlePOST(int, char *);
void openAndStoreContents(char *, char **);
void printFilePath(char *);
void readIncomingMessage(char *, int, int);
void setFileName(char *, char *);
void setFormattedMessage(char *, char *);
void setPayload(char *, int);
void setupSignalHandler();
void writeEncryptedMessage(char *, char *);

int main(int argc, char *argv[])
{
    // Setup variables.
    int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
    socklen_t sizeOfClientInfo;
    struct sockaddr_in clientAddress;

    if (argc < 2)
    {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    } // Check usage & args

    setupSignalHandler();                           // Setup the signal handler for child processes.
    listenSocketFD = setupSocketAndListen(argv[1]); // Setup the socket for listening.
    while (1)                                       // Loop until the process is killed.
    {
        establishedConnectionFD = acceptConnection(listenSocketFD); // Wait for a connection to accept.
        if (children > 5)                                           // Check if there are too many children.
        {
            close(establishedConnectionFD);
            printf("Maximum connections. Try again later\n");
            fflush(stdin);
            continue;
        }

        pid_t childThread = fork(); // Create a forked process.
        if (childThread == -1)      // Check if it forked properly.
            error("Fork is bork!");

        // Child process path.
        if (childThread == 0)
        {
            sleep(2);                                       // Immediately call sleep per the requirements.
            children++;                                     // Increment the number of children used
            close(listenSocketFD);                          // Close the listening socket
            handleIncomingRequest(establishedConnectionFD); // Handle the incoming request.
            close(establishedConnectionFD);                 // Close the existing socket which is connected to the client
            exit(0);
        }
        close(establishedConnectionFD); // Close the existing socket which is connected to the client
    }

    close(listenSocketFD); // Close the listening socket
    return 0;
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
} // Error function used for reporting issues

void catchSIGCHLD(int signo)
{
    waitpid(-1, &childStatus, 0);
    if (children > 0)
        children--;
}

void setupSignalHandler()
{
    // Signal handler for SIGCHILD
    action_SIGCHILD.sa_handler = catchSIGCHLD;
    action_SIGCHILD.sa_flags = SA_RESTART;
    sigfillset(&action_SIGCHILD.sa_mask);
    sigaction(SIGCHLD, &action_SIGCHILD, NULL);
}

// Set up the socket to listen for incoming requests.
int setupSocketAndListen(char *portNumber)
{
    int listenSocketFD;
    struct sockaddr_in serverAddress;

    // Set up the address struct for this process (the server)
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    serverAddress.sin_family = AF_INET;                          // Create a network-capable socket
    serverAddress.sin_port = htons(atoi(portNumber));            // Store the port number
    serverAddress.sin_addr.s_addr = INADDR_ANY;                  // Any address is allowed for connection to this process

    // Set up the socket
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (listenSocketFD < 0)
        error("ERROR opening socket");

    // Enable the socket to begin listening
    if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
        error("ERROR on binding");
    listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections

    return listenSocketFD;
}

// Accept the connection.
int acceptConnection(int listenSocketFD)
{
    // Setup variables.
    int establishedConnectionFD;
    socklen_t sizeOfClientInfo;
    struct sockaddr_in clientAddress;

    // Accept a connection, blocking if one is not available until one connects
    sizeOfClientInfo = sizeof(clientAddress);                                                               // Get the size of the address for the client that will connect
    establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
    if (establishedConnectionFD < 0)
        error("ERROR on accept");
    return establishedConnectionFD;
}

// Gets the oldest file for a given user and stores the file name in fileName.
// Taken previously from my assignment 2.
void getOldestFile(char *user, char *fileName)
{
    struct dirent *de; // Pointer for directory entry
    int oldestTimeStamp = -1;
    struct stat sb;
    DIR *dr;

    // Open the current directory.
    dr = opendir(".");
    // Read files in the directory.
    while ((de = readdir(dr)) != NULL)
    {
        // Look for files with the user's name.
        if (strstr(de->d_name, user))
        {
            // Store stats in the stat buffer.
            stat(de->d_name, &sb);

            // Check if this is the first time running or compare time stamps.
            if (oldestTimeStamp == -1 || oldestTimeStamp > (int)sb.st_mtime)
            {
                oldestTimeStamp = (int)sb.st_mtime;
                strcpy(fileName, de->d_name);
            }
        }
    }
    closedir(dr);
}

// Opens a file and stores the name in store.
void openAndStoreContents(char *fileName, char **store)
{
    char *line = NULL;
    size_t len = 0;
    FILE *file;

    file = fopen(fileName, "r");
    // Check for errors.
    if (file == NULL)
    {
        return;
    }
    getline(&line, &len, file);
    line[strcspn(line, "\n")] = '\0'; // Remove the trailing \n that fgets adds
    *store = line;
    fclose(file);
}

// Adapted from beej's networking guide. Sends all data in 1000 byte packets.
int sendAll(int s, char *buf)
{
    int len = strlen(buf);
    int total = 0;
    int bytesLeft = len;
    int n;
    while (total < len)
    {
        n = send(s, buf + total, 1000, 0);
        if (n == -1)
            break;
        total += n;
        bytesLeft -= n;
    }
    return n == -1 ? -1 : 0;
}

// Get and store the command and user.
void getMetaData(char *command, char *user, char *buffer)
{
    // Get the command and the user.
    strcpy(command, strtok(buffer, " "));
    strcpy(user, strtok(NULL, " "));
}

// Returns the length of the message.
int getMessageLength()
{
    // Get the length of the payload and convert it to an integer.
    char *length = strtok(NULL, " ");
    return atoi(length);
}

// Get the length of pid.
int getPidLength()
{
    int pid = (int)getpid();
    return snprintf(NULL, 0, "%d", pid);
}

// Receive the incoming message in 1000 byte increments and store it in the message variable.
void readIncomingMessage(char *message, int messageLength, int establishedConnectionFD)
{
    char buffer[1001];
    int charsRead;
    memset(buffer, '\0', sizeof(buffer));

    strcpy(message, strtok(NULL, "\0"));

    // Loop until the entire message is received if it wasn't received in the first packet that was sent.
    while (messageLength > strlen(message))
    {
        // Receive another thing of stuff.
        memset(buffer, '\0', sizeof(buffer));
        charsRead = recv(establishedConnectionFD, buffer, 1000, 0); // Read the client's message from the socket
        if (charsRead < 0)
            error("ERROR reading from socket");

        // Concat that just came in with the message that we already have and increment the counter.
        strcat(message, buffer);
    }
}

// Sets the file name.
void setFileName(char *fileName, char *user)
{
    memset(fileName, '\0', sizeof(fileName));
    sprintf(fileName, "%s.%d", user, (int)getpid());
}

// Sets the message.
void setFormattedMessage(char *formattedMessage, char *message)
{
    memset(formattedMessage, '\0', sizeof(formattedMessage));
    sprintf(formattedMessage, "%s\n", message);
}

// Writes the incoming message to a file.
void writeEncryptedMessage(char *fileName, char *formattedMessage)
{
    // Open the file, check if the file was actually opened, and then write the message in it.
    int file_descriptor = open(fileName, O_WRONLY | O_CREAT, 0600);
    if (file_descriptor < 0)
        error("File could not be opened.");
    write(file_descriptor, formattedMessage, strlen(formattedMessage) * sizeof(char));
    close(file_descriptor);
}

// Prints out the file path to stdout..
void printFilePath(char *fileName)
{
    // Get the current working directory, and then append the file that was just written.
    // Source: https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
    char cwd[PATH_MAX];
    memset(cwd, '\0', sizeof(cwd));
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd() error");
        exit(1);
    }
    sprintf(cwd, "%s/%s", cwd, fileName);
    printf("%s\n", cwd);
    fflush(stdin);
}

// Receive the message and write it to a file.
void handlePOST(int establishedConnectionFD, char *user)
{
    // Setup vavriables
    int charsRead,
        pidLength = getPidLength(),
        messageLength = getMessageLength();

    char message[messageLength + 1];
    memset(message, '\0', sizeof(message));

    readIncomingMessage(message, messageLength, establishedConnectionFD); // Reads the incoming message 1000 bytes at a time until the agreed
                                                                          // upon message length is read completely.
    char fileName[strlen(user) + pidLength + 1];                          // Format the file name to [user].[pid]
    setFileName(fileName, user);

    char formattedMessage[strlen(message) + 2];
    setFormattedMessage(formattedMessage, message);    // Format the message. Add the newline character to the end.
    writeEncryptedMessage(fileName, formattedMessage); // Write the encrypted message to a file.
    printFilePath(fileName);                           // Print out the file path.
}

// Set the payload with either the ciphertext and metadata or an error message.
void setPayload(char *payload, int messageLength)
{
    memset(payload, '\0', sizeof(payload));
    ciphertext != NULL ? sprintf(payload, "%d %s", messageLength, ciphertext) : sprintf(payload, "%s", "error ");
}

// Get the oldest file a user has and send it back to the client.
void handleGET(int establishedConnectionFD, char *user)
{
    // Setup vavriables
    int messageLength, charsSent;
    char buffer[1001], fileName[256];
    memset(buffer, '\0', sizeof(buffer));
    memset(fileName, '\0', sizeof(fileName));

    getOldestFile(user, fileName);
    openAndStoreContents(fileName, &ciphertext); // Read the file and store the contents of the file in ciphertext.
    if (fileName != NULL)                        // Delete the file if there is a file to delete.
        remove(fileName);

    messageLength = ciphertext != NULL ? strlen(ciphertext) : strlen("error "); // Create the payload to be sent back
    char payload[messageLength + 20];                                           // to OTP
    setPayload(payload, messageLength);

    charsSent = sendAll(establishedConnectionFD, payload); // Continuously send OTP 1000 byte packets of information
    if (charsSent < 0)                                     // until all the data as been sent.
        error("ERROR writing to socket");

    // Free memory.
    if (ciphertext != NULL)
        free(ciphertext);
}

// Reads the metadata sent and routes the process accordingly (get vs post.).
void handleIncomingRequest(int establishedConnectionFD)
{
    // Setup vavriables
    int messageLength, charsRead, pidLength = getPidLength();
    char command[5], user[50], buffer[1001];
    memset(user, '\0', sizeof(user));
    memset(command, '\0', sizeof(command));
    memset(buffer, '\0', sizeof(buffer));

    charsRead = recv(establishedConnectionFD, buffer, 1000, 0); // Get the message from the client.
    if (charsRead < 0)                                          // This information should be metadata on the encrypted message.
        error("ERROR reading from socket");                     // Read the client's message from the socket
    getMetaData(command, user, buffer);                         // Get metadata.

    // Route request to either post of get.
    if (strcmp(command, "post") == 0)
        handlePOST(establishedConnectionFD, user);
    if (strcmp(command, "get") == 0)
        handleGET(establishedConnectionFD, user);
}
