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

/* Arrays of string that will be used:
 * my_argv: represents the argv that will be passed to the command that will
 * be executed (using a custom data structure instead of having a single string
 * containing all command line arguments), typed by the user.
 * my_envp: local copy of the envp variable.
 */
static char *my_argv[100], *my_envp[100];

/* Array of strings representing PATHs to execute commands */
static char *search_path[10];

/* Funcao definida para lidar com sinais recebidos durante a execucao.*/
void handle_signal(int signo)
{
	printf("\n[GHMSHELL ] ");
	fflush(stdout);
}

/* Funcao para copiar o conteudo de argv para a estrutura local
 * criada para isso. Eh utilizado um array de strings, onde cada uma dessas
 * strings eh um dos argumentos presentes no argv original.
 */
void fill_argv(char *tmp_argv)
{
	char *foo = tmp_argv;
	int index = 0;
	char ret[100];
	bzero(ret, 100);

	// Enquanto o fim da string o
	while(*foo != '\0') {
		if(index == 10)
			break;

		if(*foo == ' ') {
			if(my_argv[index] == NULL)
				my_argv[index] = (char *)malloc(sizeof(char) * strlen(ret) + 1);
			else {
				bzero(my_argv[index], strlen(my_argv[index]));
			}
			strncpy(my_argv[index], ret, strlen(ret));
			strncat(my_argv[index], "\0", 1);
			bzero(ret, 100);
			index++;
		} else {
			strncat(ret, foo, 1);
		}
		foo++;
		/*printf("foo is %c\n", *foo);*/
	}
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

// Get the environment string of PATHs
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

// Insere os caminhos padrões na string path_str
// Organize the PATHs into the PATH list "search_path"
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
            // When it reaches the end of path, add a '/', a '\0' and copy to the search_path
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
}

int attach_path(char *cmd)
{
	char ret[100];
	int index;
	int fd;
	bzero(ret, 100);
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
	printf("cmd is %s\n", cmd);
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

/* Function is responsible free all memory used by my_argv to prevent memory
 * leaks 
 * > Assigns 0 to every position of each array of my_argv, erases each reference 
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

int main(int argc, char *argv[], char *envp[])
{
	char c;
	int i, fd;
	char *tmp = (char *)malloc(sizeof(char) * 100);
	char *path_str = (char *)malloc(sizeof(char) * 256);
	char *cmd = (char *)malloc(sizeof(char) * 100);
	
	signal(SIGINT, SIG_IGN);
	signal(SIGINT, handle_signal);

	copy_envp(envp);
	get_path_string(my_envp, path_str);
	insert_path_str_to_search(path_str);

	if(fork() == 0) {
		execve("/usr/bin/clear", argv, my_envp);
		exit(1);
	} else {
		wait(NULL);
	}
	printf("[GHMSHELL ] ");
	fflush(stdout);
	while(c != EOF) {
		c = getchar();
		switch(c) {
			case '\n': if(tmp[0] == '\0') {
					   printf("[GHMSHELL ] ");
				   } else {
					   fill_argv(tmp);
					   strncpy(cmd, my_argv[0], strlen(my_argv[0]));
					   strncat(cmd, "\0", 1);
					   if(index(cmd, '/') == NULL) {
						   if(attach_path(cmd) == 0) {
							   call_execve(cmd);
						   } else {
							   printf("%s: command not found\n", cmd);
						   }
					   } else {
						   if((fd = open(cmd, O_RDONLY)) > 0) {
							   close(fd);
							   call_execve(cmd);
						   } else {
							   printf("%s: command not found\n", cmd);
						   }
					   }
					   free_argv();
					   printf("[GHMSHELL ] ");
					   bzero(cmd, 100);
				   }
				   bzero(tmp, 100);
				   break;
			default: strncat(tmp, &c, 1);
				 break;
		}
	}
	free(tmp);
	free(path_str);
	for(i=0;my_envp[i]!=NULL;i++)
		free(my_envp[i]);
	for(i=0;i<10;i++)
		free(search_path[i]);
	printf("\n");
	return 0;
}
