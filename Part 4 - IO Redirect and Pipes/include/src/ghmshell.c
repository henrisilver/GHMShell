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
#include "reg.h"
#define INPUT 0
#define OUTPUT 1

#define SHELLNAME "[GHMSHELL] : %s $ ", cwd_path

/* Variable used to represent error codes. This variable is defined in
 * errno.h and is set by system call of library functions when there are
 * errors, indicating what went wrong.
 */
extern int errno;

// Indicates when a command should be executed in background.
// 0 -> NO
// 1 -> YES
int backgroundExec;
char outputFileName[100];
char inputFileName[100];
int outputExecType = 0;
int inputExecType = 0;
int num_commands = 0;
char **command_list;

int outputPipe = 0;
int inputPipe = 0;

int originalStdin;
int originalStdout;

int** pipefile;
int** pipefile2;


int foregroundActual = 0;

/* Arrays of string that will be used:
 * my_argv: represents the argv that will be passed to the command that will
 * be executed (using a custom data structure instead of having a single string
 * containing all command line arguments), typed by the user.
 * my_envp: local copy of the envp variable.
 */
static char *my_argv[100], *my_envp[100];

List *jobs_list = NULL;
int next_jid = 0;

/* Array of strings representing PATHs to execute commands */
static char *search_path[10];

// Stores the path for the current working directory.
char cwd_path[500];

void setForeground(int pid, int isJID) {
    //printf("isjid: %d - id: %d\n", isJID, pid);
    int status;
    Node *node;
    
    if(isJID) {
        node = findNode(jobs_list, pid);
    } else {
        node = findNodePid(jobs_list, pid);
    }
    
    if(!node) {
        printf("ERROR: Could not find process with pid = %d\n", pid);
        return;
    }
    kill(node->pid, SIGCONT);
    foregroundActual = node -> pid;
    node -> status = RUNNING;
    waitpid(node->pid, &status, WUNTRACED);
    if (WIFSTOPPED(status)) {
        node -> status = STOPPED;
        printf("\n[%d]+\tStopped\t\t%s\n", node->jid, node->processName);
        fflush(stdout);
    } else if (WIFSIGNALED(status)) {
        node->status = TERMINATED;
        fflush(stdout);
    }
}

void setBackground(int pid, int isJID) {
    int status;
    foregroundActual = 0;
    Node *node;
    
    if(isJID) {
        node = findNode(jobs_list, pid);
    } else {
        node = findNodePid(jobs_list, pid);
   	}
    if(!node) {
        printf("ERROR: Could not find process with pid = %d\n", pid);
        return;
    }
    kill(node->pid, SIGCONT);
    node -> status = RUNNING;
}

/* Function defined to deal with signals received during execution. */
void handle_ctrlc(int signo) {
    if (foregroundActual!=0) {
        kill(-getpgid(foregroundActual), SIGKILL);
        foregroundActual = 0;
    }
    printf("\n");
    printf(SHELLNAME);
    fflush(stdout);
}

