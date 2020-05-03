#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>

// Room Struct
struct room
{
    struct room *connectingRoom[6];
    char *connections[6];
    char name[9];
    int numberOfConnections;
    char *type;
};

// Create the bool type.
typedef enum
{
    false,
    true
} bool;

// Global variables.
char roomsDirectory[40];
struct room rooms[7];
char *path[100];
struct room *currentRoom;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Function declarations.
void createThreadToStoreTime();
void *storeTime();
void readAndPrintTime();
void getRoomsDirectory();
void createRoomsFromFiles();
void storeRoomData(char *, int *, int);
void linkRooms();
void play();
void printGamePrompt();
void freeTheHeap();

int main(void)
{
    // Get most recent directory name.
    getRoomsDirectory();
    // Create room structs from the files and store in global rooms variable.
    createRoomsFromFiles();
    // Stitches the rooms together.
    linkRooms();
    // Starts the game cycle.
    play();

    freeTheHeap();
    return 0;
}

// Get the directory with the most recent set of rooms. Stores the
// name of the directory in roomsDirectory global variable.
void getRoomsDirectory()
{
    struct dirent *de; // Pointer for directory entry
    DIR *dr = opendir(".");
    time_t mostRecentTimeStamp = NULL;
    struct stat sb;
    // Read the directory.
    while ((de = readdir(dr)) != NULL)
    {
        // Look for files that have my OSU ID and ".rooms" in them.
        if (strstr(de->d_name, "garayj.rooms"))
        {
            // Store stats in the stat buffer.
            stat(de->d_name, &sb);
            // If this is the first time running, it's the most recent.
            if (mostRecentTimeStamp < sb.st_mtime)
            {
                mostRecentTimeStamp = sb.st_mtime;
                strcpy(roomsDirectory, de->d_name);
            }
        }
    }
    closedir(dr);
}

void createRoomsFromFiles()
{
    // Set up needed variables.
    int j = 0;         // j keeps track of which "middle" room I'm on.
    struct dirent *de; // Directory entry poin

    // Open the current directory, navigate to the correct directory, and
    // read the directory while it is not null.
    DIR *dr = opendir(roomsDirectory);
    chdir(roomsDirectory);
    while ((de = readdir(dr)) != NULL)
    {
        char *line = NULL;
        int i = 0;
        // Check if the file is one of the room files.
        if (strstr(de->d_name, "startRoom") || strstr(de->d_name, "mid") || strstr(de->d_name, "endRoom"))
        {
            size_t len = 0;
            ssize_t nread;
            if (strstr(de->d_name, "mid"))
                j++;
            // Open the file and start reading the contents.
            FILE *file = fopen(de->d_name, "r");
            while (getline(&line, &len, file) > 0)
            {
                // If it's the start room, create the room in rooms[0] so that the start
                // room will always be at index 0.
                if (strstr(de->d_name, "startRoom"))
                {
                    storeRoomData(line, &i, 0);
                    rooms[0].type = "START_ROOM";
                }
                // If it's a middle room, increment j and place the new room at index j.
                else if (strstr(de->d_name, "mid"))
                {
                    storeRoomData(line, &i, j);
                    rooms[j].type = "MID_ROOM";
                }
                // If it's the end room, create the room in rooms[6] so that the end
                // room will always be at index 6.
                else
                {
                    storeRoomData(line, &i, 6);
                    rooms[6].type = "END_ROOM";
                }
            }
            fclose(file);
        }
        // Free the memory at line before another use/the end of this file.
        free(line);
    }
    chdir("..");
    // Close out the opened directory.
    closedir(dr);
}

// Takes the line read in from the file and adds that lines data to the appropriate room.
void storeRoomData(char *line, int *i, int index)
{
    // Declare variables.
    char t1[10];
    char t2[10];
    char data[20];

    // Clear them out to null terminators.
    memset(data, '\0', sizeof(data));
    memset(t1, '\0', sizeof(t1));
    memset(t2, '\0', sizeof(t2));

    // Split the line into 3 variables.
    sscanf(line, "%s %s %s", t1, t2, data);

    // If the line is the first line, pull out the name of the room.
    if (strcmp(t2, "NAME:") == 0)
        strcpy(rooms[index].name, data);

    // Else it a connection and we loop over and get the names of the rooms connected to
    // this room. Allocate memory for the names, copy the name into the connecitons property,
    // and increment the number of connections that the room has.
    else if (strcmp(t1, "CONNECTION") == 0)
    {
        rooms[index].connections[*i] = (char *)calloc(strlen(data) + 1, sizeof(char));
        strcpy(rooms[index].connections[*i], data);
        (*i)++;
        rooms[index].numberOfConnections = *i;
    }
}
void linkRooms()
{
    int i;
    int j;
    int k;
    // For every room in our global "rooms" variable.
    for (i = 0; i < 7; i++)
    {
        // Loop over all of the connections (names of rooms) for a given room.
        for (j = 0; j < rooms[i].numberOfConnections; j++)
        {
            // Loop through all of the rooms looking for a name match.
            for (k = 0; k < 7; k++)
            {
                // Skip if I'm looking at the same room.
                if (k == i)
                    continue;
                // If there is a match, link the rooms.
                if (strcmp(rooms[i].connections[j], rooms[k].name) == 0)
                {
                    rooms[i].connectingRoom[j] = &rooms[k];
                    break;
                }
            }
        }
    }
}

