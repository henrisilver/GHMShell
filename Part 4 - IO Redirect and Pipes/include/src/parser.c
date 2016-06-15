#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

char * isParseRequired(char *string);
char ** parseCommand(char *command, int *num_commands);

int main(int argc, char const *argv[])
{
    //char *command = calloc(100,sizeof(char));
    char command[100] = "jobs pepperoni|peneira viadinho|giuka sergey|";
    int num_commands = 0;
    
    if (isParseRequired(command) == NULL){
        printf("%s\n", command);
    } else {
        parseCommand(&command[0],&num_commands);
        if (DEBUG == 1) {
            printf("Numero de comandos: %d\n", num_commands);
        }
    }

    return 0;
}

// Checks if there is any '|'' char
// Returns NULL in case there isn't any
char * isParseRequired(char *string){
    return strchr(string,'|');
}

// Split the received command into sub-commands
// Returns a matrix where each line corresponds to a sub-command
// Empty sub-commands are not handled
char ** parseCommand(char *command, int *num_commands) {

    // Size of each sub-command
    int size = 0;

    // J is used to iterate through each line in the commands matrix
    int j = 0;

    // i is used to iterate through each character of the command
    int i = 0;

    // Matrix that holds the parsed sub-commands
    char **commands = NULL;

    // Temporary string used as a helper in the parsing process
    char temp_command[100];

    // There is always a first command followed by a unknown number
    // of commands. So *num_commands is initialized to 1.
    *num_commands = 1;

    // Determines the amount of sub-commands
    while (command[i++] != '\0') {
        if(command[i] == '|') {
            (*num_commands)++;
        }
    }

    // Allocation of the sub-commands matrix
    commands = (char **) calloc(*num_commands,sizeof(char *));

    i = 0;
    // Actual parsing for-loop
    for (j = 0 ; j < *num_commands ; j++) {
        size = 0;
        // (char) 123 equals to '|'
        while (command[i] != (char)124 && command[i] != '\0') {
            temp_command[size++] = command[i++];
        }
        temp_command[size] = '\0';
        i++;

        // This loop removes any white spaces after a pipe char ('|')
        while(command[i] == ' ')
            i++;

        commands[j] = (char *)calloc(size+1, sizeof(char));
        strcpy(commands[j],temp_command);

        if (DEBUG == 1) {
            printf("%s\n", commands[j]);
        }

        bzero(&temp_command[0], sizeof(char));
    }

    return commands;
}