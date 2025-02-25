//
// Created by veljkosandic on 24.12.23..
//
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "swap.h"
#include "defs.h"
struct swap_disk sd;
struct RefBits refbits;
#define REFUPDATE 10 //nakon koliko tick-ova se azuriraju biti referenciranja
#define WORKINGSETUPDATE 300 //nakon koliko tick-ova se azuriraju radni skupovi procesa,idealno umnozak REFUPDATE
//za zamjenu stranica
uchar swapbuf[4096];
void* swap_put(int i){
    if(i==0)
        start_swapping();
    pte_t *pte = 0;
    unsigned long victim = getVictim(&pte);
    if(victim==-1){
        panic("how");
    }
    victim <<=PGSHIFT;
    if(!pte)panic("PTE is NULL");
    unsigned long dblock = save_to_disk((uchar*)victim);
    if(dblock==16 * 256) {
        //printf("nope");
        if(i==0)
            end_swapping();
        return 0;
    }
    update_table((victim>>12)-getFirst());
    *pte |= PTE_S;
    *pte &= ~(PTE_V);
    *pte &= ~(0x7FFFFFFUL << 10);
    *pte |= (dblock << 10);
    memset((char*)victim, 1, PGSIZE);
    if(i==0)
        end_swapping();
    return (void*)victim;
}
char swap_replace(pte_t* pte){
    KmemLock();
    start_swapping();
    load_from_disk(PTE2PA(*pte)>>12, (uchar *) swapbuf);
    char* newPage = kallocunsafe();
    if(newPage==0){
        end_swapping();
        KmemUnlock();
        return 1;
    }
    *pte |= PTE_V;
    *pte &= ~(PTE_S);
    *pte &= ~(0x7FFFFFFUL << 10);
    *pte |= PA2PTE(newPage);
    memmove(newPage,swapbuf,4096);
    end_swapping();
    KmemUnlock();
    return 0;
}
unsigned long save_to_disk(uchar buffer[MEMORY_BLOCK_SIZE]){
    unsigned long i =sd.earliestBlock;
    for(;i<SWAP_BLOCK_CNT;i++)
        if(!(sd.alloc[i/CHAR_SIZE] & (1UL << i%CHAR_SIZE)))
            break;
    if(i==SWAP_BLOCK_CNT) {
        return SWAP_BLOCK_CNT;
    }
    sd.earliestBlock=i+1;
    sd.alloc[i/CHAR_SIZE]|=(1UL << (i%CHAR_SIZE));
    // printf("alloc:%d\n",i);
    for(unsigned k = 0;k<SWAP_MEM_CNT;k++)//zakasnjeno punjenje
        write_block(SWAP_MEM_CNT*i+k,((uchar*)buffer)+k*SWAP_BLOCK_SIZE,1);
    return i;
}
char load_from_disk(unsigned long block,uchar buffer[MEMORY_BLOCK_SIZE]){
    if(block >=SWAP_BLOCK_CNT || !(sd.alloc[block/CHAR_SIZE]&(1UL << (block%CHAR_SIZE)))){
        //  printf("%d\n",block);
        panic("wrong block number");
    }
    for(unsigned k = 0;k<SWAP_MEM_CNT;k++)
        read_block(SWAP_MEM_CNT*block+k,((uchar*)buffer)+k*SWAP_BLOCK_SIZE,1);
    sd.alloc[block/CHAR_SIZE]&=~(1UL << (block%CHAR_SIZE));
    if(sd.earliestBlock>block)sd.earliestBlock=block;
    return 1;
}
void start_swapping(){
    //printf("start\n");
    acquire(&sd.swap_lock);
    denyContextSwitch();
}
void end_swapping(){
    //printf("end\n");
    allowContextSwitch();
    release(&sd.swap_lock);
}
void free_from_disk(unsigned long block){
    KmemLock();
    start_swapping();
    if(block < SWAP_BLOCK_CNT) {
        //printf("free:%d\n",block);
        sd.alloc[block/CHAR_SIZE]&=~(1UL << (block%CHAR_SIZE));
        if(sd.earliestBlock>block)sd.earliestBlock=block;
    }
    else panic("invalid block");
    end_swapping();
    KmemUnlock();
}
void swap_init(){
    initlock(&sd.swap_lock,"swap");
    initlock(&refbits.ref_lock,"ref");
    for(unsigned i=0;i<SWAP_BLOCK_CNT/CHAR_SIZE;i++)
        sd.alloc[i]=0;
    for(unsigned i=0;i<FRAME_CNT;i++)
        refbits.refBits[i]=0;
    for(unsigned i=0;i<NPROC;i++)
        refbits.available_pagetable[i]=0;
    refbits.ptr = 0;
    refbits.max = getTotal();
    sd.earliestBlock=0;
}
unsigned long getVictim(pte_t** pteret){//vraca blok zrtvu
    acquire(&refbits.ref_lock);
    unsigned long min = FRAME_CNT;
    for(int id=0;id<NPROC;id++){
        pagetable_t pt0 = getProcTable(id);
        if(!pt0)continue;
        for(int i=0;i<512;i++){
            if(!pt0[i] || !(pt0[i] & PTE_V))continue;
            pagetable_t pt1 = (pagetable_t)PTE2PA(pt0[i]);
            for(int j=0;j<512;j++){
                if(!pt1[j] || !(pt1[j] & PTE_V)) continue;
                pagetable_t pt2 =(pagetable_t) PTE2PA(pt1[j]);
                for(int k=0;k<512;k++) {
                    pte_t *pte = &pt2[k];
                    if (pte && (*pte & PTE_V) ) {
                        unsigned long block = (PTE2PA(*pte)>>12);
                        if(block >= getFirst()) {
                            block -= getFirst();
                            if (!isCandidate(block)) {
                                if (min == FRAME_CNT || refbits.refBits[block] < refbits.refBits[min]) {
                                    min = block;
                                    *pteret = pte;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    release(&refbits.ref_lock);
    //printf("%d\n",min);
    return min + getFirst();
}
void clear_ref(unsigned long block){
    acquire(&refbits.ref_lock);
    if(block < SWAP_BLOCK_CNT)
        refbits.refBits[block]=0;
    else panic("invalid block ref clear");
    release(&refbits.ref_lock);
}
unsigned long get_ref(unsigned long block){
    acquire(&refbits.ref_lock);
    if(block < SWAP_BLOCK_CNT) {
        release(&refbits.ref_lock);
        return refbits.refBits[block];
    }
    else panic("invalid block ref get");
}
void update_refs(){//zakljucavanje radimo u trap
    for(int id=0;id<NPROC;id++){
        if(refbits.available_pagetable[id]!=1)break;
        pagetable_t pt0 = getProcTable(id);
        for(int i=0;i<512;i++){
            if(!pt0[i] || !(pt0[i] & PTE_V))continue;
            pagetable_t pt1 = (pagetable_t)PTE2PA(pt0[i]);
            for(int j=0;j<512;j++){
                if(!pt1[j] || !(pt1[j] & PTE_V)) continue;
                pagetable_t pt2 =(pagetable_t) PTE2PA(pt1[j]);
                for(int k=0;k<512;k++) {
                    pte_t *pte = &pt2[k];
                    if ((*pte & PTE_V)) {
                        if((pt2[k] >> 10) >= getFirst()){
                            unsigned long block = (PTE2PA(*pte)>>12) - getFirst();
                            refbits.refBits[block] >>= 1;
                            if(*pte & PTE_A) {
                                refbits.refBits[block] |= (1UL << 31);
                                *pte &= ~(PTE_A);
                            }
                        }
                    }
                }
            }
        }
    }
    //printf("done");
}
void update_table(unsigned long victim){
    acquire(&refbits.ref_lock);
    //printf("update");
    refbits.refBits[victim]=(1UL<<31);//da se ne bi odmah izbacila
    refbits.ptr = (victim+1)%refbits.max;
    release(&refbits.ref_lock);
}
void set_pagetable_swappable(unsigned i,char x){
    acquire(&refbits.ref_lock);
    refbits.available_pagetable[i]=x;
    if(!x)stop_thrashing(i);
    release(&refbits.ref_lock);
}
void do_timer_tasks(uint ticks){
    acquire(&refbits.ref_lock);
    update_refs_and_thrashed();
    if(ticks%REFUPDATE==0) {
        update_refs();
    }
    if(ticks%WORKINGSETUPDATE==0){
        check_for_thrashing();
    }
    release(&refbits.ref_lock);
}