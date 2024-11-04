#ifndef COP1_H
#define COP1_H

#include "r5900Interpreter.h"
#include "../ps2types.h"

union FPRreg {
	float f;
	u32 u;
	s32 s; 
};
	
// @@Rename: Possibly rename this to FPU registers.
union COP1_Registers {
    struct {
        FPRreg
            f0, f1, f2, f3, // return values
            f4, f5, f6, f7, f8, f9, f10, f11, // temporary registers
            f12, f13, f14, f15, f16, f17, f18, f19, // argument registers
            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31; // saved registers
    };
    FPRreg fpr[32];
    u32 fcr0; // reports implementation and revision of fpu
    u32 fcr31; // control register, stores status flags
    FPRreg ACC; /* accumulator register */
    u32 ACCflag;
};

f32 cop1_getFPR(u32 index);
void cop1_reset();
void cop1_decode_and_execute(R5900_Core *ee, u32 instruction);
#endif