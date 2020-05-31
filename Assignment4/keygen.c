#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char ALLOWED_CHARACTERS[27] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' '};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        perror("Wrong number of arguments\n");
        return 1;
    }
    srand(time(NULL));
    int length = atoi(argv[1]);
    int i;
    char str[length + 1];
    for (i = 0; i < length; i++)
    {
        str[i] = ALLOWED_CHARACTERS[rand() % 27];
    }
    str[length] = '\0';
    printf("%s\n", str);
    return 0;
}