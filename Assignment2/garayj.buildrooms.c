#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>

#define MAX_ROOMS 7

// Create the bool type.
typedef enum
{
  false,
  true
} bool;

typedef enum
{
  START_ROOM,
  MID_ROOM,
  END_ROOM
} roomType;
// Struct for a room.
struct room
{
  char *name;
  roomType type;
  int numberOfConnections;
  struct room *connections[6];
};

// Struct for all the rooms.
struct rooms
{
  struct room *startRoom;
  struct room *mid1;
  struct room *mid2;
  struct room *mid3;
  struct room *mid4;
  struct room *mid5;
  struct room *endRoom;
};
void initRoom(struct room **, roomType, char *[], int *);
char *pickRandomName(char *[], int *);
bool IsGraphFull(struct rooms *);
bool IsSameRoom(struct room *, struct room *);
void ConnectRoom(struct room *, struct room *);
bool ConnectionAlreadyExists(struct room *, struct room *);
bool CanAddConnectionFrom(struct room *);
void AddRandomConnection(struct rooms *);
void writeToFile(struct room *, char *, char *);

struct room *GetRandomRoom();
// List of room names.
const char *roomNames[10] = {"Normal", "Grass", "Fire", "Water", "Electric", "Dragon", "Psychic", "Ice", "Rock", "Bug"};

int main()
{
  // Seed for random.
  srand(time(0));

  // Create a directory name garayj.rooms.processID
  char *dirName = calloc(30, sizeof(char));
  int pid = getpid();                            // Get the process id.
  snprintf(dirName, 30, "garayj.rooms.%d", pid); // puts string into buffer
  mkdir(dirName);

  // List of room names that will be used. Initialize the array to be blank.
  char *usedRoomNames[MAX_ROOMS];
  int usedRoomNamesSize = 0;
  int i;
  for (i = 0; i < MAX_ROOMS; i++)
  {
    usedRoomNames[i] = calloc(9, sizeof(char));
  }
  int numberOfUsedNames = 0;

  // Create a structure with all the rooms needed for the game and init them
  // with the appropriate types, randomized names, and number of connections.
  struct rooms *allTheRooms = (struct rooms *)malloc(sizeof(struct rooms));
  initRoom(&allTheRooms->startRoom, START_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->mid1, MID_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->mid2, MID_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->mid3, MID_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->mid4, MID_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->mid5, MID_ROOM, usedRoomNames, &numberOfUsedNames);
  initRoom(&allTheRooms->endRoom, END_ROOM, usedRoomNames, &numberOfUsedNames);
  // Create all connections in graph
  while (IsGraphFull(allTheRooms) == false)
  {
    AddRandomConnection(allTheRooms);
  }
  writeToFile(allTheRooms->startRoom, dirName, "startRoom");
  writeToFile(allTheRooms->mid1, dirName, "mid1");
  writeToFile(allTheRooms->mid2, dirName, "mid2");
  writeToFile(allTheRooms->mid3, dirName, "mid3");
  writeToFile(allTheRooms->mid4, dirName, "mid4");
  writeToFile(allTheRooms->mid5, dirName, "mid5");
  writeToFile(allTheRooms->endRoom, dirName, "endRoom");

  // Free all the memories.
  free(allTheRooms->startRoom);
  free(allTheRooms->mid1);
  free(allTheRooms->mid2);
  free(allTheRooms->mid3);
  free(allTheRooms->mid4);
  free(allTheRooms->mid5);
  free(allTheRooms->endRoom);
  int j;
  for (j = 0; j < MAX_ROOMS; j++)
  {
    free(usedRoomNames[j]);
  }
  free(allTheRooms);
  free(dirName);
  return 0;
}

void writeToFile(struct room *room, char *directoryName, char *fileName)
{
  // Get the type of the room.
  char *type;
  switch (room->type)
  {
  case 0:
    type = "START_ROOM";
    break;
  case 1:
    type = "MID_ROOM";
    break;
  case 2:
    type = "END_ROOM";
    break;
  default:
    break;
  }

  char *file = calloc(40, sizeof(char));
  snprintf(file, 40, "%s/%s_%s", directoryName, room->name, fileName);
  int file_descriptor = open(file, O_WRONLY | O_CREAT, 0600);
  free(file);

  // Write room name to file.
  char *roomName = calloc(40, sizeof(char));
  snprintf(roomName, 40, "ROOM NAME: %s\n", room->name);
  write(file_descriptor, roomName, strlen(roomName) * sizeof(char));
  free(roomName);

  // Write connections to the file.
  int i;
  for (i = 0; i < room->numberOfConnections; i++)
  {
    char *connection = calloc(40, sizeof(char));
    snprintf(connection, 40, "CONNECTION %d: %s\n", i + 1, room->connections[i]->name);
    write(file_descriptor, connection, strlen(connection) * sizeof(char));
    free(connection);
  }

  // Write the room type to the file.
  char *roomType = calloc(40, sizeof(char));
  snprintf(roomType, 40, "ROOM TYPE: %s\n", type);
  write(file_descriptor, roomType, strlen(roomType) * sizeof(char));
  free(roomType);

  close(file_descriptor);
}

