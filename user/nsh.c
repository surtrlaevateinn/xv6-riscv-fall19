#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3

struct cmd
{
    int type;
};

struct execcmd
{
    int type;
    char *argv[MAXARG];
};

struct redircmd
{
    int type;
    struct cmd *cmd;
    char *file;
    int mode;
    int fd;
};

struct pipecmd
{
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct execcmd execcmd_list[MAXARG];
struct redircmd redircmd_list[MAXARG];
struct pipecmd pipecmd_list[MAXARG];
int e_list = 0;
int r_list = 0;
int p_list = 0;

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

void runcmd(struct cmd *cmd){
    int p[2];
    switch (cmd->type)
    {
    case EXEC:
        struct execcmd *ecmd = (struct execcmd*)cmd;
        exec(ecmd->argv[0],ecmd->argv);
        fprintf(2,"exec %s fail",ecmd->argv[0]);
        break;
    case REDIR:
        struct redircmd *rcmd = (struct redircmd*)cmd;
        close(rcmd->fd);
        if(open(rcmd->file,rcmd->mode)<0){
            fprintf(2,"open %s fail",rcmd->file);
            exit(-1);
        }
        runcmd(rcmd->cmd);
        break;
    case PIPE:
        struct pipecmd *pcmd = (struct pipecmd*)cmd;
        if(pipe(p)<0){
            fprintf(2,"pipe fail");
            exit(-1);
        }
        if(fork()==0){
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->left);
        }
        if(fork()==0){
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->right);
        }
        wait(0);
        wait(0);
        break;
    default:
        break;
    }
    exit(0);
}

struct cmd* parseexec(char *ps ,char *es){

    int flag = 0;
    while(ps < es && *ps != '|')
        ps++;
}

struct cmd* parsecmd(char *ps ,char *es){
    struct cmd *cmd;
    cmd = parseexec(ps,es);
    while(ps < es && strchr(" ", *ps))
        ps++;
    if(*(ps) == '|'){
        ps++;
        while(ps < es && strchr(" ", *ps))
            ps++;
        pipecmd_list[p_list].type = PIPE;
        pipecmd_list[p_list].left = cmd;
        pipecmd_list[p_list].right = parsecmd(ps,es);
        cmd = pipecmd_list + p_list;
        p_list++;
    }
    return cmd;
}

int
main(void)
{
  static char buf[100];


  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork() == 0)
      runcmd(parsecmd(buf,buf+strlen(buf)));
    wait(0);
  }
  exit(0);
}