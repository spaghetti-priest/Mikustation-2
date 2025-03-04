// #include "kernel.h"
// #include "common.h"
#include "gs/gs.h"

/** Thread status */
#define THREAD_RUN         0x01
#define THREAD_READY       0x02
#define THREAD_WAIT        0x04
#define THREAD_SUSPEND     0x08
#define THREAD_WAITSUSPEND 0x0c
#define THREAD_DORMANT     0x10

/** Thread WAIT Status */
#define THREAD_NONE  0 // Thread is not in WAIT state
#define THREAD_SLEEP 1
#define THREAD_SEMA  2

#define MAX_THREADS 256
#define MAX_SEMAPHORES 256
#define MAX_PRIORITY_LEVEL 128

std::vector<Thread> threads(MAX_THREADS);
u32 current_thread_id;

void
SetGsCrt (bool interlaced, int display_mode, bool ffmd)
{
    gs_set_crt(interlaced, display_mode, ffmd);
    syslog("SetGsCrt \n");
}

//AKA: RFU060 or SetupThread
void
InitMainThread (u32 gp, void *stack, s32 stack_size, char *args, s32 root, void *return_)
{
    u32 stack_base = (u32)stack;
    //RDRAM start + RDRAM size which is 32 MB
    u32 rdram_size = 0x00000000 + (32 * 1024 * 1024);
    u32 stack_addr = 0;   

    if (stack_base == -1) {
        stack_addr = rdram_size - stack_size; 
    } else {
        stack_addr = stack_base + stack_size;
    }

    //@Note: ??? Find out what this is 
    stack_addr -= 0x2A0;

    current_thread_id = 0;

    Thread main_thread = {
        .status             = THREAD_RUN,
        .stack              = (void*)stack_addr,
        .stack_size         = stack_size,
        .init_priority      = 0,
        .current_priority   = 0,
        .thread_id          = current_thread_id,
    };
    
    threads.push_back(main_thread);

    return_ = (void*)stack_addr;
    syslog("InitMainThread \n");
}

//AKA: RFU061
void
InitHeap (void *heap, s32 heap_size, void *return_)
{
    auto thread = &threads[0];
    u32 heap_start = (u32)heap;

    if (heap_start == -1) {
        thread->heap_base = thread->stack;
    } else {
        thread->heap_base = (void*)(heap_start + heap_size);
    }

    return_ = thread->heap_base;
    syslog("InitHeap\n");
}

void
FlushCache() 
{
    // @@Cleanup: Shut up for now
    // syslog("FlushCache\n");
}

void 
GsPutIMR (u64 imr)
{
    gs_write_64_priviledged(0x12001010, imr);
    syslog("GSPutIMR\n");
}