void readAndPrintTime()
{
    // Setup variables to print out the time.
    char fileBuffer[40];
    memset(&fileBuffer, '\0', 40);
    char *line = NULL;
    size_t len = 0;

    // Open file, read the line, printit out, close the file,
    // and free the line.
    FILE *file = fopen("currentTime.txt", "r");
    getline(&line, &len, file);
    printf("%s\n\n", line);
    fclose(file);
    free(line);
}

void play()
{
    // Init/Define variables.
    bool isGameOver = false;
    int numberOfMoves = 0;
    size_t len = 0;
    ssize_t nread;
    // Set the current room to the start room.
    currentRoom = &(rooms[0]);

    // Loop until the winning conditions are met.
    while (!isGameOver)
    {
        // Set up variables.
        char *line = NULL;
        printGamePrompt();
        // Get input from user.
        int numCharsEntered = getline(&line, &len, stdin);
        // Replace the newline with a null terminator and then print a newline.
        line[numCharsEntered - 1] = '\0';
        printf("\n");

        // Check if it's the string "time"
        if (strcmp(line, "time") == 0)
        {
            createThreadToStoreTime();
            readAndPrintTime();
        }
        else
        {
            // Check if that is a location in the list.
            int j;
            int k = currentRoom->numberOfConnections;
            for (j = 0; j < k; j++)
            {
                // Compare the stdin to all the room names. If a match is found,
                // move the current room pointer to the new room, add the room name
                // to the path list, increment the number of moves and break out of
                // the loop.
                if (strcmp(line, currentRoom->connectingRoom[j]->name) == 0)
                {
                    currentRoom = currentRoom->connectingRoom[j];
                    path[numberOfMoves] = currentRoom->name;
                    numberOfMoves++;
                    break;
                }
            }

            // Check if the user is at the end room.
            if (currentRoom->type == "END_ROOM")
            {
                isGameOver = true;
                printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS! YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n", numberOfMoves);
                int n;
                for (n = 0; n < numberOfMoves; n++)
                    printf("%s\n", path[n]);
            }

            // If the loop (j) has reached the end of the loop (j == k) without finding a match,
            // print the error message.
            else if (j == k)
                printf("HUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");
        }
        free(line);
    }
}

void printGamePrompt()
{
    int i;
    char connectionString[90];
    memset(connectionString, '\0', sizeof(connectionString));

    // Print current location.
    printf("CURRENT LOCATION: %s\n", currentRoom->name);

    // Prints out the rooms connected to the current room.

    // Copy the first name into connectionString,
    strcpy(connectionString, currentRoom->connections[0]);

    // Loop over the rest of the connections.
    for (i = 1; i < currentRoom->numberOfConnections; i++)
    {
        // Allocate memory on the heap for creating a new formatted string.
        char *roomName = calloc(11, sizeof(char));
        // Format the string.
        snprintf(roomName, 11, ", %s", currentRoom->connectingRoom[i]->name);
        // Concatenate it with connectionString.
        strcat(connectionString, roomName);
        // Free memory.
        free(roomName);
    }
    // After loop, add a period.
    strcat(connectionString, ".");
    printf("POSSIBLE CONNECTIONS: %s\n", connectionString);
    printf("WHERE TO? >");
}

void createThreadToStoreTime()
{
    int resultInt;
    pthread_t myThreadID;

    resultInt = pthread_create(
        &myThreadID,
        NULL,
        storeTime,
        NULL);
    resultInt = pthread_join(myThreadID, NULL);
}

void *storeTime()
{
    // Set up variables for strftime and lock.
    pthread_mutex_lock(&lock);
    char timeBuffer[40];
    size_t size = 40;
    char *format = "%l:%M %p, %A, %B %d, %Y";
    time_t t = time(NULL);
    struct tm *tm;

    // Clear buffer and get the time.
    memset(&timeBuffer, '\0', 40);
    tm = localtime(&t);
    strftime(timeBuffer, size, format, tm);

    // Open file, write the time, close the file and unlock the thread.
    FILE *file = fopen("currentTime.txt", "w");
    fwrite(timeBuffer, sizeof(char), sizeof(timeBuffer), file);
    fclose(file);
    pthread_mutex_unlock(&lock);
}

void freeTheHeap()
{
    // Delete lock and clean up memory.
    pthread_mutex_destroy(&lock);
    int i;
    int j;
    for (j = 0; j < 7; j++)
        for (i = 0; i < rooms[j].numberOfConnections; i++)
            free(rooms[j].connections[i]);
}