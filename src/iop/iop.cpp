// #include "common.h"
// #include "iop.h"
// #include "ps2.h"

R5000_Core iop = {0};

//@@Temporary: This are not permanant
//u32 IOP_INTC_STAT, IOP_INTC_MASK, IOP_INTC_CTRL;

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
static inline void check_addresss_error_exception() {};

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

    Instruction i = {
        .rd          = (u8)((instruction >> 11) & 0x1F),
        .rt          = (u8)((instruction >> 16) & 0x1F),
        .rs          = (u8)((instruction >> 21) & 0x1F),
        .sa          = (u32)((instruction >> 6)  & 0x1F),

        .imm         = (u16)(instruction & 0xFFFF),
        .sign_imm    = (s16)(instruction & 0xFFFF),
        .offset      = (u16)(instruction & 0xFFFF),
        .sign_offset = (s16)(instruction & 0xFFFF), // No real difference between imm and offset just naming conveince
        .instr_index = (u32)(instruction & 0x3FFFFFF),
    };

	switch(opcode)
	{
/*
*-----------------------------------------------------------------------------
* Special Instructions
*-----------------------------------------------------------------------------
*/
		case IOP_SPECIAL:
		{
			int special = instruction & 0x1F;
			switch(special)
			{
				case 0x00: 
                {
                    iop.reg[i.rd] = (u32)((s16)iop.reg[i.rt] << i.sa);
                    intlog("I-SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, i.sa);
                } break;

				case 0x02: 
                {
                    u16 result      = (iop.reg[i.rt] >> i.sa);
                    iop.reg[i.rd]   = (s32)result;

                    intlog("I-SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, i.sa);
                } break;

				case 0x03: 
                {
                    s16 result      = (iop.reg[i.rt]) >> (s16)i.sa; // ?? cast by s16 
                    iop.reg[i.rd]   = result;

                    intlog("I-SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, i.sa);
                } break;

				case 0x04: 
                {
                    u16 sa          = iop.reg[i.rs] & 0x1F;
                    iop.reg[i.rd]   = (s32)(iop.reg[i.rt] << sa);
                    //intlog("I-SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
                } break;

				case 0x06: 
                {
                    printf("SRLV\n");
                } break;

				case 0x07: 
                {
                    u16 sa          = iop.reg[i.rs] & 0x1F;
                    auto result     = (s32)(iop.reg[i.rt] >> sa);
                    iop.reg[i.rd]   = (s64)result;
                    
                    intlog("I-SRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);
                } break;

				case 0x08: 
                {
                    //@Temporary: check the 2 lsb if 0
                    jump_to(iop.reg[i.rs]);
                    intlog("I-JR source: [{:d}] pc_dest: [{:#x}]\n", i.rs, iop.reg[i.rs]);
                } break;

				case 0x09: 
                {
                    //@Temporary: check the 2 lsb if 0
                    jump_to(iop.reg[i.rs]);
                    intlog("I-JR source: [{:d}] pc_dest: [{:#x}]\n", i.rs, iop.reg[i.rs]);
                } break;

				case 0x10: 
                {
                    iop.reg[i.rd] = iop.HI;
                    intlog("I-MFHI [{:d}]\n", rd);
                } break;

				case 0x11: 
                {
                    iop.HI = iop.reg[i.rs];
                    intlog("I-MTHI [{:d}]\n", i.rs);
                } break;

				case 0x12: 
                {
                    iop.reg[i.rd] = iop.LO;
                    intlog("I-MFLO[{:d}]\n", rd);
                } break;

				case 0x13: 
                {
                    iop.LO = iop.reg[i.rs];
                    intlog("I-MTHI [{:d}]\n", i.rs);
                } break;

				case 0x18: 
                {
                    const auto product = (s64)(s32)(iop.reg[i.rs] * iop.reg[i.rt]);

                    iop.LO = product & 0xFFFFFFFF;
                    iop.HI = product >> 32;

                    intlog("I-MULT [{:d}] [{:d}]\n", rs, rt);
                } break;

				case 0x19: 
                {
                    const auto product  = (u64)(iop.reg[i.rs] * iop.reg[i.rt]);
                    
                    iop.LO = product & 0xFFFFFFFF;
                    iop.HI = product >> 32;

                    intlog("I-MULTU [{:d}] [{:d}]\n", i.rs, rt);
                } break;

				case 0x1A: 
                {
                    iop.LO = (s32)(iop.reg[i.rs] / iop.reg[i.rt]);
                    iop.HI = (s32)(iop.reg[i.rs] & iop.reg[i.rt]);
                    
                    /* @Incomplete: Result is undefined if rt is zero */
                    intlog("I-DIV [{:d}] [{:d}]\n", i.rs, rt);
                } break;

				case 0x1B: 
                {
                    //@error: handle divison by zero
                    iop.LO = (iop.reg[i.rs] / iop.reg[i.rt]);
                    iop.HI = (iop.reg[i.rs] & iop.reg[i.rt]);
                    
                    /* @Incomplete: Result is undefined if rt is zero */
                    intlog("I-DIVU [{:d}] [{:d}]\n", i.rs, rt);
                } break;

				case 0x20: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] + iop.reg[i.rt];
                    
                    /* @@Incomplete: No overflow detection for now */
                    intlog("I-ADD [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);   
                } break;

				case 0x21: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] + iop.reg[i.rt];
                    intlog("I-ADDU [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);
                } break;

				case 0x22: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] - iop.reg[i.rt];

                    /* @@Incomplete: No overflow detection for now */
                    intlog("I-SUB [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt); 
                } break;

				case 0x23: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] - iop.reg[i.rt];
                    
                    intlog("I-SUBU [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt); 
                } break;

				case 0x24: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] & iop.reg[i.rt];
                    
                    intlog("I-AND [{:d}] [{:d}] [{:d}] \n", rd, i.rs, rt);
                } break;

				case 0x25: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] | iop.reg[i.rt];
                    
                    intlog("I-OR [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);
                } break;

				case 0x26: 
                {
                    iop.reg[i.rd] = iop.reg[i.rs] ^ iop.reg[i.rt];
                    intlog("I-XOR\n");
                } break;

				case 0x27: 
                {
                    iop.reg[i.rd] = ~(iop.reg[i.rs] | iop.reg[i.rt]);
                    intlog("I-NOR [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);
                } break;

				case 0x2A: 
                {
                    auto result = iop.reg[i.rs] < iop.reg[i.rt] ? 1 : 0;
                    iop.reg[i.rd] = result;

                    intlog("I-SLT [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);
                } break;

				case 0x2B: 
                {
                    uint32_t result = (iop.reg[i.rs] < iop.reg[i.rt]) ? 1 : 0;
                    iop.reg[i.rd] = result;
                    
                    intlog("I-SLTU [{:d}] [{:d}] [{:d}]\n", rd, i.rs, rt);
                } break;
			}
		} break;
