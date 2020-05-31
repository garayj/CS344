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
#include <limits.h>

// Boolean definition.
typedef enum
{
    false,
    true
} bool;

int childStatus = -99;

void error(const char *msg)
{
    perror(msg);
    exit(1);
} // Error function used for reporting issues

int main(int argc, char *argv[])
{
    int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
    socklen_t sizeOfClientInfo;
    char buffer[100001];
    struct sockaddr_in serverAddress, clientAddress;

    if (argc < 2)
    {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    } // Check usage & args

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
        // Create a forked process.
        pid_t childThread = fork();
        // If the creation of the thread is corrupted somehow,
        // shoot out an error.
        if (childThread == -1)
        {
            perror("Fork is bork!");
            exit(1);
        }
        // Child process path.
        if (childThread == 0)
        {
            // Immediately call sleep per the requirements.
            sleep(2);

            // Setup vavriables
            char *saveptr, *command, *user, *message = NULL, *length;
            int messageLength, counter,
                pidInt = (int)getpid(),
                pidLength = snprintf(NULL, 0, "%d", pidInt);

            // Get the message from the client and display it
            memset(buffer, '\0', 100001);
            charsRead = recv(establishedConnectionFD, buffer, 100000, 0); // Read the client's message from the socket
            if (charsRead < 0)
                error("ERROR reading from socket");

            command = strtok_r(buffer, " ", &saveptr);
            user = strtok_r(NULL, " ", &saveptr);

            // POST: Receives username and message via socket. Write the message to a file and print path to file (Relative?). Should crash or terminate, still should be able to read files.
            if (strcmp(command, "post") == 0)
            {
                length = strtok_r(NULL, " ", &saveptr);
                messageLength = atoi(length);
                message = strtok_r(NULL, "\0", &saveptr);
                // Write the encrypted message to a file with the name of "[user].[pid]".
                char fileName[strlen(user) + pidLength + 1];
                memset(fileName, '\0', sizeof(fileName));
                sprintf(fileName, "%s.%d", user, pidInt);
                printf("SERVER: I received this from the client: \"%s\" and \"%s\"\n", command, user);
                fflush(stdin);

                char formattedMessage[strlen(message) + 2];
                memset(formattedMessage, '\0', sizeof(formattedMessage));
                sprintf(formattedMessage, "%s\n", message);

                int file_descriptor = open(fileName, O_WRONLY | O_CREAT, 0600);
                if (file_descriptor < 0)
                    error("File could not be opened.");
                write(file_descriptor, formattedMessage, strlen(formattedMessage) * sizeof(char));
                close(file_descriptor);
                char cwd[PATH_MAX];
                memset(cwd, '\0', sizeof(cwd));
                if (getcwd(cwd, sizeof(cwd)) == NULL)
                {
                    perror("getcwd() error");
                    exit(1);
                }
                strcat(cwd, "/");
                strcat(cwd, fileName);
                printf("%s\n", cwd);
                fflush(stdin);
            }

            // Send a Success message back to the client
            // charsRead = send(establishedConnectionFD, "I am the server, and I got your message", 39, 0); // Send success back
            // if (charsRead < 0)
            //     error("ERROR writing to socket");
            exit(0);
        }
        else
        {
            close(establishedConnectionFD); // Close the existing socket which is connected to the client
            waitpid(-1, &childStatus, WNOHANG);
            printf("parent is done\n");
        }
    }
    // Else the process is the parent process.
    // The parent process continues listening for incoming connections.
    close(listenSocketFD); // Close the listening socket
    return 0;
}