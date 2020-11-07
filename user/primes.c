#include "kernel/types.h"
#include "user/user.h"

#define Max 50

void prime(int *num,int list[]);

int main(int argc, char *argv[])
{
    int i;
    int list[Max];
    int num0=34;
    int *num = &num0;
    for(i = 2; i <36 ; i++){
        list[i-2] = i;
    }
    prime(num,list);
    exit();
    
}

void prime(int *num,int list[])
{
    int i,count=0;
    char buf[4];
    int fd[2];
    printf("prime %d\n",list[0]);
    if(*num == 1)
    {
        return;
    }
    pipe(fd);
    for(i=1;i<*num;i++){
        if(list[i]%list[0]!=0){
            write(fd[1],(char *)&list[i],4);
            count++;
        }
    }
    *num = count;
    if(*num == 0)
    {
        return;
    }
    close(fd[1]);
    int pid = fork();
    if(pid < 0){
        printf("fork error");
        exit();
    }
    if(pid == 0)
    {
        for(i = 0;i<*num;i++){
            read(fd[0],buf,4);
            list[i]=*((int *)buf);
        }
        close(fd[0]);
        prime(num,list);
        exit();
    }
    wait();
}
