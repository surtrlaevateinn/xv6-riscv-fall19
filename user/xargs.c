#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    int i,j,k=0;
    int len;
    char *margv[MAXARG];
    char buf[MAXARG];
    char a[MAXARG];
    for(i=0;i<argc-1;i++){
        margv[i] = argv[i+1];
    }
    j = argc - 1;
    while((len = read(0,buf,sizeof(buf)))>0){
        for(i=0;i<len;i++){
            if(buf[i]=='\n'){
                a[k] = '\0';
                k = 0;
                margv[j] = a;
                margv[j+1] = 0;
                j = argc-1;
                if(fork()==0){
                    exec(argv[1],margv);
                }
                wait();
            }else if (buf[i]==' ')
            {
                a[k] = '\0';
                margv[j] = a;
                k = 0;
                j++;
            }else
            {
                a[k] = buf[i];
                k++;
            }
        }
    }
	exit();
}