/*
*-----------------------------------------------------------------------------
* COP0 Instructions
*-----------------------------------------------------------------------------
*/
		case IOP_COP0:
		{
			int cop0 = instruction >> 21 & 0x1F;
			switch(cop0)
			{
				case 0x00: 
                {
                    iop.reg[i.rt] = iop.cop0.r[i.rd];
                    intlog("I-MFC0 GPR: [{:d}] COP0: [{:d}]\n", i.rt, i.rd);
                } break;

				case 0x04: 
                {
                    // @Implementation: Automatically assume we are loading COP0 for now
                    auto cop0   = i.rd;
                    auto gpr    = iop.reg[i.rt];

                    if (cop0 == 12) {
                        iop.cop0.status.current_interrupt       = (gpr >> 0) & 0x1;
                        iop.cop0.status.current_kernel_mode     = (gpr >> 1) & 0x1;
                        iop.cop0.status.previous_interrupt      = (gpr >> 2) & 0x1;
                        iop.cop0.status.previous_kernel_mode    = (gpr >> 3) & 0x1;
                        iop.cop0.status.old_interrupt           = (gpr >> 4) & 0x1;
                        iop.cop0.status.old_kernel              = (gpr >> 5) & 0x1;
                        iop.cop0.status.interrupt_mask          = (gpr >> 8) & 0xFF;
                        iop.cop0.status.isolate_cache           = (gpr >> 16) & 0x1;
                        iop.cop0.status.swapped_cache           = (gpr >> 17) & 0x1;
                        iop.cop0.status.cache_parity_bit        = (gpr >> 18) & 0x1;
                        iop.cop0.status.last_load_operation     = (gpr >> 19) & 0x1;
                        iop.cop0.status.cache_parity_error      = (gpr >> 20) & 0x1;
                        iop.cop0.status.TLB_shutdown            = (gpr >> 21) & 0x1;
                        iop.cop0.status.boot_exception_vector   = (gpr >> 22) & 0x1;
                        iop.cop0.status.reverse_endianess       = (gpr >> 25) & 0x1;
                        iop.cop0.status.cop0_enable             = (gpr >> 28) & 0x1;
                    } else if (cop0 == 13) {
                        iop.cop0.cause.excode                   = (gpr >> 0)  & 0x1F;
                        iop.cop0.cause.interrupt_pending_field  = (gpr >> 1)  & 0xFF;
                        iop.cop0.cause.coprocessor_error        = (gpr >> 28) & 0x3;
                        iop.cop0.cause.branch_delay_pointer     = (gpr << 31);
                    } else {
                        iop.cop0.r[i.rd] = iop.reg[i.rt];
                    }

                    intlog("I-MTC0 GPR: [{:d}] COP0: [{:d}]\n", i.rt, i.rd);
                } break;

				case 0x10: 
                {
                    iop.cop0.status.current_interrupt    = iop.cop0.status.previous_interrupt;
                    iop.cop0.status.current_kernel_mode  = iop.cop0.status.previous_kernel_mode;
                    iop.cop0.status.previous_interrupt   = iop.cop0.status.old_interrupt;
                    iop.cop0.status.previous_kernel_mode = iop.cop0.status.old_kernel;
                    intlog("I-RFE \n");
                } break;
			}
		} break;
