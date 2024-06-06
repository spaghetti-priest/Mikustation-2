#pragma once

#ifndef PS2TYPE_H
#define PS2TYPE_H

#include <cstring>

/*
//#define NDISASM 1
#ifdef NDISASM
#define syslog(fmt, ...) (void)0
#else
#define syslog(...) fmt::print(__VA_ARGS__ )
#endif

//@@Incomplete: Make this bold red text to indicate an error
#define errlog(...) fmt::print(__VA_ARGS__)
*/

//#define min(a,b) (a < b ? a : b)
//#define max(a,b) (a > b ? a : b)
#define KILOBYTES(b) (b * 1024)
#define MEGABYTES(b) (KILOBYTES(b) * 1024)
#define GIGABYTES(b) (MEGABYTES(b) * 1024) 

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;

typedef float f32;
typedef double f64;

#define global_variable static
//#define function static

// @Implementation @Portability: The clang compiler actually has a u128 value, MSVC and others do not, so I should
// change this in the future 
//using u128 = unsigned __int128;
//using s128 = __int128;
/*
//@@Note: This was inspired by PCSX2 type system.
*/
typedef union _u128_ {
    struct {
        u64 hi;
        u64 lo;
    };

    u64 _64[2];
    u32 _32[4];
    u16 _16[8];
    u8  _8[12];
    inline bool operator ==(_u128_ r)
    {
        return ( hi == r.hi || lo == r.lo );
    }

    inline bool operator !=(_u128_ r)
    {
        return ( hi != r.hi || lo != r.lo );
    } 
} u128;

typedef struct _int128_t_ {
    s64 hi;
    s64 lo;

    inline bool operator ==(_int128_t_ r)
    {
        return ( hi == r.hi || lo == r.lo );
    }

    inline bool operator !=(_int128_t_ r)
    {
        return ( hi != r.hi || lo != r.lo );
    } 
} s128;


// MIPS IV word is 32 bits 
union CPU_Types {
    u128 UQ; //quad
    u64  UD[2]; // double
    u32  UW[4]; // word
    u16  UH[8]; // hword
    u8   UB[16]; //byte

    s128 SQ;
    s64  SD[2];
    s32  SW[4];
    s16  SH[8];
    s8   SB[16];
};

// @@Note: Removable: Putting this here since there is no dedicated file system yet
inline const char *
strip_file_path ( const char *s ) {
#if _WIN32 || _WIN64
    const char* last = strrchr(s, '\\');

#else
    const char* last = strrchr(s, '/');
#endif
   last += 1;
   return last;
}

#endif