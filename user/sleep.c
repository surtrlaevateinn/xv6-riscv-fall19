
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc != 2){
		fprintf(2, "must 1 argument for sleep\n");
		exit();
	}
	int sleepNum = atoi(argv[1]);
	printf("(nothing happens for a little while)\n");
	sleep(sleepNum);
	exit();
}


