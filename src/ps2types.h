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

typedef union V2I V2I;
union V2I {
  struct {
   s32 x, y;
  };
  struct {
   s32 u, v;
  };
};

inline V2I v2i (s32 x, s32 y)
{
   V2I r = {};
   r.x = x;
   r.y = y;
   return r;
}

typedef union V3I V3I;
union V3I {
  struct {
   s32 x, y, z;
  };
  struct {
   s32 r, g, b;
  };
};

typedef union V3 V3;
union V3 {
  struct {
   f32 x, y, z;
  };
  struct {
   f32 r, g, b;
  };
};

inline V3I v3i (s32 x, s32 y, s32 z)
{
   V3I r = {};
   r.x = x;
   r.y = y;
   r.z = z;
   return r;
}

inline V3 v3 (f32 x, f32 y, f32 z)
{
   V3 r = {};
   r.x = x;
   r.y = y;
   r.z = z;
   return r;
}

typedef union V4I V4I;
union V4I {
  struct {
   s32 x, y, z, w;
  };
  struct {
   s32 r, g, b, a;
  };
};

typedef union V4 V4;
union V4 {
  struct {
   f32 x, y, z, w;
  };
  struct {
   f32 r, g, b, a;
  };
};

inline V4I v4i (s32 x, s32 y, s32 z, s32 w)
{
   V4I r = {};
   r.x = x;
   r.y = y;
   r.z = z;
   r.w = w;
   return r;
}

inline V4 v4 (f32 x, f32 y, f32 z, f32 w)
{
   V4 r = {};
   r.x = x;
   r.y = y;
   r.z = z;
   r.w = w;
   return r;
}

inline V3 convert_s32_to_f32_v3 (V3I in)
{
   V3 out = {};
   out.x = (float)in.x;
   out.y = (float)in.y;
   out.z = (float)in.z;
   return out;
}

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