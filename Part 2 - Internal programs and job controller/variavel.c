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

int main(int argc, char *argv[], char *envp[])
{
	printf("\n");
	int index = 0;
	for(;envp[index] != NULL; index++) {
		printf("%s\n", envp[index]);
	}
	printf("\n");
	return 0;
}