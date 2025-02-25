// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define CHAR_BITS 8
#define OFFSET_BITS 12
#define FRAME_CNT (((char*)PHYSTOP- (char*)(PGROUNDUP((uint64)end)))/PGSIZE)
void setBit(void*,char);
void* getFree();
void freerange(/*void *pa_start, void *pa_end*/);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

/*struct run {
  struct run *next;
};*/

struct {
  struct spinlock lock;
  //struct run *freelist;
  char* bitAllocFree; //bit vektor za alokator - unsigned je 32 bita
  unsigned long sframe;//pocetni okvir OM
  unsigned long totalFrameSize;//ukupan broj okvira OM
  char* saddr;//pocetna adresa OM -zaokruzena na pocetak okvira
  unsigned long earliestFrame;//potencijalni najmanji okvir koji je slobodan
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.totalFrameSize = (unsigned long)(((char*)PHYSTOP- (char*)(PGROUNDUP((uint64)end)))/PGSIZE);//pocetni broj stranica - dio mora da se alocira za vektore
  unsigned long frameVec = kmem.totalFrameSize/(CHAR_BITS) + (kmem.totalFrameSize%CHAR_BITS ? 1 : 0);
  kmem.sframe= (unsigned long)(PGROUNDUP((uint64)end))/PGSIZE + ((unsigned long)(PGROUNDUP((uint64)end))%PGSIZE ? 1 : 0);
  kmem.sframe+=frameVec;
  kmem.totalFrameSize-=frameVec;
  kmem.bitAllocFree=(char*)(PGROUNDUP((uint64)end));
  kmem.saddr= kmem.bitAllocFree+(frameVec<<OFFSET_BITS);
  kmem.earliestFrame=kmem.sframe;
  freerange(kmem.saddr, (void*)PHYSTOP);
}
//PGROUNDUP - poravnanje adrese na visi blok
void
freerange(/*void *pa_start, void *pa_end*/)
{
  /*char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);*/
  for(unsigned long i=0;i<kmem.totalFrameSize/(CHAR_BITS) + (kmem.totalFrameSize%CHAR_BITS ? 1 : 0);i++)
      kmem.bitAllocFree[i]=0xff;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
 /* struct run *r;*/

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < kmem.saddr || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  /*r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);*/
    acquire(&kmem.lock);
    setBit(pa,1);
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
 /* struct run *r;*/
 char* r = 0;
 acquire(&kmem.lock);
  /*r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;*/
  r = (char*)getFree();
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk*/
    else printf("pun");
  return (void*)r;
}
//0010101
//0100000
void setBit(void *pa,char type){
    unsigned long frame = (unsigned long)pa>>OFFSET_BITS;
    unsigned long i = (frame-kmem.sframe)/CHAR_BITS,j=(frame-kmem.sframe)%CHAR_BITS;//i-element u vektoru,j-bit u elementu;
    if(type==1){kmem.bitAllocFree[i]|=(1UL<<j);
    if(kmem.earliestFrame>frame)kmem.earliestFrame=frame;
    }
    else kmem.bitAllocFree[i]&=~(1UL<<j);
}
void* getFree(){
    for(unsigned long i = kmem.earliestFrame-kmem.sframe;i<kmem.totalFrameSize;i++) {
        if (kmem.bitAllocFree[i / CHAR_BITS] & (1UL << (i % CHAR_BITS))) {
            char* addr = kmem.saddr + (i << OFFSET_BITS);
            setBit(addr,0);
            kmem.earliestFrame=i+1+kmem.sframe;
            return (void*)addr;
        }
    }
    return 0;
}