void initRoom(struct room **room, roomType type, char *usedRoomNames[], int *number)
{
  *room = malloc(sizeof(struct room));
  (*room)->type = type;
  (*room)->numberOfConnections = 0;
  (*room)->name = pickRandomName(usedRoomNames, number);
  *number = *number + 1;
}

// Gets a unique and random name.
char *pickRandomName(char *usedRoomNames[], int *usedRoomNamesSize)
{
  int i;
  while (true)
  {
    // Pick a random room.
    int index = rand() % 10;

    // Flag to know if a the random name is unique.
    bool isUnique = true;
    for (i = 0; i < *usedRoomNamesSize; i++)
    {

      // Check if the name has already been chosen for a different room.
      if (strcmp(usedRoomNames[i], roomNames[index]) == 0)
      {
        isUnique = false;
        break;
      }
    }
    // If the it unqiue or if it's the first room name, return the name.
    if (isUnique || *usedRoomNamesSize == 0)
    {
      strcpy(usedRoomNames[*usedRoomNamesSize], roomNames[index]);
      return usedRoomNames[*usedRoomNamesSize];
    }
  }
}

// Returns true if all rooms have 3 to 6 outbound connections, false otherwise
bool IsGraphFull(struct rooms *rooms)
{
  // Check to see if all the rooms have 3 connections.
  return rooms->startRoom->numberOfConnections >= 3 &&
                 rooms->mid1->numberOfConnections >= 3 &&
                 rooms->mid2->numberOfConnections >= 3 &&
                 rooms->mid3->numberOfConnections >= 3 &&
                 rooms->mid4->numberOfConnections >= 3 &&
                 rooms->mid5->numberOfConnections >= 3 &&
                 rooms->endRoom->numberOfConnections >= 3
             ? true
             : false;
}

// Returns a random Room, does NOT validate if connection can be added
struct room *GetRandomRoom(struct rooms *rooms)
{
  int randomIndex = rand() % 7;
  switch (randomIndex)
  {
  case 0:
    return rooms->startRoom;
    break;
  case 1:
    return rooms->mid1;
    break;
  case 2:
    return rooms->mid2;
    break;
  case 3:
    return rooms->mid3;
    break;
  case 4:
    return rooms->mid4;
    break;
  case 5:
    return rooms->mid5;
    break;
  case 6:
    return rooms->endRoom;
  default:
    break;
  }
}

// Adds a random, valid outbound connection from a Room to another Room
void AddRandomConnection(struct rooms *rooms)
{
  struct room *A;
  struct room *B;

  while (true)
  {
    A = GetRandomRoom(rooms);

    if (CanAddConnectionFrom(A) == true)
      break;
  }

  do
  {
    B = GetRandomRoom(rooms);
  } while (CanAddConnectionFrom(B) == false || IsSameRoom(A, B) == true || ConnectionAlreadyExists(A, B) == true);

  ConnectRoom(A, B); // TODO: Add this connection to the real variables,
  ConnectRoom(B, A); //  because this A and B will be destroyed when this function terminates
}

// Returns true if a connection can be added from Room x (< 6 outbound connections), false otherwise
bool CanAddConnectionFrom(struct room *x)
{
  return x->numberOfConnections < 6 ? true : false;
}
// Returns true if a connection from Room x to Room y already exists, false otherwise
bool ConnectionAlreadyExists(struct room *x, struct room *y)
{
  int i;
  bool connectionExists = false;
  for (i = 0; i < x->numberOfConnections; i++)
  {
    if (x->connections[i] == y)
    {
      connectionExists = true;
    }
  }
  return connectionExists;
}

// Connects Rooms x and y together, does not check if this connection is valid
void ConnectRoom(struct room *x, struct room *y)
{
  x->connections[x->numberOfConnections] = y;
  x->numberOfConnections++;
}

// Returns true if Rooms x and y are the same Room, false otherwise
bool IsSameRoom(struct room *x, struct room *y)
{
  if (x->name == y->name)
  {
    return true;
  }
  else
  {
    return false;
  }
}
