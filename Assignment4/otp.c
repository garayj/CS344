#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

// Bool typedef
typedef enum
{
    false,
    true
} bool;

// Constant
const char ALLOWED_CHARACTERS[27] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' '};

// Pointers needint allocated memory.
char *key, *plaintext, *ciphertext = NULL;

// Functions
int setupAndConnectSocket(int);
int sendAll(int, char *);
void decrypt();
void encrypt();
void error(const char *);
void freeMemory();
void openAndStoreContents(char *, char **);
void receive(int);
void setUpPost(char *, char *);

int main(int argc, char *argv[])
{
    // Setup most variables that are needed.
    int socketFD, portNumber, charsWritten, charsRead, payloadLength;
    char *user, *keyFileName, *command;

    // Check if the number of arguments and command type is correct.
    bool isValidGet = (argc == 5 && strcmp("get", argv[1]) == 0) ? true : false;
    bool isValidPost = (argc == 6 && strcmp("post", argv[1]) == 0) ? true : false;

    // Check usage & args
    if (!isValidGet && !isValidPost)
    {
        fprintf(stderr, "USAGE: %s get [user] [key] [port] \n", argv[0]);
        fprintf(stderr, "USAGE: %s post [user] [plaintext] [key] [port] \n", argv[0]);
        exit(1);
    }

    // Set the user and command type.
    user = argv[2];
    command = argv[1];
    portNumber = atoi(argv[argc - 1]);
    keyFileName = isValidPost ? argv[4] : argv[3];

    // Check if we should be doing a post.
    // Else open up the key file for a get.
    if (isValidPost)
        setUpPost(argv[3], keyFileName);
    else
        openAndStoreContents(keyFileName, &key);

    // Set up the connection to the server.
    socketFD = setupAndConnectSocket(portNumber);

    // Create the payload to be sent to the server.
    // Calculate the length of the payload that will be sent to the server.
    // A payload will always include the command and the user name. If the
    // command is post, include the cipher text as well.
    payloadLength = strlen(command) + 2 + strlen(user);
    if (isValidPost)
        payloadLength += strlen(ciphertext) + 1;

    // Create an array big enough for the payload.
    char payload[payloadLength];
    memset(payload, '\0', sizeof(payload));

    // Format the payload where each block of information that is send is delimited by a space.
    if (isValidPost)
        // Post sends the command, user, length of the cipher text, and the cipher text.
        sprintf(payload, "%s %s %d %s", command, user, (int)strlen(ciphertext), ciphertext);
    else
        // Get sends command and user.
        sprintf(payload, "%s %s", command, user);

    // For sending data to the server for both get and post.
    // Send message to server. Adapted sendAll from the beej networking guide.
    charsWritten = sendAll(socketFD, payload); // Write to the server
    if (charsWritten < 0)
        error("CLIENT: ERROR writing to socket");

    // Get return message from server and decrypt it.
    if (isValidGet)
    {
        receive(socketFD);
        decrypt();
    }

    close(socketFD); // Close the socket
    freeMemory();
    return 0;
}

// Checks if any of the pointers need to be freed.
void freeMemory()
{
    if (key != NULL)
        free(key);
    if (plaintext != NULL)
        free(plaintext);
    if (ciphertext != NULL)
        free(ciphertext);
}

void error(const char *msg)
{

    perror(msg);
    freeMemory();
    exit(1);
} // Error function used for reporting issues

// Open and store the contents of a file.
void openAndStoreContents(char *fileName, char **store)
{
    // Set up variables.
    FILE *file;
    char *line = NULL;
    size_t len = 0;

    // Open up the file.
    file = fopen(fileName, "r");
    // Check for errors.
    if (file == NULL)
    {
        char errorMsg[256];
        sprintf(errorMsg, "Error opening %s file", fileName);
        error(errorMsg);
    }

    // Read a line from the file, add a newline character, store it,
    // and close the file.
    getline(&line, &len, file);
    line[strcspn(line, "\n")] = '\0'; // Remove the trailing \n that fgets adds
    *store = line;
    fclose(file);
}

// Encrypts the message and store it in ciphertext.
void encrypt()
{
    // Set up variables.
    int currentChar, currentKeyChar, i, length = strlen(plaintext);
    char encryptedText[length + 1];
    memset(encryptedText, '\0', sizeof(encryptedText));
    ciphertext = calloc(length + 1, sizeof(char));

    for (i = 0; i < length; i++)
    {
        // Get the current characters and check if they are valid characters.
        currentChar = (int)plaintext[i];
        currentKeyChar = (int)key[i];
        if ((currentChar < 65 && currentChar != 32) || currentChar > 90)
        {
            fprintf(stderr, "Error: input contains bad characters\n");
            freeMemory();
            exit(1);
        }

        // Get their index values.
        currentChar = currentChar == 32 ? 26 : currentChar - 65;
        currentKeyChar = currentKeyChar == 32 ? 26 : currentKeyChar - 65;

        // Convert their values to an encrypted value.
        encryptedText[i] = ALLOWED_CHARACTERS[(currentChar + currentKeyChar) % 27];
    }
    // Do the thing.
    strcpy(ciphertext, encryptedText);
}

