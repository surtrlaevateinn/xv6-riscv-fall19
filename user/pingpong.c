#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd1[2];
    int fd2[2];
    char buf1[4];
    char buf2[100];
    pipe(fd1);
    pipe(fd2);
    write(fd1[1],"ping",4);
    close(fd1[1]);
    int pid = fork();
    if(pid == 0){
	read(fd1[0],buf1,4);
	close(fd1[0]);        
	printf("%d: received %s\n",getpid(),buf1);
    write(fd2[1],"pong",4);
    close(fd2[1]);
    exit();
	
    }else{
        sleep(5);
	    read(fd2[0],buf2,sizeof(buf2));
        printf("%d: received %s\n",getpid(),buf2);
	    close(fd2[0]);
    }

	exit();
}