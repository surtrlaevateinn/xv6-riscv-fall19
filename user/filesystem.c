#include <stdio.h>
#include <disk.h>
#include <stdint.h>
#include "user.h"
#include <string.h>

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
    set_map(p->block_map,0);
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
    if(p->free_block_count == 0 || p->free_inode_count == 0){
        printf("disk is full!\n");
        exit(0);
    }
    int i,j;

    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    int a = p_inode->size / (2*DEVICE_BLOCK_SIZE);
    int b;
    if(a == 6){
        printf("dir is full!\n");
        exit(0);
    }
    struct dir_item *p_dir;
    for(i=0;i<a;i++){
        disk_read_block((p_inode->block_point[i])*2,buf_block);
        disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
        for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
            p_dir = (struct dir_item*)(buf_block+j*sizeof(struct dir_item));
            if(p_dir->valid == 0){
                if(i == a && j == 0){
                    p->free_block_count--;
                    set_map(p->block_map,)
                }
                break;
            }
        }
        if(j != (2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item))
            break;
    }
    a = (p_inode->block_point[i])*2;
    p_inode->size += sizeof(struct dir_item);

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
        if(j == sizeof(p->block_map)*8){
            printf("disk is full!\n");
            exit(0);
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
    if(p->free_block_count == 0 || p->free_inode_count == 0){
        printf("disk is full!\n");
        exit(0);
    }
    int i,j;

    int index = pre/(DEVICE_BLOCK_SIZE/sizeof(struct inode))+2;
    disk_read_block(index,buf_inode);
    struct inode *p_inode = (struct inode*)(buf_inode+(pre%16)*sizeof(struct inode));
    p_inode->size += sizeof(struct dir_item);
    int a = p_inode->size / (2*DEVICE_BLOCK_SIZE);
    int b;
    if(a == 6){
        printf("dir is full!\n");
        exit(0);
    }
    struct dir_item *p_dir;
    for(i=0;i<=a;i++){
        disk_read_block((p_inode->block_point[i])*2,buf_block);
        disk_read_block((p_inode->block_point[i])*2+1,buf_block+DEVICE_BLOCK_SIZE);
        for(j=0;j<(2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item);j++){
            p_dir = (struct dir_item*)(buf_block+j*sizeof(struct dir_item));
            if(p_dir->valid == 0)
                break;
        }
        if(j != (2*DEVICE_BLOCK_SIZE)/sizeof(struct dir_item))
            break;
    }
    a = (p_inode->block_point[i])*2;

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
    p_inode->file_type = 1;
    p_inode->size = 0;
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
        if(j == sizeof(p->block_map)*8){
            printf("disk is full!\n");
            exit(0);
        }
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

int
main(int argc, char *argv[]){   
}