// Decrypts the message and prints it out to stdout.
void decrypt()
{
    // Set up variables.
    int currentChar, currentKeyChar, decryptedChar, i, length = strlen(ciphertext);
    char decryptedText[length + 1];
    memset(decryptedText, '\0', sizeof(decryptedText));

    for (i = 0; i < length; i++)
    {
        // Get the current character. If it happens to be 26 then set it 26, the last index in the array,
        // else calculate the postion of the capital letter in the array.
        currentChar = (int)ciphertext[i];
        currentKeyChar = (int)key[i];
        currentChar = currentChar == 32 ? 26 : currentChar - 65;
        currentKeyChar = currentKeyChar == 32 ? 26 : currentKeyChar - 65;

        // Use the formula to calculate the decrypted letter.
        decryptedChar = currentChar - currentKeyChar > -1 ? currentChar - currentKeyChar : currentChar - currentKeyChar + 27;
        decryptedText[i] = ALLOWED_CHARACTERS[decryptedChar % 27];
    }
    // Do the thing.
    printf("%s\n", decryptedText);
}

// Adapted from beej networking guide. Continually sends data in 1000 byte packets
// until the entire message is sent.
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

// Setup the socket and connect..
int setupAndConnectSocket(int portNumber)
{
    // Set up variables.
    struct sockaddr_in serverAddress;
    struct hostent *serverHostInfo;

    // Set up the server address struct
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    serverAddress.sin_family = AF_INET;                          // Create a network-capable socket
    serverAddress.sin_port = htons(portNumber);                  // Store the port number
    serverHostInfo = gethostbyname("localhost");                 // Convert the machine name into a special form of address
    if (serverHostInfo == NULL)
    {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        freeMemory();
        exit(1);
    }
    memcpy((char *)&serverAddress.sin_addr.s_addr, (char *)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

    // Set up the socket
    int socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (socketFD < 0)
        error("CLIENT: ERROR opening socket");

    // Connect to server
    if (connect(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
        error("CLIENT: ERROR connecting");
    return socketFD;
}

// Receives a specified amount of data back from the server and stores that data in ciphertext.
void receive(int socketFD)
{
    // Set up variables.
    char buffer[1001], *metadata, *token;
    int charsRead, length, charsReceived = 0;
    memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse

    // Receive the length of the incoming
    charsRead = recv(socketFD, buffer, 1000, 0); // Read data from the socket, leaving \0 at end
    if (charsRead < 0)
        error("CLIENT: ERROR reading from socket");

    // The first token of the data is the length of the message. If it is "error", there is
    // an error and we abort.
    metadata = strtok(buffer, " ");
    if (strcmp(metadata, "error") == 0)
    {
        fprintf(stderr, "User has no files\n");
        freeMemory();
        exit(1);
    }
    // Set the length of incoming message.
    length = atoi(metadata);

    // Get the next token and store it in ciphertext.
    token = strtok(NULL, "\0");
    ciphertext = calloc(length + 1, sizeof(char));
    strcpy(ciphertext, token);

    // Keep track of how many character have been received and copied into ciphertext.
    charsReceived = strlen(ciphertext);

    // Continue receiving data until we have all of the data.
    while (length > charsReceived)
    {
        memset(buffer, '\0', sizeof(buffer));        // Clear out the buffer again for reuse
        charsRead = recv(socketFD, buffer, 1000, 0); // Read data from the socket, leaving \0 at end
        if (charsRead < 0)
            error("CLIENT: ERROR reading from socket");

        // Concat the received data and increment the counter.
        strcat(ciphertext, buffer);
        charsReceived = strlen(ciphertext);
    }
}

// Open message and key files, check if they are compatible, and
// encrypt the message.
void setUpPost(char *messageFileName, char *keyFileName)
{
    // Open, read and store the contents of each file.
    openAndStoreContents(keyFileName, &key);
    openAndStoreContents(messageFileName, &plaintext);

    // Check if the key is long enough to encrypt the file.
    if (strlen(key) < strlen(plaintext))
    {
        fprintf(stderr, "Error: key ‘%s’ is too short\n", keyFileName);
        freeMemory();
        exit(1);
    }

    // Encrypt message into ciphertext.
    encrypt();
}
