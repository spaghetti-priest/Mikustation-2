#pragma once

#ifndef PS2TYPE_H
#define PS2TYPE_H

// #include <cstring>

//#define MIN(a, b) (a < b ? a : b)
//#define MIN3(a, b, c) (MIN(MIN(a, b), c))
//#define MAX(a, b) (a > b ? a : b)
//#define MAX3(a, b, c) (MAX(MAX(a, b), c))

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
} u128;// __attribute__((aligned(16)));

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
} s128;// __attribute__((aligned(16)));


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

typedef struct _Vector3_ {
    s32 x, y, z;
} v3;

typedef union _Vector2_ {
    struct {
        s32 x, y;
    };
    struct {
        s32 u, v;
    };
} v2;

typedef union _Vector4_ {
    struct {
        s32 x, y, z, w;
    };
    struct {
        s32 r, g, b, a;
    };
} v4;

typedef struct Instruction_t {
    u8 opcode;
    u8 rd;
    u8 rt;
    u8 rs;
    u8 base;
    u32 sa;

    u16 imm;
    s16 sign_imm;
    u16 offset;
    s16 sign_offset;
    u32 instr_index;
} Instruction;

#endif