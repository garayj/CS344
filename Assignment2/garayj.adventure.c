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
    char *connections[6];
    char name[9];
    int numberOfConnections;
    char *type;
};

// Global variables.
char roomsDirectory[40];
struct room rooms[7];

//Function declarations.
void getRoomsDirectory();
void createMap();
void storeRoomData(char *, int *, int);
void freeTheHeap();

int main(void)
{
    // Get most recent directory name.
    getRoomsDirectory();
    // Create map.
    createMap();
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

void createMap()
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

void freeTheHeap()
{
    int i;
    int j;
    for (j = 0; j < 7; j++)
    {
        printf("Room: %s\n", rooms[j].name);
        printf("Connections: %d\n", rooms[j].numberOfConnections);

        for (i = 0; i < rooms[j].numberOfConnections; i++)
        {
            printf("Connection: %s\n", rooms[j].connections[i]);
            free(rooms[j].connections[i]);
        }
    }
}