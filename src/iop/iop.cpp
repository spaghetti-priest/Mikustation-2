#include <assert.h>

#include "fmt-10.2.1/include/fmt/core.h"

#include "../include/common.h"
#include "../include/iop/iop.h"
#include "../include/ps2types.h"
#include "../include/ps2.h"

R5000_Core iop = {0};

u32 INTC_STAT, INTC_MASK, INTC_CTRL;

u32
get_iop_pc()
{
    return iop.pc;
}

u32
get_iop_register(u32 r)
{
    return iop.reg[r];
}

u32 
translate_address(u32 addr)
{
    //KSEG0
    if (addr >= 0x80000000 && addr < 0xA0000000)
        return addr - 0x80000000;
    //KSEG1
    if (addr >= 0xA0000000 && addr < 0xC0000000)
        return addr - 0xA0000000;

    //KUSEG, KSEG2
    return addr;
}

/*******************************************
 * Load Functions
*******************************************/
static u8 
iop_core_load_8 (u32 addr)
{
    return iop_load_8(translate_address(addr));
}

static u16 
iop_core_load_16 (u32 addr)
{
    return iop_load_16(translate_address(addr));
}

static u32 
iop_core_load_32 (u32 addr)
{
    if (addr == 0xfffe0130) {
        //printf("WRITE: KSEG2 cache control\n");
        return 0;
    }

    return iop_load_32(translate_address(addr));
}

/*******************************************
 * Store Functions
*******************************************/
static void 
iop_core_store_8 (u32 addr, u8 value)
{
    iop_store_8(translate_address(addr), value);
}

static void 
iop_core_store_16 (u32 addr, u16 value)
{
    iop_store_16(translate_address(addr), value);
}

static  void 
iop_core_store_32 (u32 addr, u32 value)
{
    if (addr == 0xfffe0130) {
        //printf("WRITE: KSEG2 cache control\n");
        return;
    }

    iop_store_32(translate_address(addr), value);
}

