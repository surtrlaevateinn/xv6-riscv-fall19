// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void mkfree(void *pa,int j);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  int i;
  for(i=0;i<NCPU;i++)
    initlock(&kmems[i].lock, "kmem"); 
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  int i,j,num,k=0;
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    k++;
  num = (int)(k/NCPU);
  k = k%NCPU;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(i=0;i<NCPU;i++){
    for(j=0;j<num;j++){
      mkfree(p,i);
      p += PGSIZE;
    }
  }
  for(i=0;i<k;i++){
    mkfree(p,0);
    p += PGSIZE;
  }
}

void
mkfree(void *pa,int j)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;


  acquire(&kmems[j].lock);
  r->next = kmems[j].freelist;
  kmems[j].freelist = r;
  release(&kmems[j].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int j=cpuid();
  pop_off();

  acquire(&kmems[j].lock);
  r->next = kmems[j].freelist;
  kmems[j].freelist = r;
  release(&kmems[j].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int i = cpuid();
  pop_off();
  int j;
  
  acquire(&kmems[i].lock);
  r = kmems[i].freelist;
  if(r){
    kmems[i].freelist = r->next;
    release(&kmems[i].lock);
  }else{
    release(&kmems[i].lock);
    for(j=0;j<NCPU;j++){
      if(j == i){
        continue;
      }
      acquire(&kmems[j].lock);
      r = kmems[j].freelist;
      if(r){
        kmems[j].freelist = r->next;
        release(&kmems[j].lock);
        break;
      }
      release(&kmems[j].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
