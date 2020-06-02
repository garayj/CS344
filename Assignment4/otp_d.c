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

int childStatus = -99, children = 0;
struct sigaction action_SIGCHILD = {0};
char *ciphertext = NULL;

void error(const char *msg)
{
    perror(msg);
    exit(1);
} // Error function used for reporting issues
void CatchSIGCHLD(int signo)
{
    waitpid(-1, &childStatus, 0);
    if (children > 0)
        children--;
}

void getOldestFile(char *user, char *fileName)
{
    struct dirent *de; // Pointer for directory entry
    DIR *dr = opendir(".");
    int oldestTimeStamp = -1;
    struct stat sb;
    // Read the directory.
    while ((de = readdir(dr)) != NULL)
    {
        // Look for files that have my OSU ID and ".rooms" in them.
        if (strstr(de->d_name, user))
        {
            // Store stats in the stat buffer.
            stat(de->d_name, &sb);
            // If this is the first time running, it's the most recent.
            if (oldestTimeStamp == -1 || oldestTimeStamp > (int)sb.st_mtime)
            {
                oldestTimeStamp = (int)sb.st_mtime;
                strcpy(fileName, de->d_name);
            }
        }
    }
    closedir(dr);
}

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

int sendAll(int s, char *buf)
{
    int len = strlen(buf);
    int total = 0;
    int bytesLeft = len;
    int n;
    int counter = 0;
    while (total < len)
    {
        counter++;
        n = send(s, buf + total, 1000, 0);
        if (n == -1)
            break;
        total += n;
        bytesLeft -= n;
    }
    return n == -1 ? -1 : 0;
}

