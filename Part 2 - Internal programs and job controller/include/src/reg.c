/*
 *	Authors:
 *	Giuliano Barbosa Prado
 *	Henrique de Almeida Machado da Silveira
 *	Marcello de Paula Ferreira Costa
 *
 *	Simple list implementation for handling jobs inside a shell
 */

#include "reg.h"

List * initialize(){
	printDebug("initialize","entered function");
	List * list = NULL;

	list = (List *) calloc(1,sizeof(List));
	list -> head = NULL;
	list -> tail = NULL;
	list -> size = 0;

	return list;
}

// The key is the jid of a job
void removeNode(List *list, int key){
	Node *current = NULL;
	Node *previous = NULL;
	int listSize = 0;
	int index = 0;

	if (list != NULL) {
		listSize = list -> size;
		if (listSize == 0){
			return;
		} else if (listSize == 1) {
			current = list -> head;
			if(current -> jid == key) {
				list -> head = NULL;
				list -> tail = NULL;
				list -> size = 0;
				free(current -> processName);
				free(current);
				return;
			}
		} else {
			// Primeiro Item da Lista
			current = list -> head;
			if(current -> jid == key) {
				list -> head = current -> next;
				list -> size--;
				free(current -> processName);
				free(current);
				return;
			}

			index++;
			previous = current;
			current = current -> next;
			while (index < listSize) {
				if (current -> jid == key) {
					previous -> next = current -> next;
					list -> size--;
					free(current -> processName);
					free(current);
					return;
				} else {
					previous = current;
					current = current-> next;
					index++;
				}
			}
		}
	}
}

Node * createNode(char *processName, pid_t pid, int jid, int status){
	Node *node = (Node *) malloc(sizeof(Node));
	node -> processName = (char *) malloc (strlen(processName)*sizeof(char)+1);

	if (node != NULL && node -> processName != NULL){
		strcpy(node -> processName, processName);
		node -> pid = pid;
		node -> jid = jid;
		node -> status = status;
		node -> next = NULL;
		return node;
	}

	return NULL;
}

void insertTail(List *list, Node *node) {
	if (list != NULL && node != NULL) {

		if (list -> size == 0){
			list -> tail = node;
			list -> head = node;
		} else {
			// Insertion
			list -> tail -> next = node;
			list -> tail = node;
		}
		list -> size++;
	}
}

int isEmpty(List *list){
	if (list -> size == 0)
		return 0;
	else
		return 1;
}

void listToString(List *list){
	printDebug("listToString","entered function");
	int listSize = list -> size;
	int i = 0;
	Node *current = NULL;

	printf("List Size: %d\n", listSize);

	current = list -> head;
	for (i < 0; i < listSize; i++){
		printf(" Job: %s\n", current -> processName);
		printf(" PID: %d\n", current -> pid);
		printf(" JID: %d\n", current -> jid);
		if(current -> status == RUNNING){
			printf("Active\n");
		} else if(current -> status == DONE) {
			printf("Done\n");
		} else {
			printf("Terminated\n");
		}
		printf("--------------------------\n");
		current = current -> next;
	}
}

void printDebug(char *TAG, char *message){
	printf("%s : %s\n", TAG, message);
}

void emptyList(List *list){
	Node *current = NULL;
	Node *previous = NULL;

	int listSize = list -> size;
	int i = 0;

	if (list -> size == 0){
		return;
	} else {
		current = list -> head;
		previous = current;
		while (i++ < listSize){
			printDebug("emptyList","Removing another node");
			free(current -> processName);
			current = current -> next;

			free(previous);
			previous = current;
		}
	}
}

Node * findNode(List *list, int key){

	int listSize;
	Node *current = NULL;
	if (list != NULL){
		listSize = list -> size;
		if(listSize > 0){
			current = list -> head;

			while(current != NULL){
				if(current -> jid == key){
					return current;
				}
				else {
					current = current -> next;
				}
			}
		} else {
			return NULL;
		}
	}
}