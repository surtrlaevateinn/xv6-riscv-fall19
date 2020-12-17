#include <stdio.h>
#include <disk.h>
#include <stdint.h>
#include "user.h"
#include <string.h>
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

typedef struct super_block {
    int32_t magic_num;                  // 幻数
    int32_t free_block_count;           // 空闲数据块数
    int32_t free_inode_count;           // 空闲inode数
    int32_t dir_inode_count;            // 目录inode数
    uint32_t block_map[128];            // 数据块占用位图
    uint32_t inode_map[32];             // inode占用位图
} sp_block;

struct inode {
    uint32_t size;              // 文件大小
    uint16_t file_type;         // 文件类型（文件/文件夹）
    uint16_t link;              // 连接数
    uint32_t block_point[6];    // 数据块指针
};

struct dir_item {               // 目录项一个更常见的叫法是 dirent(directory entry)
    uint32_t inode_id;          // 当前目录项表示的文件/目录的对应inode
    uint16_t valid;             // 当前目录项是否有效 
    uint8_t type;               // 当前目录项类型（文件/目录）
    char name[121];             // 目录项表示的文件/目录的文件名/目录名
};

#define SIZE_SUPER 1
#define SIZE_INODE 32

void set_map(uint32_t a[],int index){
    uint32_t b = a[index/(sizeof(uint32_t)*8)];
    uint32_t m = (1 << 31-(index%(sizeof(uint32_t)*8)));
    a[index/(sizeof(uint32_t)*8)] = b | m;
}

void clear_map(uint32_t a[],int index){
    uint32_t b = a[index/(sizeof(uint32_t)*8)];
    uint32_t m = (1 << 31-(index%(sizeof(uint32_t)*8)));
    a[index/(sizeof(uint32_t)*8)] = b & ~m;
}

// Return 1 if bit at position index in array is set to 1
int isset_map(uint32_t a[],int index){
    uint32_t b = a[index/(sizeof(uint32_t)*8)];
    uint32_t m = (1 << 31-(index%(sizeof(uint32_t)*8)));
    return ((b & m) == m);
}

void fileSystem_init(){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf[2*DEVICE_BLOCK_SIZE];
    memset(buf,0,sizeof(buf));
    struct inode *p_inode = (struct inode*)buf_inode;
    sp_block *p = (sp_block*)malloc(2*DEVICE_BLOCK_SIZE);
    p->magic_num = 180110525;
    p->free_block_count = sizeof(p->block_map)*8-SIZE_SUPER-SIZE_INODE-1;
    p->free_inode_count = sizeof(p->inode_map)*8-1;
    p->dir_inode_count = 1;
    memset(p->block_map,0,sizeof(p->block_map));
    p->block_map[0] = UINT32_MAX;
    set_map(p->block_map,SIZE_SUPER+SIZE_INODE-1);
    set_map(p->block_map,SIZE_SUPER+SIZE_INODE);
    memset(p->inode_map,0,sizeof(p->inode_map));
    set_map(p->inode_map,0);
    disk_write_block(0,p);
    disk_write_block(1,p+DEVICE_BLOCK_SIZE);
    p_inode->link = 1;
    p_inode->file_type = 1;
    p_inode->size = 0;
    p_inode->block_point[0] = 0+SIZE_INODE+SIZE_SUPER;
    int i;
    for(i=1;i<6;i++){
        p_inode->block_point[i] = 0;
    }
    disk_write_block(2,p_inode);
    disk_write_block((0+SIZE_INODE+SIZE_SUPER)*2,buf);
    disk_write_block((0+SIZE_INODE+SIZE_SUPER)*2+1,buf+DEVICE_BLOCK_SIZE);


    close_disk();
    return;
}

