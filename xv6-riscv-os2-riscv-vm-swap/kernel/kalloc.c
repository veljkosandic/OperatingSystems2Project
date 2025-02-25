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
#define ARR_SIZE (FRAME_CNT+CHAR_BITS-1)/CHAR_BITS
void freerange(/*void *pa_start, void *pa_end*/);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

struct {
    struct spinlock lock;
    char bitAlloc[ARR_SIZE]; //bit vektor za alokator - unsigned je 32 bita
    char kernelPages[ARR_SIZE];
    unsigned long sframe;//pocetni okvir OM
    char* saddr;//pocetna adresa OM -zaokruzena na pocetak okvira
    unsigned long earliestFrame;//potencijalni najmanji okvir koji je slobodan
    unsigned long total;
    unsigned long available;
} kmem;

void
kinit()
{
    initlock(&kmem.lock, "kmem");
    kmem.sframe= (unsigned long)(PGROUNDUP((uint64)end)/PGSIZE);
    kmem.earliestFrame=kmem.sframe;
    kmem.saddr =(char*) PGROUNDUP((uint64)end);
    kmem.total = (unsigned long)(((char*)PHYSTOP- (char*)(PGROUNDUP((uint64)end)))/PGSIZE);
    kmem.available=kmem.total;
    freerange(end, (void*)PHYSTOP);
}
//PGROUNDUP - poravnanje adrese na visi blok
void
freerange(void *pa_start, void *pa_end) {
    for (unsigned long i = 0; i < ARR_SIZE; i++) {
        kmem.bitAlloc[i] = 0x00;
        kmem.kernelPages[i] = 0x00;
    }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");
    memset(pa, 1, PGSIZE);
    acquire(&kmem.lock);
    setBit(pa,0,0);
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    char* r = 0;
    acquire(&kmem.lock);
    r = (char*)getFree();
    if(!r)r = (char*)swap_put(0);
    release(&kmem.lock);
    if(r) {
        memset((char *) r, 5, PGSIZE);
    }
    //else printf("pun");
    return (void*)r;
}
void* kallocsetkern(void)
{
    char* r = 0;
    acquire(&kmem.lock);
    r = (char*)getFree();
    if(!r)r = (char*)swap_put(0);
    if(r)
        setToKernPage(r);
    release(&kmem.lock);
    if(r) {
        memset((char *) r, 5, PGSIZE);
    }
    //else printf("pun");
    return (void*)r;
}
void* kallocunsafe(void)
{
    char* r = 0;
    r = (char*)getFree();
    if(!r)r = (char*)swap_put(1);
    if(r) {
        memset((char *) r, 5, PGSIZE);
    }
    //else printf("pun");
    return (void*)r;
}
//0010101
//0100000
void setBit(void *pa,char allocValue,char kernValue){
    unsigned long frame = (unsigned long)pa>>OFFSET_BITS;
    if(frame < kmem.sframe)return;
    unsigned long i = (frame-kmem.sframe)/CHAR_BITS,j=(frame-kmem.sframe)%CHAR_BITS;//i-element u vektoru,j-bit u elementu;
    if(allocValue==0){kmem.bitAlloc[i]&=~(1UL<<j);
        if(kmem.earliestFrame>frame)kmem.earliestFrame=frame;
        clear_ref(frame  - kmem.sframe);
    }
    else kmem.bitAlloc[i]|=(1UL<<j);
    if(kernValue==0){
        if(kmem.kernelPages[i] & (1UL<<j))
            kmem.available++;
        kmem.kernelPages[i]&=~(1UL<<j);
    }
}
void* getFree(){
    for(unsigned long i = kmem.earliestFrame-kmem.sframe;i<kmem.total;i++) {
        if (!(kmem.bitAlloc[i / CHAR_BITS] & (1UL << (i % CHAR_BITS)))) {
            char* addr = kmem.saddr + (i << OFFSET_BITS);
            setBit(addr,1,1);
            kmem.earliestFrame=i+1+kmem.sframe;
            return (void*)addr;
        }
    }
    return 0;
}
void setToKernPage(void *pa){
    unsigned long frame = (unsigned long)pa>>OFFSET_BITS;
    unsigned long i = (frame-kmem.sframe)/CHAR_BITS,j=(frame-kmem.sframe)%CHAR_BITS;//i-element u vektoru,j-bit u elementu;
    if(frame < kmem.sframe)return;
    kmem.kernelPages[i]|=(1UL<<j);
    kmem.available--;
}
char isCandidate(unsigned long i){
    if(i>=kmem.total)return 1;
    //if(!(kmem.bitAlloc[i / CHAR_BITS] & (1UL << (i % CHAR_BITS)))) return 2;
    if((kmem.kernelPages[i / CHAR_BITS] & (1UL << (i % CHAR_BITS))))return 3;
    return 0;
}
unsigned long getTotal(){
    return kmem.total;
}
unsigned long getFirst(){
    return kmem.sframe;
}
unsigned long getAvailable(){
    return kmem.available;
}
void KmemLock(){
    acquire(&kmem.lock);
}
void KmemUnlock(){
    release(&kmem.lock);
}