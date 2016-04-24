#include "include/src/reg.h"

int main(int argc, const char *argv[]){

	List *jobsList = NULL;

	jobsList = initialize();
	listToString(jobsList);

	printf("****** INSERTING *******\n");
	insertTail(jobsList,createNode("Job 1",101,1,0));
	insertTail(jobsList,createNode("Job 2",102,2,0));
	insertTail(jobsList,createNode("Job 3",103,3,0));
	insertTail(jobsList,createNode("Job 4",104,4,0));

	listToString(jobsList);

	printf("****** REMOVING *******\n");
	removeNode(jobsList,1);
	removeNode(jobsList,2);

	listToString(jobsList);

	printf("****** INSERTING *******\n");
	insertTail(jobsList,createNode("Job 1",101,1,0));
	insertTail(jobsList,createNode("Job 2",102,2,0));

	listToString(jobsList);
	emptyList(jobsList);
	free(jobsList);
	
	return 0;
}
