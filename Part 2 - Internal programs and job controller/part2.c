/*
 * Departamento de Sistemas de Computacao
 * Instituto de Ciencias Matematicas e de Computacao
 * Universidade de Sao Paulo
 * SSC0743 - Sistemas Operacionais II
 *
 * Project 1 - 1st semester of 2016
 * Group members:
 * - Giuliano Barbosa Prado - 7961109
 * - Henrique de Almeida Machado da Silveira - 7961089
 * - Marcello de Paula Ferreira Costa - 7960690
 *
 * Project based on the code found in
 * "Writing Your Own Shell", by Hiran Ramankutty
 */

/* Including libraries */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>


/* Variable used to represent error codes. This variable is defined in
 * errno.h and is set by system call of library functions when there are
 * errors, indicating what went wrong.
 */
extern int errno;

int backgroundExec;

/* Arrays of string that will be used:
 * my_argv: represents the argv that will be passed to the command that will
 * be executed (using a custom data structure instead of having a single string
 * containing all command line arguments), typed by the user.
 * my_envp: local copy of the envp variable.
 */
static char *my_argv[100], *my_envp[100];

/* Array of strings representing PATHs to execute commands */
static char *search_path[10];

/* Function defined to deal with signals received during execution. */
void handle_signal(int signo)
{
	printf("\n[GHMSHELLRecv ] ");
	fflush(stdout);
}

/* Function used to copy the content of argv to the local structure. A
 * string array is used, where each of the of the strings is one of the
 * arguments present in the origianl argv typed by the user.
*/
void fill_argv(char *tmp_argv)
{
	// Make foo point to the first character of the tmp_argv,
	// to iterate over it.
	char *foo = tmp_argv;
	int index = 0;

	// Argument buffer used to store the current argument
	char ret[100];
	bzero(ret, 100);

	// While we don't reach the end of the string
	while(*foo != '\0') {
		if(index == 10)
			break;

		// If a space is found, it means we reached the end of the current
		// command.
		if(*foo == ' ') {
			// Allocate memory for the command in the custom structure if
			//  needed.
			if(my_argv[index] == NULL)
				my_argv[index] = (char *)malloc(sizeof(char) * strlen(ret) + 1);
			// Otherwise, clear the string with zeros.
			else {
				bzero(my_argv[index], strlen(my_argv[index]));
			}
			// Copies the argument
			strncpy(my_argv[index], ret, strlen(ret));
			strncat(my_argv[index], "\0", 1);
			bzero(ret, 100);
			index++;
		} 
		// If not, we add the current character to the argument buffer.
		else {
			strncat(ret, foo, 1);
		}
		// Move to next character.
		foo++;
	}
	// Copies the last argument, as there is no space character in the
	// end of the tmp_argv.
	my_argv[index] = (char *)malloc(sizeof(char) * strlen(ret) + 1);
	strncpy(my_argv[index], ret, strlen(ret));
	strncat(my_argv[index], "\0", 1);
}

/* Copies the envp values received by the main function to the my_envp
 * local variable.
 */
void copy_envp(char **envp)
{
	int index = 0;
	for(;envp[index] != NULL; index++) {
		my_envp[index] = (char *)malloc(sizeof(char) * (strlen(envp[index]) + 1));
		memcpy(my_envp[index], envp[index], strlen(envp[index]));
	}
}

/* Get the environment string of PATHs */
void get_path_string(char **tmp_envp, char *bin_path)
{
	int count = 0;
	while(1) {
		// Changed here: the strstr function was taking the first occurence
		// of "PATH" and taking that string. However, that first occurence
		// was "XDG_SESSION_PATH", and not "PATH" only. To fix this, we use
		// strncmp to check if the beginning of the string mathes "PATH=",
		// indicating that we found the correct entry in the environment
		// variables.
        
        // Search the environment PATH string. When it's found, copy the result
        // to the bin_path. Else, search in the next environment variable
		if(strncmp(tmp_envp[count], "PATH=", 5) == 0) {
			strncpy(bin_path, tmp_envp[count], strlen(tmp_envp[count]));
	    	break;
	    } else {
			count++;
		}
	}
    
}

