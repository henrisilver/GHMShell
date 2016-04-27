#ifndef __REG__
#define __REG__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNNING 0
#define DONE 1
#define TERMINATED 2

typedef struct Node {
	char *processName;
	pid_t pid;
	int jid;
	int status;

	struct Node *next;
} Node;

typedef struct List {
	int size;
	Node *head;
	Node *tail;
} List;

List * initialize();
void removeNode(List *list, int key);
Node * createNode(char *processName, pid_t pid, int jid, int status);
void insertTail(List *list, Node *node);
int isEmpty(List *list);
Node * findNode(List *list, int jid);
void listToString(List *list);
void printDebug(char *TAG, char *message);
void emptyList(List *list);

#endif