/*******************************************
 * Load and Store Instructions
*******************************************/
static void 
LB (u32 instruction)
{
	u16 base        = instruction >> 21 & 0x1f;
    u16 rt          = instruction >> 16 & 0x1f;
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = iop.reg[base] + offset;

    iop.reg[rt] = (s64)iop_core_load_8(vaddr);
    syslog("I-LB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);   
}

static void 
LBU (u32 instruction)
{
	u16 base        = instruction >> 21 & 0x1f;
    u16 rt          = instruction >> 16 & 0x1f;
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = iop.reg[base] + offset;
    
    iop.reg[rt] = (u64)iop_core_load_8(vaddr);
    syslog("I-LBU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base); 
} 

static void 
LH (u32 instruction)
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u16 base    = instruction >> 21 & 0x1F;
    u16 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = iop.reg[base] + offset;
    /* @Implementation: Should this be a boolean? */
    u8 low_bit  = vaddr & 0b1;

    if (low_bit != 0) {
    	/* 	
    		@@@Incomplete: This is the exception vector system for the EE and not
    	 	the IOP. The development is for the IOP COP0 is not finished so this
    		just only asserts 
    	 */

        //errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        //Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        //handle_exception_level_1(ee, &exc);
        assert(1);
    }
    iop.reg[rt] = (s64)iop_core_load_16(vaddr);

    syslog("I-LH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base); 
} 

static void 
LHU (u32 instruction)
{
	s32 offset  = (s16)(instruction & 0xFFFF);
    u16 rt      = (instruction >> 16) & 0x1F;
    u16 base    = (instruction >> 21) & 0x1F;
    u32 vaddr   = iop.reg[base] + offset;
    /* @Implementation: Should this be a boolean? */
    u8 low_bit  = vaddr & 0x1;

    if (low_bit != 0) {
    	assert(1);
    }
    iop.reg[rt] = (u64)iop_core_load_16(vaddr);
    syslog("I-LHU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
} 

static void 
LW (u32 instruction)
{
	s32 offset  = (s16)(instruction & 0xFFFF);
    u16 base    = instruction >> 21 & 0x1F;
    u16 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = iop.reg[base] + offset;
    u8 low_bits = vaddr & 0x3;
    
    if (low_bits != 0) {
    	assert(1);
    }
 //   iop.reg[rt] = (s64)ee_load_32(vaddr);
    iop.reg[rt] = (s64)iop_core_load_32(translate_address(vaddr));
    syslog("I-LW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LWL (u32 instruction)
{
    const u32 LWL_MASK[8] =
    {   
    	0x00ffffffULL, 0x0000ffffULL, 0x000000ffULL, 0x00000000ULL,
    };

    const u8 LWL_SHIFT[4] = {24, 16, 8, 0};

    u16 base    		= instruction >> 21 & 0x1f;
    u16 rt      		= instruction >> 16 & 0x1f;
    s16 offset  		= (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = iop.reg[base] + offset;
    u32 aligned_vaddr   = vaddr & ~0x3;
    u32 shift           = vaddr & 0x3;
    
    u32 aligned_dword 	= iop_core_load_32(aligned_vaddr);
    u32 result 			= (iop.reg[rt] & LWL_MASK[shift] | aligned_vaddr << LWL_SHIFT[shift]);
    iop.reg[rt] 		= result;

    syslog("I-LWL [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
}

static void 
LWR (u32 instruction)
{
    const u32 LWR_MASK[8] =
    {   
        0x00000000ULL, 0xff000000ULL, 0xffff0000ULL, 0xffffff00ULL, 
    };

    const u8 LWR_SHIFT[4] = {0, 8, 16, 24};

    u16 base    		= instruction >> 21 & 0x1f;
    u16 rt      		= instruction >> 16 & 0x1f;
    s16 offset  		= (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = iop.reg[base] + offset;
    u32 aligned_vaddr   = vaddr & ~0x3;
    u32 shift           = vaddr & 0x3;
    
    u32 aligned_dword 	= iop_core_load_32(aligned_vaddr);
    u32 result 			= (iop.reg[rt] & LWR_MASK[shift] | aligned_vaddr << LWR_SHIFT[shift]);
    iop.reg[rt] 		= result;

    syslog("I-LDR [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
} 

static void 
LUI (u32 instruction)
{
	//s32 imm     = (s32)(s16)((instruction & 0xFFFF) << 16);
    u32 imm     = ((instruction & 0xFFFF) << 16);
    u16 rt      = instruction >> 16 & 0x1F;
    iop.reg[rt] = imm;

 //   syslog("I-LUI [{:d}] [{:#x}]\n", rt, imm); 
} 

static void 
SB (u32 instruction)
{
	s32 offset  = (s16)(instruction & 0xFFFF);
    u16 rt      = (instruction >> 16) & 0x1F;
    u16 base    = (instruction >> 21) & 0x1F;
    u32 vaddr   = iop.reg[base] + offset;
    s8 value    = (s8)iop.reg[rt];

    iop_core_store_8(vaddr, value);

 //   syslog("I-SB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
} 

static void 
SH (u32 instruction)
{
	s32 offset      = (s16)(instruction & 0xFFFF);
    u16 rt          = (instruction >> 16) & 0x1F;
    u16 base        = (instruction >> 21) & 0x1F;
    u32 vaddr       = iop.reg[base] + offset;
    s16 value       = iop.reg[rt];
    u8 low_bits     = vaddr & 0x1;

    if (low_bits != 0) {
      assert(1);
    }

    iop_core_store_16(vaddr, value);
    syslog("I-SH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SW (u32 instruction)
{
	s16 offset  = (s16)(instruction & 0xFFFF);
    u16 base    = instruction >> 21 & 0x1F;
    u16 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = iop.reg[base] + offset;
    s32 value   = iop.reg[rt];
    u8 low_bits = vaddr & 0x3;
    
    if (low_bits != 0) {
    	assert(1);
    }

    iop_core_store_32(vaddr, value);
    syslog("I-SW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SWL (u32 instruction)
{
	const u32 SWL_MASK[8] =
    {   
    	0x00ffffffULL, 0x0000ffffULL, 0x000000ffULL, 0x00000000ULL,
    };

    const u8 SWL_SHIFT[4] = {24, 16, 8, 0};

    u16 base    		= instruction >> 21 & 0x1f;
    u16 rt      		= instruction >> 16 & 0x1f;
    s16 offset  		= (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = iop.reg[base] + offset;
    u32 aligned_vaddr   = vaddr & ~0x3;
    u32 shift           = vaddr & 0x3;

	u32 aligned_dword 	= ee_load_32(aligned_vaddr);
    u32 result 			= (iop.reg[rt] & SWL_MASK[shift] | aligned_vaddr << SWL_SHIFT[shift]);
    iop_core_store_32(aligned_vaddr, aligned_dword);
    syslog("I-SWL [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
} 

static void 
SWR (u32 instruction)
{	
    const u32 LWR_MASK[8] =
    {   
        0x00000000ULL, 0xff000000ULL, 0xffff0000ULL, 0xffffff00ULL, 
    };

    const u8 LWR_SHIFT[4] = {0, 8, 16, 24};

    u16 base    		= instruction >> 21 & 0x1f;
    u16 rt      		= instruction >> 16 & 0x1f;
    s16 offset  		= (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = iop.reg[base] + offset;
    u32 aligned_vaddr   = vaddr & ~0x3;
    u32 shift           = vaddr & 0x3;

	u32 aligned_dword 	= ee_load_32(aligned_vaddr);
    u32 result 			= (iop.reg[rt] & LWR_MASK[shift] | aligned_vaddr << LWR_SHIFT[shift]);
    iop_core_store_32(aligned_vaddr, aligned_dword);
    syslog("I-SWL [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
} 

/*******************************************
* Computational ALU Instructions
* Arithmetic
• Logical
• Shift
• Multiply
• Divide
*******************************************/

static void
ADD (u32 instruction)
{
	u16 rd = instruction >> 11 & 0x1F;
	u16 rt = instruction >> 16 & 0x1F;
	u16 rs = instruction >> 21 & 0x1F;
	iop.reg[rd] = iop.reg[rs] + iop.reg[rt];
	
	/* @@Incomplete: No overflow detection for now */
    syslog("I-ADD [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);   
} 

static void
ADDI (u32 instruction)
{
	u16 imm = instruction & 0xFFFF;
	u16 rt 	= instruction >> 16 & 0x1F;
	u16 rs 	= instruction >> 21 & 0x1F;
	iop.reg[rt] = imm + iop.reg[rs];

	/* @@Incomplete: No overflow detection for now */
    syslog("I-ADDI [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);  
} 

static void
ADDU (u32 instruction)
{
	u16 rd = instruction >> 11 & 0x1F;
	u16 rt = instruction >> 16 & 0x1F;
	u16 rs = instruction >> 21 & 0x1F;
	iop.reg[rd] = iop.reg[rs] + iop.reg[rt];
    syslog("I-ADDU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void
ADDIU (u32 instruction)
{
	u16 rt 	= instruction >> 16 & 0x1F;
	u16 rs 	= instruction >> 21 & 0x1F;
	auto imm = (u32)(s16)(instruction & 0xFFFF);
	
	iop.reg[rt] = imm + iop.reg[rs];
 //   syslog("I-ADDIU: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void
SUB (u32 instruction)
{
	u16 rd = instruction >> 11 & 0x1F;
	u16 rt = instruction >> 16 & 0x1F;
	u16 rs = instruction >> 21 & 0x1F;
	iop.reg[rd] = iop.reg[rs] - iop.reg[rt];

	/* @@Incomplete: No overflow detection for now */
    syslog("I-SUB [{:d}] [{:d}] [{:d}]\n", rd, rs, rt); 
}

static void
SUBU (u32 instruction)
{
	u16 rd         = instruction >> 11 & 0x1F;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rd]    = iop.reg[rs] - iop.reg[rt];
    
    syslog("I-SUBU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt); 
}

static void 
AND(u32 instruction)
{
	u16 rd         = instruction >> 11 & 0x1F;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rd]    = iop.reg[rs] & iop.reg[rt];
    
    syslog("I-AND [{:d}] [{:d}] [{:d}] \n", rd, rs, rt);
}

static void 
ANDI (u32 instruction)
{
	u16 imm        = instruction & 0xFFFF;
	u16 rt 	       = instruction >> 16 & 0x1F;
	u16 rs 	       = instruction >> 21 & 0x1F;
	iop.reg[rt]    = imm & iop.reg[rs];
    
    syslog("I-ANDI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void
OR (u32 instruction)
{
	u16 rd         = instruction >> 11 & 0x1F;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rd]    = iop.reg[rs] | iop.reg[rt];
    
    syslog("I-OR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void
ORI (u32 instruction)
{
	u16 imm        = instruction & 0xFFFF;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rt]    = imm | iop.reg[rs];

    syslog("I-ORI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void
XOR (u32 instruction)
{
	u16 rd = instruction >> 11 & 0x1F;
	u16 rt = instruction >> 16 & 0x1F;
	u16 rs = instruction >> 21 & 0x1F;
	iop.reg[rd] = iop.reg[rs] ^ iop.reg[rt];
	syslog("I-XOR\n");
}

static void
XORI (u32 instruction)
{
	u16 imm        = instruction & 0xFFFF;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rt]    = imm ^ iop.reg[rs];

    syslog("I-XORI [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);;
}

static void
NOR (u32 instruction)
{
	u16 rd         = instruction >> 11 & 0x1F;
	u16 rt         = instruction >> 16 & 0x1F;
	u16 rs         = instruction >> 21 & 0x1F;
	iop.reg[rd]    = ~(iop.reg[rs] | iop.reg[rt]);

    syslog("I-NOR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
MULT (u32 instruction)
{
	u16 rt 				= instruction >> 16 & 0x1F;
	u16 rs 				= instruction >> 21 & 0x1F;
	const auto product 	= (s64)(s32)(iop.reg[rs] * iop.reg[rt]);

	iop.LO = product & 0xFFFFFFFF;
	iop.HI = product >> 32;

    syslog("I-MULT [{:d}] [{:d}]\n", rs, rt);
} 

static void 
MULTU (u32 instruction)
{
	u16 rt 				= instruction >> 16 & 0x1F;
	u16 rs 				= instruction >> 21 & 0x1F;
	const auto product 	= (u64)(iop.reg[rs] * iop.reg[rt]);
	
	iop.LO = product & 0xFFFFFFFF;
	iop.HI = product >> 32;

    syslog("I-MULTU [{:d}] [{:d}]\n", rs, rt);
}   

static void 
DIV (u32 instruction)
{
	u16 rt 	= instruction >> 16 & 0x1F;
	u16 rs 	= instruction >> 21 & 0x1F;

	iop.LO = (s32)(iop.reg[rs] / iop.reg[rt]);
	iop.HI = (s32)(iop.reg[rs] & iop.reg[rt]);
	
    /* @Incomplete: Result is undefined if rt is zero */
    syslog("I-DIV [{:d}] [{:d}]\n", rs, rt);
} 

static void 
DIVU (u32 instruction)
{
	u16 rt 	= instruction >> 16 & 0x1F;
	u16 rs 	= instruction >> 21 & 0x1F;

    //@error: handle divison by zero
	iop.LO = (iop.reg[rs] / iop.reg[rt]);
	iop.HI = (iop.reg[rs] & iop.reg[rt]);
	
    /* @Incomplete: Result is undefined if rt is zero */
    syslog("I-DIVU [{:d}] [{:d}]\n", rs, rt);
}

static void 
SRL (u32 instruction)
{
    u16 sa 		= instruction >> 6 & 0x1F;
    u16 rd 		= instruction >> 11 & 0x1F;
    u16 rt 		= instruction >> 16 & 0x1F;
    u16 result 	= (iop.reg[rt] >> sa);
    
    iop.reg[rd] = (s32)result;

    syslog("I-SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
} 

static void 
SRLV (u32 instruction)
{
    printf("SRLV\n");
} 

static void 
SRA (u32 instruction)
{
    s16 sa      = instruction >> 6 & 0x1F;
    u16 rd      = instruction >> 11 & 0x1F;
    u16 rt      = instruction >> 16 & 0x1F;
    s16 result  = (iop.reg[rt]) >> sa;
    
    iop.reg[rd] = result;

    syslog("I-SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
} 

static void 
SRAV (u32 instruction)
{
    u16 rs = (instruction >> 21) & 0x1F;
    u16 rt = (instruction >> 16) & 0x1F;
    u16 rd = (instruction >> 11) & 0x1F;
    u16 sa = iop.reg[rs] & 0x1F;
    
    auto result 	= (s32)(iop.reg[rt] >> sa);
    iop.reg[rd] 	= (s64)result;
    
    syslog("I-SRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);

}

static void 
SLL (u32 instruction)
{
    u16 sa = instruction >> 6 & 0x1F;
    u16 rd = instruction >> 11 & 0x1F;
    u16 rt = instruction >> 16 & 0x1F;
    iop.reg[rd] = (u32)((s16)iop.reg[rt] << sa);
    
    syslog("I-SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
} 

static void 
SLLV (u32 instruction)
{
    u16 rd = instruction >> 11 & 0x1F;
    u16 rt = instruction >> 16 & 0x1F;
    u16 rs = instruction >> 21 & 0x1F;
    u16 sa = iop.reg[rs] & 0x1F;

    iop.reg[rd] = (s32)(iop.reg[rt] << sa);
    //syslog("I-SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SLT (u32 instruction)
{
    u16 rd = instruction >> 11 & 0x1F;
    u16 rt = instruction >> 16 & 0x1F;
    u16 rs = instruction >> 21 & 0x1F;

    auto result = iop.reg[rs] < iop.reg[rt] ? 1 : 0;
    iop.reg[rd] = result;

    syslog("I-SLT [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
} 

static void 
SLTI (u32 instruction)
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u16 rt  = instruction >> 16 & 0x1F;
    u16 rs  = instruction >> 21 & 0x1F;

    auto result = (iop.reg[rs] < (s32)imm) ? 1 : 0;
    iop.reg[rt] = result;
	
    syslog("I-SLTI [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
}

static void 
SLTU (u32 instruction)
{
    u16 rs = (instruction >> 21) & 0x1F;
    u16 rt = (instruction >> 16) & 0x1F;
    u16 rd = (instruction >> 11) & 0x1F;
    uint32_t result = (iop.reg[rs] < iop.reg[rt]) ? 1 : 0;
    iop.reg[rd] = result;
	
    syslog("I-SLTU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);

} 

static void 
SLTIU (u32 instruction)
{
    s32 imm = (s16)(instruction & 0xFFFF);
    u16 rs = instruction >> 21 & 0x1f;
    u16 rt = instruction >> 16 & 0x1f;

    u32 result = (iop.reg[rs] < imm) ? 1 : 0;
    iop.reg[rt] = result;

    syslog("I-SLTIU [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);
}

/*******************************************
 * Jump and Branch Instructions
*******************************************/
//void op_bcond();
static inline void 
jump_to (s32 address) 
{
    iop.branch_pc    = address;
    iop.is_branching = true;
    iop.delay_slot   = 1;
}

static inline void 
branch (bool is_branching, u32 offset) 
{
    if (is_branching) {
        iop.is_branching = true;
        iop.branch_pc    = iop.pc + 4 + offset;
        iop.delay_slot   = 1;
    }
}

static void 
J (u32 instruction)
{
	u32 instr_index = (instruction & 0x3FFFFFF);
    u32 offset      = ((iop.pc + 4) & 0xF0000000) + (instr_index << 2);
    jump_to(offset);
    
    syslog("I-J [{:#x}]\n", offset);
} 

static void 
JR (u32 instruction)
{
	u16 source = instruction >> 21 & 0x1F;
    //@Temporary: check the 2 lsb if 0
    jump_to(iop.reg[source]);
	
     syslog("I-JR source: [{:d}] pc_dest: [{:#x}]\n", source, iop.reg[source]);
} 

static void 
JAL (u32 instruction)
{
	u32 instr_index     = (instruction & 0x3FFFFFF);
    u32 target_address  = ((iop.pc + 4) & 0xF0000000) + (instr_index << 2);
    iop.reg[31]  		= iop.pc + 8;
    jump_to(target_address); 

    syslog("I-JAL [{:#x}] \n", target_address);  
}

static void 
JALR (u32 instruction)
{
	u16 rs = instruction >> 21 & 0x1f;
    u16 rd = instruction >> 11 & 0x1f;
    u32 return_addr = iop.pc + 8;

    if (rd != 0) { iop.reg[rd] = return_addr; } 
    else 		 { iop.reg[31] = return_addr; }
    jump_to(iop.reg[rs]);
	
    syslog("I-JALR [{:d}]\n", rs);
} 

static void 
BEQ (u32 instruction)
{
	s16 imm = (s16)(instruction & 0xFFFF);
    imm     = imm << 2;
    u16 rs  = instruction >> 21 & 0x1f;
    u16 rt  = instruction >> 16 & 0x1f;

    bool condition = iop.reg[rs] == iop.reg[rt];
    branch(condition, imm);

 //   syslog("I-BEQ [{:d}] [{:d}] [{:#x}] \n", rs, rt, imm);
} 

static void 
BLEZ (u32 instruction)
{
	s16 offset  = (s16)((instruction & 0xFFFF) << 2);
    u16 rs      = instruction >> 21 & 0x1F;

    bool condition = iop.reg[rs] <= 0;
    branch(condition, offset);

    syslog("I-BLEZ [{:d}] [{:#x}]\n", rs, offset); 
} 

static void 
BGTZ (u32 instruction)
{
	s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u16 rs      = instruction >> 21 & 0x1F;

    bool condition = iop.reg[rs] > 0;
    branch(condition, offset);

    syslog("I-BGTZ [{:d}] [{:#x}] \n", rs, offset);
}

static void 
BNE (u32 instruction)
{
	s32 imm     = (s16)(instruction & 0xFFFF);
    imm         = (imm << 2);
    u16 rs      = instruction >> 21 & 0x1F;
    u16 rt      = instruction >> 16 & 0x1F;

    bool condition = iop.reg[rs] != iop.reg[rt];
    branch(condition, imm);

    syslog("I-BNE [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
}

/*******************************************
 * IOP specific instructions
*******************************************/
static void
MFHI (u32 instruction)
{
    u16 rd          = instruction >> 11 & 0x1F;
    iop.reg[rd]   = iop.HI;
    
    syslog("I-MFHI [{:d}]\n", rd);
}

static void
MTHI (u32 instruction)
{
    u16 rs = instruction >> 21 & 0x1F;
    iop.HI = iop.reg[rs];
    
    syslog("I-MTHI [{:d}]\n", rs);
}

static void
MFLO (u32 instruction)
{
    u16 rd        = instruction >> 11 & 0x1F;
    iop.reg[rd]   = iop.LO;
    
    syslog("I-MFLO[{:d}]\n", rd);
}

static void
MTLO (u32 instruction)
{
    u32 rs = instruction >> 21 & 0x1F;
    iop.LO = iop.reg[rs];
    
    syslog("I-MTHI [{:d}]\n", rs);
}

static void
RFE (u32 instruction)
{
    iop.cop0.status.current_interrupt    = iop.cop0.status.previous_interrupt;
    iop.cop0.status.current_kernel_mode  = iop.cop0.status.previous_kernel_mode;
    iop.cop0.status.previous_interrupt   = iop.cop0.status.old_interrupt;
    iop.cop0.status.previous_kernel_mode = iop.cop0.status.old_kernel;
    syslog("I-RFE \n");
}

/*******************************************
 * IOP COP0 instructions
*******************************************/
static void
MFC0 (u32 instruction)
{
    u16 rd          = instruction >> 11 & 0x1F;
    u16 rt          = instruction >> 16 & 0x1F;
    iop.reg[rt]     = iop.cop0.r[rd];
    
    syslog("I-MFC0 GPR: [{:d}] COP0: [{:d}]\n", rt, rd);
}

static void
MTC0 (u32 instruction)
{
    u16 rd          = instruction >> 11 & 0x1F;
    u16 rt          = instruction >> 16 & 0x1F;
    iop.cop0.r[rd]  = iop.reg[rt];
    
    syslog("I-MTC0 GPR: [{:d}] COP0: [{:d}]\n", rt, rd);
}

/* Exception instructions. */
//void op_break(); void op_syscall();

enum OPCODES : int {
	IOP_SPECIAL = 0x00,
	IOP_COP0 = 0x10,
};

static void 
decode_and_execute (u32 instruction) 
{
    if (instruction == 0x00000000) return;
	u32 opcode = instruction >> 26;

	switch(opcode)
	{
		case IOP_SPECIAL:
		{
			int special = instruction & 0x1F;
			switch(special)
			{
				case 0x00: SLL(instruction); 	break;
				case 0x02: SRL(instruction); 	break;
				case 0x03: SRA(instruction); 	break;
				case 0x04: SLLV(instruction); 	break;
				case 0x06: SRLV(instruction); 	break;
				case 0x07: SRAV(instruction); 	break;

				case 0x08: JR(instruction); 	break;
				case 0x09: JALR(instruction); 	break;

				case 0x10: MFHI(instruction); 	break;
				case 0x11: MTHI(instruction); 	break;
				case 0x12: MFLO(instruction); 	break;
				case 0x13: MTLO(instruction); 	break;

				case 0x18: MULT(instruction); 	break;
				case 0x19: MULTU(instruction); 	break;
				case 0x1A: DIV(instruction); 	break;
				case 0x1B: DIVU(instruction); 	break;
				case 0x20: ADD(instruction); 	break;
				case 0x21: ADDU(instruction); 	break;
				case 0x22: SUB(instruction); 	break;
				case 0x23: SUBU(instruction); 	break;

				case 0x24: AND(instruction);	break;
				case 0x25: OR(instruction); 	break;
				case 0x27: NOR(instruction);	break;
				case 0x26: XOR(instruction);	break;

				case 0x2A: SLT(instruction); 	break;
				case 0x2B: SLTU(instruction); 	break;
			}
		} break;

		case IOP_COP0:
		{
			int cop0 = instruction >> 21 & 0x1F;
			switch(cop0)
			{
				case 0x00: MFC0(instruction); 	break;
				case 0x04: MTC0(instruction); 	break;
				case 0x10: RFE(instruction);	break;
			}
		} break;

		case 0x02: J(instruction); 		break;
		case 0x03: JAL(instruction); 	break;
		case 0x04: BEQ(instruction); 	break;
		case 0x05: BNE(instruction); 	break;
		case 0x06: BLEZ(instruction); 	break;
		case 0x07: BGTZ(instruction); 	break;
		
		case 0x08: ADDI(instruction); 	break;
		case 0x09: ADDIU(instruction); 	break;

		case 0x0C: ANDI(instruction); 	break;
		case 0x0D: ORI(instruction);  	break;
		case 0x0E: XORI(instruction); 	break;

		case 0x0F: LUI(instruction);	break;
		case 0x20: LB(instruction);		break;
		case 0x21: LH(instruction);		break;
		case 0x22: LWL(instruction);	break;
		case 0x23: LW(instruction);		break;
		case 0x24: LBU(instruction);	break;
		case 0x26: LWR(instruction);	break;
		case 0x28: SB(instruction);		break;
		case 0x29: SH(instruction);		break;
		case 0x2A: SWL(instruction);	break;
		case 0x2B: SW(instruction);		break;
		case 0x2E: SWR(instruction);	break;
		
		case 0x0A: SLTI(instruction); 	break;
		case 0x0B: SLTIU(instruction); 	break;
	};
}

void
iop_cycle() 
{
    if (iop.delay_slot > 0) iop.delay_slot -= 1;
	if (iop.is_branching) {
		iop.is_branching      = false;
        iop.next_instruction  = iop_core_load_32(iop.pc);
        decode_and_execute(iop.next_instruction);
        iop.pc = iop.branch_pc;
	}
	iop.current_instruction = iop_core_load_32(iop.pc);
	iop.next_instruction 	= iop_core_load_32(iop.pc + 4);
	decode_and_execute(iop.current_instruction);
	iop.pc += 4;
    iop_cop0_write_status(&iop.cop0.status, iop.reg[12]);
    iop_cop0_write_cause(&iop.cop0.cause, iop.reg[13]);
}

void
iop_reset() 
{
	printf("Resetting IOP Core\n");
    iop = {
		.pc = 0xbfc00000,
		.current_cycle = 0,
	};
    iop.cop0.r[15] = 0x0F;
}