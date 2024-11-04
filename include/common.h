#pragma once
#ifndef COMMON_H
#define COMMON_H
#include "ps2types.h"
#if 0
#include "fmt-10.2.1/include/fmt/core.h"
#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"
#endif
#if 0
//#define NDISASM 1
#ifdef NDISASM
#define syslog(fmt, ...) (void)0
#else
#define syslog(...) fmt::print(__VA_ARGS__ )
#endif

#define errlog(...) fmt::print(__VA_ARGS__)
#endif

typedef struct Range {
    u32 start;
    u32 size;
    
    inline Range(u32 _start, u32 _size) : start(_start), size(_size) {}

    inline bool contains (u32 addr) {
        return (addr >= start) && (addr < start + size);
    }
} Range;
#endif