#pragma once
#ifndef COMMON_H
#if 0
#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"
#endif

#include "fmt-10.2.1/include/fmt/core.h"
#include "fmt-10.2.1/include/fmt/color.h"

// Messy system I know....
#define DISASM 1
// #define NESSENTIAL_LOGS 1 
#define NINTERPRETER_LOGS 1

#ifdef DISASM
    #ifdef NESSENTIAL_LOGS
        #define syslog(fmt, ...) (void)0
    #else
        #define syslog(...) fmt::print(__VA_ARGS__)
    #endif

    #ifdef NINTERPRETER_LOGS
        #define intlog(fmt, ...) (void)0
    #else
        #define intlog(...) fmt::print(__VA_ARGS__)
    #endif
#endif

#define errlog(...) fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, __VA_ARGS__)

/* 
*   @@Incomplete: Create a safe malloc and free system here
*   Also FIFO system here aswell?
*/
typedef struct _FIFO
{
    int             entries;
    unsigned int    current_size;
    unsigned int    max_size;
    void            *head;
    void            *tail;
} Miku_Fifo;

#define COMMON_H
#endif