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
    struct room *connections[6];
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

int main(void)
{
    // Get most recent directory name.
    getRoomsDirectory();
    // create map.
    createMap();
    printf("%s\n%s\n%d\n", rooms[0].name, rooms[0].type, rooms[0].numberOfConnections);
    int i;
    for (i = 0; i < rooms[0].numberOfConnections; i++)
    {
        printf("%s\n", rooms[0].connections[i]);
        free(rooms[0].connections[i]);
    }

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
    DIR *dr = opendir(roomsDirectory);
    struct dirent *de;
    char readBuffer[128];
    FILE *file;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    chdir(roomsDirectory);

    // Reading the directory.
    while ((de = readdir(dr)) != NULL)
    {
        int i = 0;
        // Look for specific files in the directory.
        // I've added startRoom, mid and endRoom as
        // file postfixes to look for.
        if (strstr(de->d_name, "startRoom"))
        {
            file = fopen(de->d_name, "r");

            // Reads the first line.
            while (getline(&line, &len, file) > 0)
            {
                char t1[10];
                char t2[10];
                char data[20];
                memset(data, '\0', sizeof(data));
                memset(t1, '\0', sizeof(t1));
                memset(t2, '\0', sizeof(t2));

                sscanf(line, "%s %s %s", t1, t2, data);
                // If the line is the first line.
                if (strcmp(t2, "NAME:") == 0)
                {
                    strcpy(rooms[0].name, data);
                }
                // Get the connections.
                else if (strcmp(t1, "CONNECTION") == 0)
                {
                    printf("hi\n");
                    rooms[0].connections[i] = (char *)calloc(strlen(data) + 1, sizeof(char));
                    strcpy(rooms[0].connections[i], data);
                    i++;
                    rooms[0].numberOfConnections = i;
                }
            }

            // Assign type.

            rooms[0].type = "START_ROOM";
            fclose(file);
        }
    }
    free(line);
    closedir(dr);
}
