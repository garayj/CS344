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

typedef enum
{
    false,
    true
} bool;

const char ALLOWED_CHARACTERS[27] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' '};

char *key, *plaintext, *ciphertext = NULL;

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

// Attempts to open files. Errors out if n
void openAndStoreContents(char *fileName, char **store)
{
    FILE *file;
    char *line = NULL;
    size_t len = 0;

    file = fopen(fileName, "r");
    // Check for errors.
    if (file == NULL)
    {
        char errorMsg[256];
        sprintf(errorMsg, "Error opening %s file", fileName);
        error(errorMsg);
    }

    getline(&line, &len, file);
    line[strcspn(line, "\n")] = '\0'; // Remove the trailing \n that fgets adds
    *store = line;
    fclose(file);
}

void encrypt()
{
    int length = strlen(plaintext);
    char encryptedText[length + 1];
    memset(encryptedText, '\0', sizeof(encryptedText));
    ciphertext = calloc(length + 1, sizeof(char));

    int i;
    for (i = 0; i < length; i++)
    {
        int currentChar = (int)plaintext[i];
        int currentKeyChar = (int)key[i];
        if ((currentChar < 65 && currentChar != 32) || currentChar > 90)
        {
            fprintf(stderr, "Error: input contains bad characters\n");
            freeMemory();
            exit(1);
        }
        currentChar = currentChar == 32 ? 26 : currentChar - 65;
        currentKeyChar = currentKeyChar == 32 ? 26 : currentKeyChar - 65;
        encryptedText[i] = ALLOWED_CHARACTERS[(currentChar + currentKeyChar) % 27];
    }
    strcpy(ciphertext, encryptedText);
}
void decrypt()
{
    int length = strlen(ciphertext);
    char decryptedText[length + 1];
    memset(decryptedText, '\0', sizeof(decryptedText));

    int i;
    for (i = 0; i < length; i++)
    {
        int currentChar = (int)ciphertext[i];
        int currentKeyChar = (int)key[i];

        currentChar = currentChar == 32 ? 26 : currentChar - 65;
        currentKeyChar = currentKeyChar == 32 ? 26 : currentKeyChar - 65;
        int decryptedChar = currentChar - currentKeyChar > -1 ? currentChar - currentKeyChar : currentChar - currentKeyChar + 27;
        decryptedText[i] = ALLOWED_CHARACTERS[decryptedChar % 27];
    }
    printf("%s\n", decryptedText);
}
int sendAll(int s, char *buf, int *len)
{
    int total = 0;
    int bytesLeft = *len;
    int n;
    int counter = 0;
    while (total < *len)
    {
        printf("%d\n", counter);
        counter++;
        n = send(s, buf + total, bytesLeft, 0);
        if (n == -1)
            break;
        total += n;
        bytesLeft -= n;
    }
    *len = total;
    return n == -1 ? -1 : 0;
}

int main(int argc, char *argv[])
{
    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    struct hostent *serverHostInfo;
    char buffer[256];
    char *user, *messageFileName, *keyFileName, *command;

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

    // Check if we should be doing a post.
    if (isValidPost)
    {
        // Set the plaintext file and key.
        messageFileName = argv[3];
        keyFileName = argv[4];

        // Open, read and store the contents of each file.
        openAndStoreContents(keyFileName, &key);
        openAndStoreContents(messageFileName, &plaintext);
        if (strlen(key) < strlen(plaintext))
        {
            fprintf(stderr, "Error: key ‘%s’ is too short\n", keyFileName);
            freeMemory();
            exit(1);
        }
        // Encrypt message into ciphertext.
        encrypt();
    }

    // Check if we should be doing a get.
    if (isValidGet)
    {
        keyFileName = argv[3];
        openAndStoreContents(keyFileName, &key);
        openAndStoreContents("jose.4370", &ciphertext);
        decrypt();
    }

    // Set up the server address struct
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    serverAddress.sin_family = AF_INET;                          // Create a network-capable socket
    serverAddress.sin_port = htons(portNumber);                  // Store the port number
    serverHostInfo = gethostbyname("localhost");                 // Convert the machine name into a special form of address
    if (serverHostInfo == NULL)
    {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        exit(1);
    }
    memcpy((char *)&serverAddress.sin_addr.s_addr, (char *)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

    // Set up the socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (socketFD < 0)
        error("CLIENT: ERROR opening socket");

    // Connect to server
    if (connect(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
    {
        error("CLIENT: ERROR connecting");
    }

    int payloadLength = strlen(command) + 2 + strlen(user);
    if (isValidPost)
    {
        payloadLength += strlen(ciphertext);
        payloadLength += 1;
    }

    char payload[payloadLength];
    memset(payload, '\0', sizeof(payload));
    if (isValidPost)
    {
        int ciphertextLength = strlen(ciphertext);
        sprintf(payload, "%s %s %d %s", command, user, ciphertextLength, ciphertext);
    }
    else
        sprintf(payload, "%s %s", command, user);
    int paylen = strlen(payload);

    // Send message to server
    charsWritten = sendAll(socketFD, payload, &paylen); // Write to the server
    if (charsWritten < 0)
        error("CLIENT: ERROR writing to socket");

    // // Get return message from server
    // memset(payload, '\0', sizeof(payload));                  // Clear out the buffer again for reuse
    // charsRead = recv(socketFD, payload, sizeof(payload), 0); // Read data from the socket, leaving \0 at end
    // if (charsRead < 0)
    //     error("CLIENT: ERROR reading from socket");
    // printf("CLIENT: I received this from the server: \"%s\"\n", payload);

    close(socketFD); // Close the socket
    freeMemory();
    return 0;
}