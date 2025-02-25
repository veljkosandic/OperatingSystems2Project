//
// Created by veljkosandic on 2.1.24..
//
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#define FORK_CNT 30
int
main(int argc, char *argv[]) {
    while (1) {
        int pids[FORK_CNT];
        for (int i = 0; i < FORK_CNT; i++) {
            int pid = fork();
            pids[i] = pid;
            if (pids[i] == 0) {
                int j = 0;
                for (; j <500; j++) {
                    uint64 a = (uint64) sbrk(4096);
                    if (a == 0xffffffffffffffff) {
                        break;
                    }
                    // modify the memory to make sure it's really allocated.
                    *(char *) (a + 4096 - 1) = 1;
                }
                exit(0);
            }
        }
        for (int i = 0; i < FORK_CNT; i++) {
            wait(&pids[i]);
        }
        printf("Test ce se ponoviti uskoro...\n");
        sleep(100);
    }
}