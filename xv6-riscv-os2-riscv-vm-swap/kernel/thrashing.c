//
// Created by veljkosandic on 1.1.24..
//
//dodatni bit u tabelama da je vec bio pristupan - bit 9
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "swap.h"
#include "defs.h"
#define REFBITCNT 8 //Koliko posljednjih bita referenciranja posmatramo
#define SLEEP_TIME 200 //Koliko vremena proces spava u slucaju da dodje do thrashing-a
unsigned long working_set_size[NPROC];//od koliko okvira se sastoji svaki radni skup procesa, sto vise ima alociranih, to je veca zrtva
unsigned long thrashing_blocked[NPROC];
extern struct RefBits refbits;
void thrashing_init(){
    for(unsigned long i = 0;i<NPROC;i++)
        thrashing_blocked[i] = 0;
}
void check_for_thrashing() {
    unsigned long total = 0;
    for (int id = 0; id < NPROC; id++) {
        working_set_size[id] = 0;
        if (thrashing_blocked[id] != 0)continue;
        pagetable_t pt0 = getProcTable(id);
        if (!pt0)continue;
        for (int i = 0; i < 512; i++) {
            if (!pt0[i] || !(pt0[i] & PTE_V))continue;
            pagetable_t pt1 = (pagetable_t) PTE2PA(pt0[i]);
            for (int j = 0; j < 512; j++) {
                if (!pt1[j] || !(pt1[j] & PTE_V)) continue;
                pagetable_t pt2 = (pagetable_t) PTE2PA(pt1[j]);
                for (int k = 0; k < 512; k++) {
                    pte_t *pte = &pt2[k];
                    if (((*pte & PTE_V) || (*pte & PTE_S)) && *pte & PTE_T) {
                        working_set_size[id]++;
                        *pte &= ~(PTE_T);
                        total++;
                    }
                }
            }
        }
    }
    //printf("%d\n",total);
    if (total > getAvailable()) {
        printf("\n---thrashing detected---\n");
    }
    while (total > getAvailable()) {
        int max = 0;
        for (int i = 0; i < NPROC; i++) {
            if (working_set_size[i] > working_set_size[max])max = i;
        }
        //procLock(max,0);
        if(getStat(max)!=5) {
            printf("process pid %d is blocked from using scheduler for some time... %d stat=%d\n", getpid(max), max,
                   getStat(max));
            thrashing_blocked[max] = SLEEP_TIME;
        }
        //procLock(max,1);
        total -= working_set_size[max];
        working_set_size[max] = 0;
    }
}
void update_refs_and_thrashed(){
    //za thrashing
    for(int id=0;id<NPROC;id++){
        if(thrashing_blocked[id]!=0 || refbits.available_pagetable[id]!=1)continue;
        pagetable_t pt0 = getProcTable(id);
        for(int i=0;i<512;i++){
            if(!pt0[i] || !(pt0[i] & PTE_V))continue;
            pagetable_t pt1 = (pagetable_t)PTE2PA(pt0[i]);
            for(int j=0;j<512;j++){
                if(!pt1[j] || !(pt1[j] & PTE_V)) continue;
                pagetable_t pt2 =(pagetable_t) PTE2PA(pt1[j]);
                for(int k=0;k<512;k++) {
                    pte_t *pte = &pt2[k];
                    if (!(*pte & PTE_T) && ((*pte & PTE_V) || (*pte & PTE_S)) && (*pte & PTE_A) ){
                        *pte |= PTE_T;
                    }
                }
            }
        }
    }
    for(unsigned long i = 0;i<NPROC;i++)
        if(thrashing_blocked[i]){
            thrashing_blocked[i]--;
            if(thrashing_blocked[i]==0) {
                //procLock(i,0);
                printf("process %d can be in scheduler again\n", getpid(i));
                //procLock(i,1);
            }
        }
}
unsigned long is_thrash_blocked(unsigned long val){
    unsigned long ret ;
    acquire(&refbits.ref_lock);
    ret = thrashing_blocked[val];
    release(&refbits.ref_lock);
    return ret;
}
void stop_thrashing(int i){
    thrashing_blocked[i]=0;
}