int main(int argc, char *argv[])
{
    int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
    socklen_t sizeOfClientInfo;
    char buffer[1001];
    struct sockaddr_in serverAddress, clientAddress;

    if (argc < 2)
    {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    } // Check usage & args

    // Signal handler for SIGCHILD
    action_SIGCHILD.sa_handler = CatchSIGCHLD;
    action_SIGCHILD.sa_flags = SA_RESTART;
    sigfillset(&action_SIGCHILD.sa_mask);
    sigaction(SIGCHLD, &action_SIGCHILD, NULL);

    // Set up the address struct for this process (the server)
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    portNumber = atoi(argv[1]);                                  // Get the port number, convert to an integer from a string
    serverAddress.sin_family = AF_INET;                          // Create a network-capable socket
    serverAddress.sin_port = htons(portNumber);                  // Store the port number
    serverAddress.sin_addr.s_addr = INADDR_ANY;                  // Any address is allowed for connection to this process

    // Set up the socket
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (listenSocketFD < 0)
        error("ERROR opening socket");

    // Enable the socket to begin listening
    if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
        error("ERROR on binding");
    listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections
    while (1)
    {
        // Accept a connection, blocking if one is not available until one connects
        sizeOfClientInfo = sizeof(clientAddress);                                                               // Get the size of the address for the client that will connect
        establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
        if (establishedConnectionFD < 0)
            error("ERROR on accept");

        // Check if there are too many children.
        if (children > 5)
        {
            close(establishedConnectionFD);
            printf("Maximum connections. Try again later\n");
            fflush(stdin);
            continue;
        }

        // Create a forked process.
        pid_t childThread = fork();

        // If the creation of the thread is corrupted somehow,
        // shoot out an error.
        if (childThread == -1)
        {
            error("Fork is bork!");
        }

        // Child process path.
        if (childThread == 0)
        {
            // Immediately call sleep per the requirements.
            sleep(2);
            // Increment the number of children used
            children++;
            close(listenSocketFD); // Close the listening socket

            // Setup vavriables
            char *saveptr, command[5], user[50], *length;
            memset(user, '\0', sizeof(user));
            memset(command, '\0', sizeof(command));
            int messageLength, counter = 0,
                               pidInt = (int)getpid(),
                               pidLength = snprintf(NULL, 0, "%d", pidInt);

            // Get the message from the client and display it
            memset(buffer, '\0', sizeof(buffer));
            charsRead = recv(establishedConnectionFD, buffer, 1000, 0); // Read the client's message from the socket
            if (charsRead < 0)
                error("ERROR reading from socket");

            // Get the command and the user.
            strcpy(command, strtok_r(buffer, " ", &saveptr));
            strcpy(user, strtok_r(NULL, " ", &saveptr));

            // POST: Receives username and message via socket. Write the message to a file and
            // print path to file (Relative?). Should crash or terminate, still should be able to read files.
            if (strcmp(command, "post") == 0)
            {
                // Get the length of the payload and convert it to an integer.
                length = strtok_r(NULL, " ", &saveptr);
                messageLength = atoi(length);

                // Create a string with enough space for the message and copy it in from the buffer.
                char message[messageLength + 1];
                memset(message, '\0', sizeof(message));
                strcpy(message, strtok_r(NULL, "\0", &saveptr));

                // Keep track of the number of characters received.
                counter += strlen(message);

                // Loop until the entire message is received if it wasn't received in the first packet that was sent.
                while (messageLength > counter)
                {
                    // Receive another thing of stuff.
                    memset(buffer, '\0', sizeof(buffer));
                    charsRead = recv(establishedConnectionFD, buffer, 1000, 0); // Read the client's message from the socket
                    if (charsRead < 0)
                        error("ERROR reading from socket");

                    // Concat that just came in with the message that we already have and increment the counter.
                    strcat(message, buffer);
                    counter += charsRead;
                }

                // Write the encrypted message to a file with the name of "[user].[pid]".
                // Format the file name.
                char fileName[strlen(user) + pidLength + 1];
                memset(fileName, '\0', sizeof(fileName));
                sprintf(fileName, "%s.%d", user, pidInt);

                // Format the message. Add the newline character to the end.
                char formattedMessage[strlen(message) + 2];
                memset(formattedMessage, '\0', sizeof(formattedMessage));
                sprintf(formattedMessage, "%s\n", message);

                // Open the file, check if the file was actually opened, and then write the message in it.
                int file_descriptor = open(fileName, O_WRONLY | O_CREAT, 0600);
                if (file_descriptor < 0)
                    error("File could not be opened.");
                write(file_descriptor, formattedMessage, strlen(formattedMessage) * sizeof(char));
                close(file_descriptor);

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
            if (strcmp(command, "get") == 0)
            {

                char fileName[256];
                memset(fileName, '\0', sizeof(fileName));
                getOldestFile(user, fileName);
                // Read the file
                // Store the contents of the file in a string.
                openAndStoreContents(fileName, &ciphertext);
                // Delete the file.
                if (fileName != NULL)
                    remove(fileName);

                if (ciphertext != NULL)
                {
                    messageLength = strlen(ciphertext);
                    char payload[strlen(ciphertext) + 20];
                    memset(payload, '\0', sizeof(payload));
                    sprintf(payload, "%d %s", messageLength, ciphertext);

                    // Send the string back.
                    charsRead = sendAll(establishedConnectionFD, payload); // Send success back
                    if (charsRead < 0)
                        error("ERROR writing to socket");
                }
                // There was no files associated with the user.
                else
                {
                    char *errorSig = "error ";
                    charsRead = send(establishedConnectionFD, errorSig, strlen(errorSig), 0);
                    if (charsRead < 0)
                        error("ERROR writing metadata to socket");
                }

                // Free memory.
                if (ciphertext != NULL)
                    free(ciphertext);
            }

            // Send a Success message back to the client
            close(establishedConnectionFD); // Close the existing socket which is connected to the client
            exit(0);
        }
        close(establishedConnectionFD); // Close the existing socket which is connected to the client
    }
    // Else the process is the parent process.
    // The parent process continues listening for incoming connections.
    close(listenSocketFD); // Close the listening socket
    return 0;
}