/* Function defined to deal with signals received during execution. */
void handle_ctrlz(int signo) {
    if (foregroundActual!=0) {
        kill(foregroundActual, SIGSTOP);
        foregroundActual = 0;
    }
    printf("\n");
    printf(SHELLNAME);
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
            my_argv[index][strlen(ret)] = '\0';
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
    my_argv[index][strlen(ret)] = '\0';
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
            search_path[index][strlen(ret)] = '\0';
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
    search_path[index][strlen(ret)] = '\0';
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

void update_jobs_status(){
    Node * current = NULL;
    int status;
    pid_t return_pid;
    
    if (jobs_list != NULL){
        current = jobs_list -> head;
        while(current != NULL){
            return_pid = waitpid(current->pid,&status,WNOHANG|WUNTRACED);
            if(return_pid == current->pid){
                if (WIFEXITED(status)){
                    current -> status = DONE;
                } else if (WIFSIGNALED(status)) {
                    current -> status = TERMINATED;
                } else if (WIFSTOPPED(status)) {
                    current -> status = STOPPED;
                }
                
            }
            current = current -> next;
        }
    }
}

/* Function acts as wrapper to the whole process of forking the
 * shell and also checking if the call to execve was sucessful */
void call_execve(char *cmd)
{
    if (strcmp(cmd,"jobs") == 0){
        update_jobs_status();
        listToString(jobs_list);
        return;
    }
    
    int index;
    
    int i; // Control variable (Stores execve return value)
    
    int forkResult = fork();
    if(forkResult == 0) {
        // Child Process
        //setpgid(getpid(), getpid());
        //signal(SIGINT, SIG_IGN); //<<<<<<< DESCOMENTAR
        //signal(SIGTSTP, SIG_IGN); //<<<<<<< DESCOMENTAR
        //signal(SIGTTOU, SIG_IGN); //<<<<<<< DESCOMENTAR
        
        /* Make yourself process group leader */
        if(setpgid(0,0) < 0) {
            perror("Could not set PGID");
            exit(1);
        }
        
        /* transfer controlling terminal */
        if( tcsetpgrp(fileno(stdin), getpgrp()) < 0) {
            perror("Error in child tcsetpgrp for STDIN");
            exit(1);
        }
        
        /* transfer controlling terminal */
        if( tcsetpgrp(fileno(stdout), getpgrp()) < 0) {
            perror("Error in child tcsetpgrp for STDOUT");
            exit(1);
        }
        
        printf("<INPUT: %d, OUTPUT: %d, cmd: %s>",inputPipe,outputPipe,cmd);
        
        if (outputPipe == (*pipefile)[INPUT])
            close((*pipefile)[OUTPUT]);
        if (outputPipe == (*pipefile2)[OUTPUT])
            close((*pipefile2)[INPUT]);
        dup2(outputPipe,fileno(stdout));
        dup2(inputPipe,fileno(stdin));
//        close((*pipefile)[INPUT]);
//        close((*pipefile2)[OUTPUT]);
        
        
        //close(pipefile2[INPUT]);
        
        //        dup2(outputPipe,fileno(stdout));
        //        close(pipefile[INPUT]);
        //        pipe(pipefile2);
        //        inputPipe = pipefile2[INPUT];
        //        dup2(inputPipe,fileno(stdin));
        //        close(pipefile2[OUTPUT]);
        
        if (outputExecType==1)
        {
            FILE* out = fopen(outputFileName,"a+");
            fflush(stdout);
            dup2(fileno(out),fileno(stdout));
            fclose(out);
        }
        else if (outputExecType==2)
        {
            FILE* out = fopen(outputFileName,"w");
            fflush(stdout);
            dup2(fileno(out),fileno(stdout));
            fclose(out);
        }
        else if (outputExecType==3)
        {
            FILE* out = fopen(outputFileName,"w");
            fflush(stderr);
            dup2(fileno(out),fileno(stderr));
            fclose(out);
        }
        
        if(inputExecType==1) {
            FILE* in = fopen(inputFileName, "r");
            if(!in) {
                perror(inputFileName);
            } else {
                fflush(stdin);
                dup2(fileno(in), fileno(stdin));
                fclose(in);
            }
        }
        
        
        i = execve(cmd, my_argv, my_envp);
  
        
        //FILE* a = fdopen(outputPipe, "a");
        //write(outputPipe, "\0", 1);
        //close(outputPipe);
        //pipefile[0] = pipefile2[0];
        //pipefile[1] = pipefile2[1];
        //pipe(pipefile2);
        
        
        printf("errno is %d\n", errno);
        if(i < 0) {
            // execve failed
            printf("%s: %s\n", cmd, "command not found");
            exit(1);
        }
    } else {
        // Father Process
        
        // Inserir na lista
        insertTail(jobs_list,createNode(cmd,forkResult,next_jid++,RUNNING));
        
        if (backgroundExec == 0) {
            foregroundActual = forkResult;
            //printf("\n\ncmd: %s\nfga: %d\n\n", cmd, foregroundActual);
            int status;
            waitpid(forkResult, &status, WUNTRACED); // NULL implies a wait to any child
            putc('\0', stdout);
            // Reparar no status. Deve ser colocado como terminado
            Node *node = findNode(jobs_list, next_jid-1);
            if (WIFSIGNALED(status)) {
                node -> status = TERMINATED;
            } else if (WIFSTOPPED(status)) {
                node -> status = STOPPED;
                printf("\n[%d]+\tStopped\t\t%s\n", node->jid, node->processName);
            } else {
                node -> status = DONE;
            }
            /* transfer controlling terminal */
            if( tcsetpgrp(fileno(stdin), getpgrp()) < 0) {
                perror("Error in parent tcsetpgrp for STDIN");
                exit(1);
            }
            
            /* transfer controlling terminal */
            if( tcsetpgrp(fileno(stdout), getpgrp()) < 0) {
                perror("Error in parent tcsetpgrp for STDOUT");
                exit(1);
            }
        }
        else {
            foregroundActual = 0;
        }
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
    emptyList(jobs_list);
}

void putInBackground() {
    if (my_argv[1] == NULL) {
        printf("usage: bg <process_id>\n");
    } else {
        int isJID;
        char *id;
        Node *job = NULL;
        if(index(my_argv[1], '%')!= NULL) {
            isJID = 1;
            if(strlen(my_argv[1]) == 1 && my_argv[2] != NULL) {
                id = my_argv[2];
            } else {
                id = my_argv[1];
                id++;
            }
            job = findNode(jobs_list, atoi(id));
        }
        else {
            isJID = 0;
            id = my_argv[1];
            job = findNodePid(jobs_list, atoi(id));
        }
        if(job) {
            printf("[debug] process to be in background: <%s>\n",id);
            setBackground(atoi(id), isJID);
        } else {
            if(isJID) {
                printf("Invalid JID: %d\n", atoi(id));
            } else {
                printf("Invalid PID: %d\n", atoi(id));
            }
        }
    }
}

void putInForeground() {
    if (my_argv[1] == NULL) {
        printf("usage: fg <process_id>\n");
    } else {
        int isJID;
        char *id;
        Node *job = NULL;
        if(index(my_argv[1], '%')!= NULL) {
            isJID = 1;
            printf("arg[1]: %s\n", my_argv[1]);
            if(strlen(my_argv[1]) == 1 && my_argv[2] != NULL) {
                printf("arg[2]: %s\n", my_argv[2]);
                id = my_argv[2];
            } else {
                id = my_argv[1];
                id++;
            }
            printf("id: %s\n", id);
            job = findNode(jobs_list, atoi(id));
        }
        else {
            isJID = 0;
            id = my_argv[1];
            job = findNodePid(jobs_list, atoi(id));
        }
        if(job) {
            printf("[debug] process to be in foreground: <%s>\n",id);
            setForeground(atoi(id), isJID);
        } else {
            if(isJID) {
                printf("Invalid JID: %d\n", atoi(id));
            } else {
                printf("Invalid PID: %d\n", atoi(id));
            }
        }
    }
}

int localCommand (char* command, char *tmp, char *path_str) {
    
    if(strncmp(command, "quit", 4) == 0 && strlen(command) == 4) {
        Node * current = NULL;
        int pid;
        if (jobs_list != NULL){
            current = jobs_list -> head;
            while(current != NULL){
                pid = current->pid;
                kill(-pid, SIGKILL);
                current = current -> next;
            }
        }
        free_argv();
        cleanup(tmp, path_str);
        exit(0);
    }
    if(strncmp(command, "cd", 2) == 0) {
        if(chdir(my_argv[1]) == -1) {
            printf("cd : %s: No such file or directory\n", my_argv[1]);
        }
        getcwd(cwd_path, 500);
        return -1;
    } else if(strncmp(command, "jobs", 4) == 0){
        call_execve(command);
        return -1;
    } else if(strncmp(command, "pwd", 3) == 0 && strlen(command) == 3) {
        printf("%s\n", getcwd(cwd_path, 500));
        return -1;
    } else if(strncmp(command, "bg", 2) == 0) {
        putInBackground();
        return -1;
    } else if(strncmp(command, "fg", 2) == 0) {
        putInForeground();
        return -1;
    }
    return 0;
}

// If the last character of my_argv[0] (the command) is '&' or
// the las argument is '&', we set backgroundExec to 1, meaning we
// must perform a background execution of the command. Also, the '&'
// in both cases is removed from the strings.
void checkBackgroundExecution() {
    int index, done = 0;
    for(index=0;my_argv[index]!=NULL && !done;index++) {
        if(strncmp(my_argv[index], "&", 1) == 0) {
            backgroundExec = 1;
            free(my_argv[index]);
            my_argv[index] = NULL;
            done = 1;
        } else if(my_argv[index][strlen(my_argv[index]) - 1] == '&') {
            backgroundExec = 1;
            my_argv[index][strlen(my_argv[index]) - 1] = '\0';
            done = 1;
        }
    }
}

void checkOutputExecution2() {
    int index, done = 0;
    char* out;
    for(index=0;my_argv[index]!=NULL && !done;index++) {
        if(strcmp(my_argv[index], ">>") == 0) {
            outputExecType = 1;
            strcpy(outputFileName, my_argv[index+1]);
            free(my_argv[index]);
            free(my_argv[index+1]); //sempre funciona?
            my_argv[index] = NULL;
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado para %s\n", outputFileName);
        } else if(strncmp(my_argv[index], ">>",2) == 0) {
            outputExecType = 1;
            strcpy(outputFileName, &my_argv[index][2]);
            free(my_argv[index]);
            my_argv[index] = NULL;
            done = 1;
            printf("Redirecionado para %s\n", outputFileName);
        } else if(strncmp(&my_argv[index][strlen(my_argv[index]) - 2],">>",2) == 0) {
            outputExecType = 1;
            strcpy(outputFileName, my_argv[index+1]);
            my_argv[index][strlen(my_argv[index]) - 2] = '\0';
            free(my_argv[index+1]);
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado(2) para %s\n", outputFileName);
        }
        else if((out = strstr(my_argv[index],">>")) != 0) {
            outputExecType = 1;
            strcpy(outputFileName, &out[2]);
            *out = '\0';
            done = 1;
            printf("Redirecionado(3) para %s\n", outputFileName);
        }
    }
}

void checkOutputExecution() {
    int index, done = 0;
    char* out;
    for(index=0;my_argv[index]!=NULL && !done;index++) {
        if(strcmp(my_argv[index], ">") == 0) {
            outputExecType = 2;
            strcpy(outputFileName, my_argv[index+1]);
            free(my_argv[index]);
            free(my_argv[index+1]); //sempre funciona?
            my_argv[index] = NULL;
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado para %s\n", outputFileName);
        } else if(strncmp(my_argv[index], ">",1) == 0) {
            outputExecType = 2;
            strcpy(outputFileName, &my_argv[index][1]);
            free(my_argv[index]);
            my_argv[index] = NULL;
            done = 1;
            printf("Redirecionado para %s\n", outputFileName);
        } else if(strncmp(&my_argv[index][strlen(my_argv[index]) - 1],">",1) == 0) {
            outputExecType = 2;
            strcpy(outputFileName, my_argv[index+1]);
            my_argv[index][strlen(my_argv[index]) - 1] = '\0';
            free(my_argv[index+1]);
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado(2) para %s\n", outputFileName);
        }
        else if((out = strstr(my_argv[index],">")) != 0) {
            outputExecType = 2;
            strcpy(outputFileName, &out[1]);
            *out = '\0';
            done = 1;
            printf("Redirecionado(3) para %s\n", outputFileName);
        }
    }
}

void checkOutputExecution3() {
    int index, done = 0;
    char* out;
    for(index=0;my_argv[index]!=NULL && !done;index++) {
        if(strcmp(my_argv[index], "2>") == 0) {
            outputExecType = 2;
            strcpy(outputFileName, my_argv[index+1]);
            free(my_argv[index]);
            free(my_argv[index+1]); //sempre funciona?
            my_argv[index] = NULL;
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado error para %s\n", outputFileName);
        } else if(strncmp(my_argv[index], "2>",2) == 0) {
            outputExecType = 2;
            strcpy(outputFileName, &my_argv[index][2]);
            free(my_argv[index]);
            my_argv[index] = NULL;
            done = 1;
            printf("Redirecionado error para %s\n", outputFileName);
        } else if(strncmp(&my_argv[index][strlen(my_argv[index]) - 2],"2>",2) == 0) {
            outputExecType = 2;
            strcpy(outputFileName, my_argv[index+1]);
            my_argv[index][strlen(my_argv[index]) - 2] = '\0';
            free(my_argv[index+1]);
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado(2) error para %s\n", outputFileName);
        }
        else if((out = strstr(my_argv[index],"2>")) != 0) {
            outputExecType = 2;
            strcpy(outputFileName, &out[2]);
            *out = '\0';
            done = 1;
            printf("Redirecionado(3) error para %s\n", outputFileName);
        }
    }
}

void checkInputExecution() {
    int index, done = 0;
    char* out;
    for(index=0;my_argv[index]!=NULL && !done;index++) {
        if(strcmp(my_argv[index], "<") == 0) {
            inputExecType = 1;
            strcpy(inputFileName, my_argv[index+1]);
            free(my_argv[index]);
            free(my_argv[index+1]); //sempre funciona?
            my_argv[index] = NULL;
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado input para %s\n", inputFileName);
        } else if(strncmp(my_argv[index], "<",1) == 0) {
            inputExecType = 1;
            strcpy(inputFileName, &my_argv[index][1]);
            free(my_argv[index]);
            my_argv[index] = NULL;
            done = 1;
            printf("Redirecionado input para %s\n", inputFileName);
        } else if(strncmp(&my_argv[index][strlen(my_argv[index]) - 1],"<",1) == 0) {
            inputExecType = 1;
            strcpy(inputFileName, my_argv[index+1]);
            my_argv[index][strlen(my_argv[index]) - 1] = '\0';
            free(my_argv[index+1]);
            my_argv[index+1] = NULL;
            done = 1;
            printf("Redirecionado(2) input para %s\n", inputFileName);
        }
        else if((out = strstr(my_argv[index],"<")) != 0) {
            inputExecType = 1;
            strcpy(inputFileName, &out[1]);
            *out = '\0';
            done = 1;
            printf("Redirecionado(3) input para %s\n", inputFileName);
        }
    }
}

void printArgs(char *tmp){
    int i = 0;
    printf("%s\n", tmp);
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
        
        if(temp_command[size - 1] == ' ') {
            temp_command[size - 1] = '\0';
            size--;
        } else {
            temp_command[size] = '\0';
        }
        i++;
        
        // This loop removes any white spaces after a pipe char ('|')
        while(command[i] == ' ')
            i++;
        
        commands[j] = (char *)calloc(size+1, sizeof(char));
        strcpy(commands[j],temp_command);
        
        bzero(&temp_command[0], sizeof(char));
    }
    
    return commands;
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
    
    // Create a copy of the original pipes
    originalStdout = dup(fileno(stdout));
    originalStdin = dup(fileno(stdin));
    
    // Ignores buffered signals
    //signal(SIGINT, SIG_IGN);
    //signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    
    // Sets handle_signal to be the signal handler when new
    // signals are received
    signal(SIGINT, handle_ctrlc);
    signal(SIGTSTP, handle_ctrlz);
    
    jobs_list = initialize();
    
    // Copies the environment variables to a local copy.
    copy_envp(envp);
    
    // Gets the PATH string from the environment variables
    get_path_string(my_envp, path_str);
    
    // Parses the PATH string, storing different paths,
    // separated by ':', in different positions of
    // the search_path array
    insert_path_str_to_search(path_str);
    
    getcwd(cwd_path, 500);
    
    int i;
    
    // Fork and execute the file to clear the terminal
    if(fork() == 0) {
        execve("/usr/bin/clear", argv, my_envp);
        exit(1);
    } else {
        wait(NULL);
    }
    
    /* Make yourself process group leader */
    if(setpgid(getpid(), getpid()) < 0) {
        perror("Could not set PGID for main");
        exit(1);
    }
    
    /* transfer controlling terminal */
    if( tcsetpgrp(fileno(stdin), getpgrp()) < 0) {
        perror("Error in main tcsetpgrp for STDIN");
        exit(1);
    }
    
    /* transfer controlling terminal */
    if( tcsetpgrp(fileno(stdout), getpgrp()) < 0) {
        perror("Error in main tcsetpgrp for STDOUT");
        exit(1);
    }
    
    printf(SHELLNAME);
    fflush(stdout);

    // Shell parser
    while(c != EOF) {
        // Get char and select function
        c = getchar();
        switch(c) {
                // If '\n', execute command
            case '\n': if(tmp[0] == '\0') {
                // Print the terminal label if there is no command
                printf(SHELLNAME);
            } else {
                
                num_commands = 0;
                
                command_list = parseCommand(tmp, &num_commands);
                
                
                
                for(i = 0; i < num_commands; i++) {
                    //printf("\nCOMMAND[i]\n%s\n\n", command_list[i]);
                    
                    // Copy the arguments of the command to my_argv array
                    fill_argv(command_list[i]);
                    //printArgs(tmp);
                    
                    // Disable background execution
                    backgroundExec = 0;
                    
                    // Checks if the command's execution must be in the background
                    checkBackgroundExecution();
                    
                    // Disable input and output redirection
                    strcpy(outputFileName,"");
                    strcpy(inputFileName,"");
                    outputExecType = 0;
                    inputExecType = 0;
                    if (i<num_commands-1){
                        pipefile2 = malloc(2*sizeof(int*));
                        pipefile2[0] = malloc(sizeof(int));
                        pipefile2[1] = malloc(sizeof(int));
                        pipe((*pipefile2));
                    }
                    if (i==0) {
                        inputPipe = originalStdin;
                        fprintf(stderr, "<originalStdin for cmd: %s>\n",command_list[i]);
                    }
                    else
                    {
                        inputPipe = (*pipefile)[INPUT];
                        fprintf(stderr, "<in%d for cmd: %s>\n",inputPipe,command_list[i]);
                    }
                    
                    if (i==num_commands-1) {
                        outputPipe = originalStdout;
                        fprintf(stderr, "<originalStdout for cmd: %s>\n",command_list[i]);
                    }
                    else
                    {
                        
                        outputPipe = (*pipefile2)[OUTPUT];
                        fprintf(stderr, "<out%d for cmd: %s>\n",outputPipe,command_list[i]);
                    }
                    
                    
                    
//                    close((*pipefile)[OUTPUT]);
//                    close((*pipefile2)[INPUT]);
                    
                    // Check if the output is redirected
                    if (outputExecType == 0)
                        checkOutputExecution3();
                    if (outputExecType == 0)
                        checkOutputExecution2();
                    if (outputExecType == 0)
                        checkOutputExecution();
                    
                    checkInputExecution();
                    
                    // Copy the first argument to the cmd (name of file)
                    // and insert '\0'
                    strncpy(cmd, my_argv[0], strlen(my_argv[0]));
                    cmd[strlen(my_argv[0])] = '\0';
                    
                    // If there is no '/' in the beginning of the command, 
                    // attach the path and execute
                    if (localCommand(cmd, command_list[i], path_str)==0) {
                        if(index(cmd, '/') == NULL) {
                            if(attach_path(cmd) == 0) {
                                call_execve(cmd);
                            } else {
                                printf("%s: command not found\n", cmd);
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
                    printf(SHELLNAME);
                    fflush(stdin);
                    bzero(cmd, 100);
                    free(pipefile);
                    pipefile = pipefile2;
                    
                }
                for(i = 0; i < num_commands; i++) {
                    free(command_list[i]);
                }
                
                free(command_list);
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