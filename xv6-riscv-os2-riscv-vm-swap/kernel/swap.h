//
// Created by veljkosandic on 24.12.23..
//

#ifndef XV6_RISCV_OS2_RISCV_VM_SWAP_SWAP_H
#define XV6_RISCV_OS2_RISCV_VM_SWAP_SWAP_H
#include "spinlock.h"
#include "riscv.h"
#include "param.h"
//Velicina bloka u swap - 1 KB
//Velicina bloka u OM - 4 KB
//Velicina swap - 16 MB
#define SWAP_BLOCK_CNT  4096
#define SWAP_BLOCK_SIZE 1024
#define MEMORY_BLOCK_SIZE  1024*4
#define SWAP_MEM_CNT  4
#define CHAR_SIZE 8
struct swap_disk{
    struct spinlock swap_lock;
    char alloc[SWAP_BLOCK_CNT/CHAR_SIZE];
    unsigned long earliestBlock;
};
struct RefBits{
    unsigned long refBits[FRAME_CNT];
    unsigned long ptr,max;
    struct spinlock ref_lock;
    char available_pagetable[NPROC];
};
#endif //XV6_RISCV_OS2_RISCV_VM_SWAP_SWAP_H
