#ifndef __REG__
#define __REG__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef pid_t jid_t;

typedef struct Node {
	char *processName;
	pid_t pid;
	jid_t jid;
	int status;

	struct Node *next;
} Node;

typedef struct List {
	int size;
	Node *head;
	Node *tail;
} List;

List * initialize();
void removeNode(List *list, jid_t key);
Node * createNode(char *processName, pid_t pid, jid_t jid, int status);
void insertTail(List *list, Node *node);
int isEmpty(List *list);
Node * findNode(List *list, jid_t jid);
void listToString(List *list);
void printDebug(char *TAG, char *message);
void emptyList(List *list);

#endif