void createFile(char *filename,int pre){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_super[2*DEVICE_BLOCK_SIZE];
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf_block[2*DEVICE_BLOCK_SIZE];
    disk_read_block(0,buf_super);
    disk_read_block(1,buf_super+DEVICE_BLOCK_SIZE);
    sp_block *p = (sp_block*)buf_super;
    if(p->free_block_count < 7 || p->free_inode_count == 0){
        printf("disk is full!\n");
        return;
    }
    int i,j;

    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    int a,flag=0;
    struct dir_item *p_dir;
    for(i=0;i<6;i++){
        if(p_inode->block_point[i]!=0 && flag == 0){
            disk_read_block((p_inode->block_point[i])*2,buf_block);
            disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 0){
                    p_inode->size += 6*DEVICE_BLOCK_SIZE*2;
                    flag = 1;
                    break;
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    if(flag == 0){
        for(i=0;i<6;i++){
            if(p_inode->block_point[i]==0 && flag == 0){
                for(j=0;j<sizeof(p->block_map)*8;j++){
                    if(!isset_map(p->block_map,j)){
                        p_inode->block_point[i] = j;
                        set_map(p->block_map,j);
                        p->free_block_count--;
                        disk_read_block((p_inode->block_point[i])*2,buf_block);
                        disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
                        p_dir = (struct dir_item*)buf_block;
                        p_inode->size += 6*DEVICE_BLOCK_SIZE*2;
                        flag = 1;
                        break;
                    }
                }
            }
        }
        if(i==6){
            printf("disk is full!\n");
            return;
        }
    }
    a = (p_inode->block_point[i])*2;
    disk_write_block(index,buf_inode);

    for(i==0;i<sizeof(p->inode_map)*8;i++){
        if(!isset_map(p->inode_map,i)){
            set_map(p->inode_map,i);
            p->free_inode_count--;
            break;
        }
    }
    int inode_id = i;
    index = i/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    p_inode = (struct inode*)(buf_inode+(i%16)*sizeof(struct inode));
    p_inode->file_type = 0;
    p_inode->size = 6*DEVICE_BLOCK_SIZE*2;
    p_inode->link = 1;
    j = 0;
    for(i = 0;i<6;i++){
        for(;j<sizeof(p->block_map)*8;j++){
            if(!isset_map(p->block_map,j)){
                p_inode->block_point[i] = j;
                set_map(p->block_map,j);
                p->free_block_count--;
                break;
            }
        }
    }
    disk_write_block(index,buf_inode);
    disk_write_block(0,buf_super);
    disk_write_block(0,buf_super+DEVICE_BLOCK_SIZE);

    p_dir->inode_id = inode_id;
    strcpy(p_dir->name,filename);
    p_dir->type = 0;
    p_dir->valid = 1;
    disk_write_block(a,buf_block);
    disk_write_block(a+1,buf_block+DEVICE_BLOCK_SIZE);

    close_disk();
    return;
}

void createDir(char *dirname,int pre){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_super[2*DEVICE_BLOCK_SIZE];
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf_block[2*DEVICE_BLOCK_SIZE];
    disk_read_block(0,buf_super);
    disk_read_block(1,buf_super+DEVICE_BLOCK_SIZE);
    sp_block *p = (sp_block*)buf_super;
    if(p->free_inode_count == 0){
        printf("disk is full!\n");
        return;
    }
    int i,j;

    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    int a,flag=0;
    struct dir_item *p_dir;
    for(i=0;i<6;i++){
        if(p_inode->block_point[i]!=0 && flag == 0){
            disk_read_block((p_inode->block_point[i])*2,buf_block);
            disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 0){
                    p_inode->size += 6*DEVICE_BLOCK_SIZE*2;
                    flag = 1;
                    break;
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    if(flag == 0){
        for(i=0;i<6;i++){
            if(p_inode->block_point[i]==0 && flag == 0){
                for(j=0;j<sizeof(p->block_map)*8;j++){
                    if(!isset_map(p->block_map,j)){
                        p_inode->block_point[i] = j;
                        set_map(p->block_map,j);
                        p->free_block_count--;
                        disk_read_block((p_inode->block_point[i])*2,buf_block);
                        disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
                        p_dir = (struct dir_item*)buf_block;
                        p_inode->size += 6*DEVICE_BLOCK_SIZE*2;
                        flag = 1;
                        break;
                    }
                }
            }
        }
        if(i==6){
            printf("disk is full!\n");
            return;
        }
    }
    a = (p_inode->block_point[i])*2;
    disk_write_block(index,buf_inode);

    for(i==0;i<sizeof(p->inode_map)*8;i++){
        if(!isset_map(p->inode_map,i)){
            set_map(p->inode_map,i);
            p->free_inode_count--;
            p->dir_inode_count++;
            break;
        }
    }
    int inode_id = i;
    index = i/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    p_inode = (struct inode*)(buf_inode+(i%16)*sizeof(struct inode));
    p_inode->file_type = 1;
    p_inode->size = 0;
    p_inode->link = 1;

    for(i = 0;i<6;i++){
        p_inode->block_point[i] = 0;
    }
    disk_write_block(index,buf_inode);
    disk_write_block(0,buf_super);
    disk_write_block(0,buf_super+DEVICE_BLOCK_SIZE);

    p_dir->inode_id = inode_id;
    strcpy(p_dir->name,dirname);
    p_dir->type = 1;
    p_dir->valid = 1;
    disk_write_block(a,buf_block);
    disk_write_block(a+1,buf_block+DEVICE_BLOCK_SIZE);

    close_disk();
    return;
}

void ls(int dirIndex){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf_block[2*DEVICE_BLOCK_SIZE];
    int i,j;
    int index = dirIndex/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(dirIndex%16)*sizeof(struct inode));
    struct dir_item *p_dir;
    for(i=0;i<6;i++){
        if(p_inode->block_point[i]!=0){
            disk_read_block((p_inode->block_point[i])*2,buf_block);
            disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 1){
                    fprintf(2, "%s    ", p_dir->name);
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    fprintf(2,"\n");
    close_disk();
    return;
}

void copyfile(char *filename,int srcFileIndex,int dst){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_super[2*DEVICE_BLOCK_SIZE];
    char buf_inode_src[DEVICE_BLOCK_SIZE];
    char buf_inode_dst[DEVICE_BLOCK_SIZE];
    char buf_block_src[2*DEVICE_BLOCK_SIZE];
    char buf_block_dst[2*DEVICE_BLOCK_SIZE];
    disk_read_block(0,buf_super);
    disk_read_block(1,buf_super+DEVICE_BLOCK_SIZE);
    sp_block *p = (sp_block*)buf_super;
    if(p->free_inode_count == 0 || p->free_block_count < 7){
        printf("disk is full!\n");
        return;
    }
    int index_src = srcFileIndex/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    int index_dst = dst/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index_src,buf_inode_src);
    disk_read_block(index_dst,buf_inode_dst);

    struct inode *p_inode_src = (struct inode*)(buf_inode_src+(srcFileIndex%16)*sizeof(struct inode));
    struct inode *p_inode_dst = (struct inode*)(buf_inode_dst+(dst%16)*sizeof(struct inode));
    int i,j,a,flag=0;
    struct dir_item *p_dir;

    for(i=0;i<6;i++){
        if(p_inode_dst->block_point[i]!=0 && flag == 0){
            disk_read_block((p_inode_dst->block_point[i])*2,buf_block_dst);
            disk_read_block((p_inode_dst->block_point[i])*2+1,buf_block_dst+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block_dst;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 0){
                    p_inode_dst->size += 6*DEVICE_BLOCK_SIZE*2;
                    flag = 1;
                    break;
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    if(flag == 0){
        for(i=0;i<6;i++){
            if(p_inode_dst->block_point[i]==0 && flag == 0){
                for(j=0;j<sizeof(p->block_map)*8;j++){
                    if(!isset_map(p->block_map,j)){
                        p_inode_dst->block_point[i] = j;
                        set_map(p->block_map,j);
                        p->free_block_count--;
                        disk_read_block((p_inode_dst->block_point[i])*2,buf_block_dst);
                        disk_read_block((p_inode_dst->block_point[i])*2+1,buf_block_dst+DEVICE_BLOCK_SIZE);
                        p_dir = (struct dir_item*)buf_block_dst;
                        p_inode_dst->size += 6*DEVICE_BLOCK_SIZE*2;
                        flag = 1;
                        break;
                    }
                }
            }
        }
        if(i==6){
            printf("disk is full!\n");
            return;
        }
    }
    a = (p_inode_dst->block_point[i])*2;
    disk_write_block(index_dst,buf_inode_dst);
    for(i==0;i<sizeof(p->inode_map)*8;i++){
        if(!isset_map(p->inode_map,i)){
            set_map(p->inode_map,i);
            p->free_inode_count--;
            break;
        }
    }
    int inode_id = i;

    p_dir->inode_id = inode_id;
    strcpy(p_dir->name,filename);
    p_dir->type = 0;
    p_dir->valid = 1;
    disk_write_block(a,buf_block_dst);
    disk_write_block(a+1,buf_block_dst+DEVICE_BLOCK_SIZE);

    index_dst = i/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode_dst);
    p_inode_dst = (struct inode*)(buf_inode_dst+(i%16)*sizeof(struct inode));
    p_inode_dst->file_type = p_inode_src->file_type;
    p_inode_dst->size = p_inode_src->size;
    p_inode_dst->link = p_inode_src->link;
    j = 0;
    for(i = 0;i<6;i++){
        for(;j<sizeof(p->block_map)*8;j++){
            if(!isset_map(p->block_map,j)){
                p_inode_dst->block_point[i] = j;
                set_map(p->block_map,j);
                p->free_block_count--;
                disk_read_block((p_inode_src->block_point[i])*2,buf_block_src);
                disk_read_block((p_inode_src->block_point[i])*2+1,buf_block_src+DEVICE_BLOCK_SIZE);
                disk_write_block((p_inode_dst->block_point[i])*2,buf_block_src);
                disk_write_block((p_inode_dst->block_point[i])*2+1,buf_block_src+DEVICE_BLOCK_SIZE);
                break;
            }
        }
    }

    disk_write_block(index_dst,buf_inode_dst);
    disk_write_block(0,buf_super);
    disk_write_block(0,buf_super+DEVICE_BLOCK_SIZE);
    close_disk();
    return;

}

char args[MAXARG][MAXARG];
int l;

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

int finddir(int pre,char *name){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf_block[2*DEVICE_BLOCK_SIZE];
    int i,j;
    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    struct dir_item *p_dir;
    for(i=0;i<6;i++){
        if(p_inode->block_point[i]!=0){
            disk_read_block((p_inode->block_point[i])*2,buf_block);
            disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 1 && !strcmp(p_dir->name,name) && p_dir->type==1){
                    close_disk();
                    return p_dir->inode_id;
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    close_disk();
    fprintf(2,"can not find dir!\n");
    return -1;
}

int findfile(int pre,char *name){
    if(open_disk() == -1){
        printf("open disk faild!\n");
        exit(0);
    }
    char buf_inode[DEVICE_BLOCK_SIZE];
    char buf_block[2*DEVICE_BLOCK_SIZE];
    int i,j;
    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    struct dir_item *p_dir;
    for(i=0;i<6;i++){
        if(p_inode->block_point[i]!=0){
            disk_read_block((p_inode->block_point[i])*2,buf_block);
            disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
            p_dir = (struct dir_item*)buf_block;
            for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
                if(p_dir->valid == 1 && !strcmp(p_dir->name,name) && p_dir->type==0){
                    close_disk();
                    return p_dir->inode_id;
                }
                p_dir += sizeof(struct dir_item);
            }
        }
    }
    close_disk();
    fprintf(2,"can not find file!\n");
    return -1;
}

int
main(void)
{
    static char buf[100];
    char *argv[MAXARG];
    fileSystem_init();

    int i,j;
    int index,index2;
    char word[100];
    char srcname[100];

    // Read and run input commands.
    while(getcmd(buf, sizeof(buf)) >= 0){
        if(!strncmp(buf,"ls",2)){
            if(buf[2]=='\0' || !strcmp(buf,"ls /")){
                ls(0);
            }else if (buf[3]!='/')
            {
                fprintf(2,"cmd error!\n");
            }else
            {
                i=4;
                j=0;
                index=0;
                while (buf[i]!='\0')
                {
                    if(buf[i] == '/'){
                        word[j] = '\0';
                        j = 0;
                        i++;
                        index = finddir(index,word);
                    }else
                    {
                        word[j++]=buf[i++];    
                    }
                }
                word[j] = '\0';
                index = finddir(index,word);
                ls(index);
            }
        }
        if(!strncmp(buf,"mkdir",5)){
            if (buf[6]!='/')
            {
                fprintf(2,"cmd error!\n");
            }else
            {
                i=7;
                j=0;
                index=0;
                while (buf[i]!='\0')
                {
                    if(buf[i] == '/'){
                        word[j] = '\0';
                        j = 0;
                        i++;
                        index = finddir(index,word);
                    }else
                    {
                        word[j++]=buf[i++];    
                    }
                }
                word[j] = '\0';
                createDir(word,index);
            }
        }
        if(!strncmp(buf,"touch",5)){
            if (buf[6]!='/')
            {
                fprintf(2,"cmd error!\n");
            }else
            {
                i=7;
                j=0;
                index=0;
                while (buf[i]!='\0')
                {
                    if(buf[i] == '/'){
                        word[j] = '\0';
                        j = 0;
                        i++;
                        index = finddir(index,word);
                    }else
                    {
                        word[j++]=buf[i++];    
                    }
                }
                word[j] = '\0';
                createFile(word,index);
            }
        }
        if(!strncmp(buf,"cp",2)){
            if (buf[6]!='/')
            {
                fprintf(2,"cmd error!\n");
            }else
            {
                i=7;
                j=0;
                index=0;
                while (buf[i]!=' ')
                {
                    if(buf[i] == '/'){
                        word[j] = '\0';
                        j = 0;
                        i++;
                        index = finddir(index,word);
                    }else
                    {
                        word[j++]=buf[i++];
                    }
                }
                word[j] = '\0';
                index2 = findfile(word,index);
                strcpy(srcname,word);
                if (buf[++i]!='/')
                {
                    fprintf(2,"cmd error!\n");
                }else
                {
                    i++;
                    j=0;
                    index=0;
                    while (buf[i]!='\0')
                    {
                        if(buf[i] == '/'){
                            word[j] = '\0';
                            j = 0;
                            i++;
                            index = finddir(index,word);
                        }else
                        {
                            word[j++]=buf[i++];
                        }
                    }
                    word[j] = '\0';
                    index = finddir(index,word);
                    copyfile(srcname,index2,index);
                }
            }
        }
        if(!strcmp(buf,"shutdown")){
            exit(0);
        }
    }
}

