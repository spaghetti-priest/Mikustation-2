#pragma once

#ifndef KERNEL_H
#define KERNEL_H

// From: Ps2SDK
// Used as argument for CreateThread, ReferThreadStatus
typedef struct t_ee_thread_param
{
    int status;           // 0x00
    void *func;           // 0x04
    void *stack;          // 0x08
    int stack_size;       // 0x0C
    void *gp_reg;         // 0x10
    int initial_priority; // 0x14
    int current_priority; // 0x18
    u32 attr;             // 0x1C
    u32 option;           // 0x20 Do not use - officially documented to not work.
} thread_param;

typedef struct t_ee_sema_param
{
    int count,
        max_count,
        init_count,
        wait_threads;
    u32 attr,
        option;
} semaphore_param;

struct TCB //Internal thread structure
{
    struct TCB *prev;
    struct TCB *next;
    int status;
    void *func;
    void *current_stack;
    void *gp_reg;
    short current_priority;
    short init_priority;
    int wait_type; //0=not waiting, 1=sleeping, 2=waiting on semaphore
    int sema_id;
    int wakeup_count;
    int attr;
    int option;
    void *_func; //???
    int argc;
    char **argv;
    void *initial_stack;
    int stack_size;
    int *root; //function to return to when exiting thread?
    void *heap_base;
};

struct thread_context //Stack context layout
{
    u32 sa_reg;  // Shift amount register
    u32 fcr_reg;  // FCR[fs] (fp control register)
    u32 unkn;
    u32 unused;
    u128 at, v0, v1, a0, a1, a2, a3;
    u128 t0, t1, t2, t3, t4, t5, t6, t7;
    u128 s0, s1, s2, s3, s4, s5, s6, s7, t8, t9;
    u64 hi0, hi1, lo0, lo1;
    u128 gp, sp, fp, ra;
    u32 fp_regs[32];
};

struct semaphore //Internal semaphore structure
{
    struct sema *free; //pointer to empty slot for a new semaphore
    int count;
    int max_count;
    int attr;
    int option;
    int wait_threads;
    struct TCB *wait_next, *wait_prev;
};

//@@Incomplete: My own incomplete interpretation of a BIOS thread
struct Thread
{
    int status;
    void *stack;
    int stack_size;
    int init_priority;
    int current_priority;
    u32 thread_id;
    void *heap_base;
};

void    SetGsCrt(bool interlaced, int display_mode, bool ffmd);
void    InitMainThread(u32 gp, void *stack, int stack_size, char *args, int root, void *return_); //AKA: RFU060 or SetupThread
void    InitHeap(void *heap, int heap_size, void *return_); //AKA: RFU061
void    FlushCache(); 
void    GsPutIMR(u64 imr);

#endif