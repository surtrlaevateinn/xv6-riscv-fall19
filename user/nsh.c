#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3

char args[MAXARG][MAXARG];
int l;

void runcmd(char *argv[],int argc);

int
getcmd(char *buf, int nbuf)
{
  fprintf(2, "@ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

void parsecmd(char *cmd){
  int i=0,j=0;
  l = 0;
  while(cmd[i] != '\n' && cmd[i] != '\0'){
    while(strchr(" ",cmd[i])){
      i++;
    }
    while(!strchr(" \n\0",cmd[i])){
      args[l][j] = cmd[i];
      j++;
      i++;
    }
    args[l][j] = '\0';
    l++;
    j = 0;
  }
}

void execpipe(int b,char*argv[],int argc){
  argv[b] = 0;
  int fd[2];
  pipe(fd);
  if(fork()==0){
    close(1);
    dup(fd[1]);
    close(fd[0]);
    close(fd[1]);
    runcmd(argv,b+1);
  }else
  {
    close(0);
    dup(fd[0]);
    close(fd[0]);
    close(fd[1]);
    runcmd(argv+b+1,argc-b-1);
  }
}

void runcmd(char *argv[],int argc){
  int i;
  for(i=0;i<argc;i++){
    if(!strcmp(argv[i],"|")){
      execpipe(i,argv,argc);
    }
  }
  for(i=0;i<argc;i++){
    if(!strcmp(argv[i],"<")){
      if(i==argc-1){
        fprintf(2,"missing file for redirection");
      }
      close(0);
      if(open(argv[i+1],O_RDONLY)<0){
        fprintf(2,"open %s failed",argv[i+1]);
        exit(-1);
      }
      argv[i] = 0;
    }
    if(!strcmp(argv[i],">")){
      if(i==argc-1){
        fprintf(2,"missing file for redirection");
      }
      close(1);
      if(open(argv[i+1],O_CREATE|O_WRONLY)<0){
        fprintf(2,"open %s failed",argv[i+1]);
        exit(-1);
      }
      argv[i] = 0;
    }
  }
  exec(argv[0],argv);
}

int
main(void)
{
  static char buf[100];
  char *argv[MAXARG];

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork() == 0){
      parsecmd(buf);
      int i = 0;
      for(i=0;i<l;i++){
        argv[i] = args[i];
      }
      argv[i] = 0;
      runcmd(argv,l);
    }
    wait(0);
  }
  exit(0);
}