/*
*-----------------------------------------------------------------------------
* Normal Instructions
*-----------------------------------------------------------------------------
*/
		case 0x02: 
        {
            u32 offset = ((iop.pc + 4) & 0xF0000000) + (i.instr_index << 2);
            jump_to(offset);
            
            intlog("I-J [{:#x}]\n", offset);
        } break;

		case 0x03: 
        {
            u32 target_address  = ((iop.pc + 4) & 0xF0000000) + (i.instr_index << 2);
            iop.reg[31]         = iop.pc + 8;
            jump_to(target_address); 

            intlog("I-JAL [{:#x}] \n", target_address);  
        } break;

		case 0x04: 
        {
            // s16 imm = i.sign_imm << 2;
            s32 imm         = i.sign_imm << 2;
            bool condition  = iop.reg[i.rs] == iop.reg[i.rt];
            branch(condition, imm);

            intlog("I-BEQ [{:d}] [{:d}] [{:#x}] \n", i.rs, i.rt, imm);
        } break;

		case 0x05: 
        {
            s32 imm         = i.sign_imm << 2;
            bool condition  = iop.reg[i.rs] != iop.reg[i.rt];
            branch(condition, imm);

            intlog("I-BNE [{:d}] [{:d}], [{:#x}]\n", i.rt, i.rs, imm);
        } break;

		case 0x06: 
        {
            s16 offset      = i.sign_offset << 2;
            bool condition  = iop.reg[i.rs] <= 0;
            branch(condition, offset);

            intlog("I-BLEZ [{:d}] [{:#x}]\n", i.rs, offset); 
        } break;

		case 0x07: 
        {
            s32 offset      = i.sign_offset << 2;
            bool condition  = iop.reg[i.rs] > 0;
            branch(condition, offset);

            intlog("I-BGTZ [{:d}] [{:#x}] \n", i.rs, offset);
        } break;
		
		case 0x08: 
        {
            iop.reg[i.rt] = i.imm + iop.reg[i.rs];
            /* @@Incomplete: No overflow detection for now */
            intlog("I-ADDI [{:d}] [{:d}] [{:#x}]\n", i.rt, i.rs, i.imm); 
        } break;

		case 0x09: 
        {
            auto imm        = (u32)i.sign_imm;
            iop.reg[i.rt]   = imm + iop.reg[i.rs];
         //   intlog("I-ADDIU: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
        } break;
        
        case 0x0A: 
        {
            auto result     = (iop.reg[i.rs] < (s32)i.sign_imm) ? 1 : 0;
            iop.reg[i.rt]   = result;
            
            intlog("I-SLTI [{:d}] [{:d}], [{:#x}]\n", i.rt, i.rs, i.sign_imm);
        } break;

        case 0x0B: 
        {
            u32 result      = (iop.reg[i.rs] < i.sign_imm) ? 1 : 0;
            iop.reg[i.rt]   = result;

            intlog("I-SLTIU [{:d}] [{:d}] [{:#x}]\n", i.rt, i.rs, i.sign_imm);
        } break;

		case 0x0C: 
        {
            iop.reg[i.rt] = i.imm & iop.reg[i.rs];
            intlog("I-ANDI: [{:d}] [{:d}] [{:#x}] \n", i.rt, i.rs, i.imm);
        } break;

		case 0x0D: 
        {
            iop.reg[i.rt] = i.imm | iop.reg[i.rs];
            intlog("I-ORI: [{:d}] [{:d}] [{:#x}] \n", i.rt, i.rs, i.imm);
        } break;

		case 0x0E: 
        {
            iop.reg[i.rt] = i.imm ^ iop.reg[i.rs];
            intlog("I-XORI [{:d}] [{:d}] [{:#x}] \n", i.rt, i.rs, i.imm);
        } break;

		case 0x0F: 
        {
            //s32 imm     = (s32)(s16)((instruction & 0xFFFF) << 16);
            u32 imm         = i.imm << 16;
            iop.reg[i.rt]   = imm;

         //   intlog("I-LUI [{:d}] [{:#x}]\n", rt, imm); 
        } break;

		case 0x20: 
        {
            u32 vaddr       = iop.reg[i.rs] + i.sign_offset;
            iop.reg[i.rt]   = (s64)iop_core_load_8(vaddr);
            intlog("I-LB [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs);  
        } break;

		case 0x21: 
        {
            u32 vaddr   = iop.reg[i.rs] + i.sign_offset;
            u8 low_bit  = vaddr & 0b1; /* @Implementation: Should this be a boolean? */

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
            iop.reg[i.rt] = (s64)iop_core_load_16(vaddr);
            intlog("I-LH [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs); 
        } break;

		case 0x22: 
        {
            const u32 LWL_MASK[8] =
            {   
                0x00ffffffULL, 0x0000ffffULL, 0x000000ffULL, 0x00000000ULL,
            };

            const u8 LWL_SHIFT[4] = {24, 16, 8, 0};

            u32 vaddr           = iop.reg[i.rs] + i.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x3;
            u32 shift           = vaddr & 0x3;
            
            u32 aligned_dword   = iop_core_load_32(aligned_vaddr);
            u32 result          = (iop.reg[i.rt] & LWL_MASK[shift] | aligned_vaddr << LWL_SHIFT[shift]);
            iop.reg[i.rt]       = result;

            intlog("I-LWL [{:d}] [{:#x}] [{:d}]\n", i.rt, i.sign_offset, i.rs);
        } break;

		case 0x23: 
        {
            u32 vaddr   = iop.reg[i.rs] + i.sign_offset;
            u8 low_bits = vaddr & 0x3;
            
            if (low_bits != 0) {
                assert(1);
            }
            iop.reg[i.rt] = (s64)iop_core_load_32(translate_address(vaddr));
            intlog("I-LW [{:d}] [{:#x}] [{:d}] \n", rt, i.sign_offset, i.rs;);
        } break;

		case 0x24: 
        {
            u32 vaddr       = iop.reg[i.rs] + i.sign_offset;
            iop.reg[i.rt]   = (u64)iop_core_load_8(vaddr);
            intlog("I-LBU [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs;); 
        } break;

		case 0x26: 
        {
            const u32 LWR_MASK[8] =
            {   
                0x00000000ULL, 0xff000000ULL, 0xffff0000ULL, 0xffffff00ULL, 
            };

            const u8 LWR_SHIFT[4] = {0, 8, 16, 24};

            u32 vaddr           = iop.reg[i.rs] + i.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x3;
            u32 shift           = vaddr & 0x3;
            
            u32 aligned_dword   = iop_core_load_32(aligned_vaddr);
            u32 result          = (iop.reg[i.rt] & LWR_MASK[shift] | aligned_vaddr << LWR_SHIFT[shift]);
            iop.reg[i.rt]       = result;

            intlog("I-LDR [{:d}] [{:#x}] [{:d}]\n", i.rt, i.sign_offset, i.rs;);
        } break;

		case 0x28: 
        {
            u32 vaddr   = iop.reg[i.rs] + i.sign_offset;
            s8 value    = (s8)iop.reg[i.rt];
            iop_core_store_8(vaddr, value);

         //   intlog("I-SB [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs;);
        } break;
        
        case 0x29: 
        {
            u32 vaddr       = iop.reg[i.rs] + i.sign_offset;
            s16 value       = iop.reg[i.rt];
            u8 low_bits     = vaddr & 0x1;

            if (low_bits != 0) {
              assert(1);
            }

            iop_core_store_16(vaddr, value);
            intlog("I-SH [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs;);
        } break;

		case 0x2A: 
        {
            const u32 SWL_MASK[8] =
            {   
                0x00ffffffULL, 0x0000ffffULL, 0x000000ffULL, 0x00000000ULL,
            };

            const u8 SWL_SHIFT[4] = {24, 16, 8, 0};

            u32 vaddr           = iop.reg[i.rs] + i.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x3;
            u32 shift           = vaddr & 0x3;

            u32 aligned_dword   = iop_core_load_32(aligned_vaddr);
            u32 result          = (iop.reg[i.rt] & SWL_MASK[shift] | aligned_vaddr << SWL_SHIFT[shift]);
            //iop_core_store_32(aligned_vaddr, aligned_dword);
            iop_core_store_32(aligned_vaddr, result);
            intlog("I-SWL [{:d}] [{:#x}] [{:d}]\n", i.rt, i.sign_offset, i.rs;);
        } break;

		case 0x2B: 
        {
            u32 vaddr   = iop.reg[i.rs] + i.sign_offset;
            s32 value   = iop.reg[i.rt];
            u8 low_bits = vaddr & 0x3;
            
            if (low_bits != 0) {
                assert(1);
            }

            iop_core_store_32(vaddr, value);
            intlog("I-SW [{:d}] [{:#x}] [{:d}] \n", i.rt, i.sign_offset, i.rs;);
        } break;

		case 0x2E: 
        {
            const u32 SWR_MASK[8] =
            {   
                0x00000000ULL, 0xff000000ULL, 0xffff0000ULL, 0xffffff00ULL, 
            };

            const u8 SWR_SHIFT[4] = {0, 8, 16, 24};

            u32 vaddr           = iop.reg[i.rs] + i.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x3;
            u32 shift           = vaddr & 0x3;

            u32 aligned_dword   = iop_core_load_32(aligned_vaddr);
            u32 result          = (iop.reg[i.rt] & SWR_MASK[shift] | aligned_vaddr << SWR_SHIFT[shift]);
            //iop_core_store_32(aligned_vaddr, aligned_dword);
            iop_core_store_32(aligned_vaddr, result);
            intlog("I-SWL [{:d}] [{:#x}] [{:d}]\n", i.rt, i.sign_offset, i.rs;);
        } break;
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
    // iop_cop0_write_status(&iop.cop0.status, iop.reg[12]);
    // iop_cop0_write_cause(&iop.cop0.cause, iop.reg[13]);
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