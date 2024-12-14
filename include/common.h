#pragma once
#ifndef COMMON_H
#define COMMON_H
#include "ps2types.h"
#if 0
#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"
#endif

#include "fmt-10.2.1/include/fmt/core.h"
#include "fmt-10.2.1/include/fmt/color.h"

#define NDISASM 1
#ifdef NDISASM
#define syslog(fmt, ...) (void)0
#else
#define syslog(...) fmt::print(__VA_ARGS__)
#endif

#define errlog(...) fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, __VA_ARGS__)

typedef struct Range {
    u32 start;
    u32 size;
    
    inline Range(u32 _start, u32 _size) : start(_start), size(_size) {}

    inline bool contains (u32 addr) {
        return (addr >= start) && (addr < start + size);
    }
} Range;

/* 
*   @@Incomplete: Create a safe malloc and free system here
*   Also FIFO system here aswell?
*/
typedef struct _FIFO
{
    int entries;
    u32 current_size;
    u32 max_size;
    void *head;
    void *tail;
} FIFO;

#endif