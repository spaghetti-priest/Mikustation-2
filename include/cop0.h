#pragma once

#ifndef COP0_H
#define COP0_H

#include "ps2types.h"
#include "r5900Interpreter.h"

enum ExceptionCodes {
    __INTERRUPT               = 0x0,  __TLB_MODIFIED            = 0x1,
    __TLB_REFILL              = 0x2,  __TLB_REFILL_STORE        = 0x3,
    __ADDRESS_ERROR           = 0x4,  __ADDRESS_ERROR_STORE     = 0x5,
    __BUS_ERROR_INSTRUCTION   = 0x6,  __BUS_ERROR_DATA          = 0x7,
    __SYSCALL                 = 0x8,  __BREAKPOINT              = 0x9,
    __RES_INSTRUCTION         = 0xA,  __COP_UNUSABLE            = 0xB,
    __OVER_FLOW               = 0xC,  __TRAP                    = 0xD,
};

enum ExceptionVectors {
    V_RESET_NMI = 0x000, V_TLB_REFILL   = 0x000,
    V_COUNTER   = 0x080, V_DEBUG        = 0x100,
    V_COMMON    = 0x180, V_INTERRUPT    = 0x200
};

typedef struct _Exception_ {
    ExceptionVectors vector;
    ExceptionCodes code;
} Exception;

inline Exception 
get_exception(ExceptionVectors vector, ExceptionCodes code)
{
    Exception r;
    r.vector = vector != V_COMMON ? vector : V_COMMON;
    r.code   = code;
    return r;
}

void handle_exception_level_1 (_R5900Core_*ee, Exception *exc);

inline void set_cop0_status (COP0_Status *s, u32 data) 
{
    s->IE       = (data >> 0)  & 0x1;
    s->EXL      = (data >> 1)  & 0x1;
    s->ERL      = (data >> 2)  & 0x1;
    s->KSU      = (data >> 3)  & 0x3;
    s->IM_2     = (data >> 10) & 0x1;
    s->IM_3     = (data >> 11) & 0x1;
    s->BEM      = (data >> 12) & 0x1;
    s->IM_7     = (data >> 15) & 0x1;
    s->EIE      = (data >> 16) & 0x1;
    s->EDI      = (data >> 17) & 0x1;
    s->CH       = (data >> 18) & 0x1;
    s->BEV      = (data >> 22) & 0x1;
    s->DEV      = (data >> 23) & 0x1;
    s->CU       = (data >> 28) & 0xF;
    s->value    = data;
}

inline void set_cop0_cause (COP0_Cause *c, u32 data) 
{
    c->ex_code          = (data >> 2)  & 0x1F; 
    c->int0_pending     = (data >> 10) & 0x1;
    c->int1_pending     = (data >> 11) & 0x1;
    c->timer_pending    = (data >> 15) & 0x1;
    c->EXC2             = (data >> 16) & 0x3;
    c->CE               = (data >> 28) & 0x3;
    c->BD2              = (data >> 30) & 0x1;
    c->BD               = (data >> 31) & 0x1;
    c->value            = data;
}

#endif