/* Insert the PATHs found in the path_str into the
 * search_path array.
 */
void insert_path_str_to_search(char *path_str) 
{
    // PS: PATH format:
    // PATH=caminho1:caminho2:caminho3
    
	int index=0;
	char *tmp = path_str;
	char ret[100];
    
    // Ignore the string "PATH="
	while(*tmp != '=')
		tmp++;
	tmp++;

    // Until the end of the PATH
	while(*tmp != '\0') {
        // For each path...
		if(*tmp == ':') {
            // When it reaches the end of path, add a '/', a '\0' and
            // copy to the search_path
			strncat(ret, "/", 1);
			search_path[index] = (char *) malloc(sizeof(char) * (strlen(ret) + 1));
			strncat(search_path[index], ret, strlen(ret));
			strncat(search_path[index], "\0", 1);
			index++;
			bzero(ret, 100);
		} else {
            // Concatenate the letter into ret
			strncat(ret, tmp, 1);
		}
		tmp++;
	}
	// Copying the last entry in the PATH string, which does not have a
	// terminating ':' character
	strncat(ret, "/", 1);
	search_path[index] = (char *) malloc(sizeof(char) * (strlen(ret) + 1));
	strncat(search_path[index], ret, strlen(ret));
	strncat(search_path[index], "\0", 1);
}

/* Attach the PATHs in the list to the command, to test if the
 * command is available.
 */
int attach_path(char *cmd)
{
	char ret[100];
	int index;
	int fd;
	bzero(ret, 100);
    // For each path, concatenate it with the command
    // If the file exists, store it in cmd and return 0
    // Else, return -1
	for(index=0;search_path[index]!=NULL;index++) {
		strcpy(ret, search_path[index]);
		strncat(ret, cmd, strlen(cmd));
		if((fd = open(ret, O_RDONLY)) > 0) {
			strncpy(cmd, ret, strlen(ret));
			close(fd);
			return 0;
		}
	}
	return -1;
}

/* Function acts as wrapper to the whole process of forking the
 * shell and also checking if the call to execve was sucessful */
void call_execve(char *cmd)
{
	int i; // Control variable (Stores execve return value)
	//printf("cmd is %s\n", cmd);
	if(fork() == 0) {
		// Child Process
		i = execve(cmd, my_argv, my_envp);
		printf("errno is %d\n", errno);
		if(i < 0) {
			// execve failed
			printf("%s: %s\n", cmd, "command not found");
			exit(1);		
		}
	} else {
		// Father Process
		wait(NULL); // NULL implies a wait to any child
	}
}

/* Function is responsible for freeing all memory used by my_argv to prevent
 * memory leaks.
 * Assigns 0 to every position of each array of my_argv, erases each reference 
 * my_argv array holds and then calls free to each position of my_argv array.
 */
void free_argv()
{
	int index;
	for(index=0;my_argv[index]!=NULL;index++) {
		bzero(my_argv[index], strlen(my_argv[index])+1);
		my_argv[index] = NULL;
		free(my_argv[index]);
		
	}
}

void cleanup(char *tmp, char *path_str) {
	
	// Iteration variable
	int i;

	// Deallocate memory
	free(tmp);
	free(path_str);
	for(i=0;my_envp[i]!=NULL;i++)
		free(my_envp[i]);
	for(i=0;i<10;i++)
		free(search_path[i]);
	printf("\n");
}

int localCommand (char* command, char *tmp, char *path_str) {

    if(strncmp(command, "quit", 4) == 0 && strlen(command) == 4) {
        free_argv();
        cleanup(tmp, path_str);
        exit(0);
    }
    if(strncmp(command, "cd", 3) == 0) {
        chdir(my_argv[1]);
        return -1;
    } else if(strncmp(command, "pwd", 3) == 0 && strlen(command) == 3) {
		// Stores the path for the current working directory.
		char path[500];
		printf("%s\n", getcwd(path, 500));
		return -1;
	}
    return 0;
}

// If the last character of my_argv[0] (the command) is '&' or
// the las argument is '&', we set backgroundExec to 1, meaning we
// must perform a background execution of the command. Also, the '&'
// in both cases is removed from the strings.
void checkBackgroundExecution() {
	if(my_argv[0][strlen(my_argv[0]) - 1] == '&') {
		backgroundExec = 1;
		my_argv[0][strlen(my_argv[0]) - 1] = '\0';
	} else {
		int index;
		for(index=0;my_argv[index]!=NULL;index++) {
			if(strncmp(my_argv[index], "&", 1) == 0) {
				backgroundExec = 1;
				free(my_argv[index]);
				my_argv[index] = NULL;
			}
		}
	}
}

int main(int argc, char *argv[], char *envp[])
{
	// Character used to read the user's input, character by character
	char c;

	// File descriptor used to test the command if a full path
	// was given (the '/' character is present)
	int fd;

	// String used to hold the user's input
	char *tmp = (char *)malloc(sizeof(char) * 100);

	// String used to hold the PATH entry in the environment
	// variables received.
	char *path_str = (char *)malloc(sizeof(char) * 256);

	// Command string, composed of the actual command and
	// the PATH to it.
	char *cmd = (char *)malloc(sizeof(char) * 100);
	
	// Ignores buffered signals
	signal(SIGINT, SIG_IGN);

	// Sets handle_signal to be the signal handler when new
	// signals are received
	signal(SIGINT, handle_signal);

	// Copies the environment variables to a local copy.
	copy_envp(envp);

	// Gets the PATH string from the environment variables
	get_path_string(my_envp, path_str);

	// Parses the PATH string, storing different paths,
	// separated by ':', in different positions of
	// the search_path array
	insert_path_str_to_search(path_str);

    // Fork and execute the file to clear the terminal
	if(fork() == 0) {
		execve("/usr/bin/clear", argv, my_envp);
		exit(1);
	} else {
		wait(NULL);
	}
	printf("[GHMSHELL ] ");
	fflush(stdout);
    // Shell parser
	while(c != EOF) {
        // Get char and select function
		c = getchar();
		switch(c) {
            // If '\n', execute command
			case '\n': if(tmp[0] == '\0') {
                       // Print the terminal label if there is no command
					   printf("[GHMSHELL ] ");
				   } else {
				   		backgroundExec = 0;

                       // Copy the arguments of the command to my_argv array
					   fill_argv(tmp);

					   // Checks if the command's execution must be in the background
					   checkBackgroundExecution();

                       // Copy the first argument to the cmd (name of file)
                       // and insert '\0'
					   strncpy(cmd, my_argv[0], strlen(my_argv[0]));
					   strncat(cmd, "\0", 1);

                       // If there is no '/' in the beginning of the command, 
                       // attach the path and execute
                       if (localCommand(cmd, tmp, path_str)==0) {
                           if(index(cmd, '/') == NULL) {
                               if(attach_path(cmd) == 0) {
                                   call_execve(cmd);
                               } else {
                                   printf("%s: command not found1\n", cmd);
                               }
                               // Else, directly execute the command
                           } else {
                               if((fd = open(cmd, O_RDONLY)) > 0) {
                                   close(fd);
                                   call_execve(cmd);
                               } else {
                                   printf("%s: command not found\n", cmd);
                               }
                           }
                       }
                       // Clear the arguments, cmd, and repeat the shell
					   free_argv();
					   printf("[GHMSHELL ] ");
					   bzero(cmd, 100);
				   }
				   bzero(tmp, 100);
				   break;
			default: 
				strncat(tmp, &c, 1);
				break;
		}
	}
  	
  	cleanup(tmp, path_str);
	return 0;
}
