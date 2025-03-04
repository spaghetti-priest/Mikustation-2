//@@Temporary: @@Move: Might move this into its own Bus file
#include "iop/iop.h"
void 
output_to_console (u32 value)
{
    //console << (char)value;
    //console.flush();
    if ((char)value == '#') printf("\n");
    printf("%c", (char)value);
}
    
void 
iop_output_to_console (u32 iop_pc)
{
    std::string value;
    u32 pointer     = get_iop_register(5);
    u32 text_size   = get_iop_register(6);

    while (text_size) {
          auto c = (char)_iop_ram_[pointer & 0x1FFFFF];
          //putc(c);
          value += c;
          pointer++;
          text_size--;
    }
    printf("{%s}", value.c_str());
}

u8 
iop_load_8 (u32 address)
{
    u8 r = 0;

    if (PSX_RAM.contains(address))  
        return *(u8*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];

    errlog("[ERROR]: Could not read iop_load_memory8() at address [{:#09x}]\n", address);

    return r;
}

u16 
iop_load_16 (u32 address)
{
    u16 r = 0;

    if (PSX_RAM.contains(address))  
        return *(u16*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
 
    errlog("[ERROR]: Could not read iop_load_memory16() at address [{:#09x}]\n", address);
    
    return r;
}

u32 
iop_load_32 (u32 address)
{
    u32 r = 0;

    switch(address)
    {
        case 0x1f801010: return 0;      break; // BIOS_RAM
    }

    if(address == 0x1D000000) {
        printf("READ: EE->IOP communication\n");
        return 0;
    }

    if ((address >= 0x1F801080 && address <= 0x1F8010EF) || (address >= 0x1F801500 && address <= 0x1F80155F)) {
        return iop_dmac_read_32(address);
    }

    if (PSX_RAM.contains(address))  
        return *(u32*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u32*)&_bios_memory_[address & 0x3FFFFF];

    errlog("[ERROR]: Could not read iop_load_memory32() at address [{:#09x}]\n", address);

    return r;
}

void 
iop_store_8 (u32 address, u8 value)
{
    if (PSX_RAM.contains(address))  {
      *(u8*)&_iop_ram_[address] = value;
      return;  
    } 

    // @@Note: This is a write to POST2 which is unknown as to what it does?
    if (address == 0x1F802070) return;

    errlog("[ERROR]: Could not write iop_store_memory8() value: [{:#09x}] to address: [{:#09x}] \n", value, address);

}

void 
iop_store_16 (u32 address, u16 value)
{
    if (PSX_RAM.contains(address)) {
      *(u16*)&_iop_ram_[address] = value;
      return;  
    } 

    errlog("[ERROR]: Could not write iop_store_memory16() value: [{:#09x}] to address: [{:#09x}] \n", value, address); 
}

void 
iop_store_32 (u32 address, u32 value)
{
    switch (address) {
        case 0x1f801010: return;    break; // BIOS ROM
        case 0x1f801020: return;    break; // COMMON DELAY
        case 0x1f801004: return;    break; // Expansion 2 Base Address
        case 0x1f80101c: return;    break; // Expansion 2 
        case 0x1f802070: return;    break; // POST2
        case 0x1f801000: return;    break; // Expansion 1 Base Adresss
        case 0x1f801008: return;    break; // Expansion 1 
        case 0x1f801014: return;    break; // SPU_DELAY  
        case 0x1f801018: return;    break; // CDROM_DELAY  
        case 0x1f80100c: return;    break; // Expansion 3  
        case 0x1f8010f0: return;    break; // DMA_Control Register  
        case 0x1f808268: return;    break; // SIO2 control
        case 0x1f801060: return;    break; // RAM SIZE
    }

    u32 iop_pc = get_iop_pc();
    if (iop_pc == 0x12C48 || iop_pc == 0x1420C || iop_pc == 0x1430C) {
        iop_output_to_console(iop_pc);
        return;
    }

    if ((address >= 0x1F801080 && address <= 0x1F8010EF) || (address >= 0x1F801500 && address <= 0x1F80155F)) {
        iop_dmac_write_32(address, value);
        return;
    }

    if (address == 0x1D000010) {
        printf("WRITE: IOP->EE communication\n");
        return;
    } 

    if (PSX_RAM.contains(address))  {
        *(u32*)&_iop_ram_[address] = value;
        return;  
    }

    if (address >= 0x1F808240 && address <= 0x1F80825F) {
        // SIO_SEND1/ SEND2 or Port 1/2 Control
        //printf("SIO registers address [%#x]\n", address);
        return;
    }

    errlog("[ERROR]: Could not write iop_store_memory32() value: [{:#09x}] to address: [{:#09x}] \n", value, address);

}

/*******************************************
 * Load Functions
*******************************************/
u8 
ee_load_8 (u32 address) 
{
    uint8_t r = 0;

    if (address < 0x10000000)
        return *(u8*)&_rdram_[address & 0x01FFFFFF];

    if (address >= 0x1FC00000 && address < 0x20000000) 
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];
    

    errlog("[ERROR]: Could not read load_memory8() at address [{:#09x}]\n", address);    

    return r;
}

u16 
ee_load_16 (u32 address) 
{
    uint16_t r = 0;

    if (address < 0x10000000)
        return *(u16*)&_rdram_[address & 0x01FFFFFF];

    if (address >= 0x1FC00000 && address < 0x20000000) 
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];

    if (address >= 0x1C000000 && address < 0x1C200000)
        return *(u16*)&_iop_ram_[address & 0x1FFFFF];

    if (address == 0x1a000006) 
        return 1;

    errlog("[ERROR]: Could not read load_memory16() at address [{:#09x}]\n", address);
    
    return r;
}

u32 
ee_load_32 (u32 address) 
{
    uint32_t r = 0;

    if (address < 0x10000000) 
        return *(uint32_t*)&_rdram_[address & 0x01FFFFFF];
    
    if (address >= 0x1FC00000 && address < 0x20000000) 
        return *(uint32_t*)&_bios_memory_[address & 0x3FFFFF];

    if (address >= 0x1C000000 && address < 0x1C200000)
       return *(u32*)&_iop_ram_[address & 0x1FFFFF];

    switch(address) 
    {
        case 0x10002010:
        {
            return ipu_read_32(address);
        } break;

        case 0x1000f130:
        {
            return 0;
        } break;

        case 0x1000f000:
        {
            return intc_read(address);
        } break;

        case 0x1000f010:
        {
            return intc_read(address);
        } break;

        case 0x1000f430:
        {
            //printf("Read from MCH_RICM\n");
            return 0;
        } break;
        
        case 0x1000f440:
        {
            uint8_t SOP = (MCH_RICM >> 6) & 0xF;
            uint8_t SA  = (MCH_RICM >> 16) & 0xFFF;
            if (!SOP)
            {
                switch (SA)
                {
                case 0x21:
                    if (_rdram_sdevid < 2)
                    {
                        _rdram_sdevid++;
                        return 0x1F;
                    }
                    return 0;
                case 0x23:
                    return 0x0D0D;
                case 0x24:
                    return 0x0090;
                case 0x40:
                    return MCH_RICM & 0x1F;
                }
            }
        } break;
    }

    if (address >= 0x10008000 && address < 0x1000f590)
        return dmac_read(address);

    if ((address >= 0x10003000) && (address <= 0x100030A0))
        return gif_read(address);

    if (address >= 0x1000F200 && address <= 0x1000F260)
        return sif_read(address);

    if (address >= 0x12000000 && address < 0x12002000)
        return gs_read_32_priviledged(address);

    if (address >= 0x10000000 && address <= 0x10001840)
        return timer_read(address);
    
    errlog("[ERROR]: Could not read load_memory32() at address [{:#09x}]\n", address);
    
    return r;
}

u64 
ee_load_64 (u32 address) 
{
    uint64_t r = 0;
    if (address < 0x10000000)
        return *(uint64_t*)&_rdram_[address & 0x01FFFFFF];

    if (address >= 0x12000000 && address < 0x12002000)
        return gs_read_64_priviledged(address);
    
    if (address == 0x10006000 || address == 0x10006008) {
        gif_fifo_read(address);
        return 0;
    }
    
    // errlog("[ERROR]: Could not read load_memory64() at address [{:#09x}]\n", address);
    return r;
}

u128 ee_load_128() {
    u128 r = { 0 };
    return r;
}

/*******************************************
 * Store Functions
*******************************************/
void 
ee_store_8 (u32 address, u8 value) 
{
    if (address < 0x10000000){
        *(u8*)&_rdram_[address & 0x01FFFFFF] = value;
        return;
    }

    if (address == 0x1000f180) {
        output_to_console(value);
        return;
    };

    if (address >= 0x1C000000 && address < 0x1C200000) {
       *(u8*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory8() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_16 (u32 address, u16 value) 
{
    if (address < 0x10000000) {
        *(u16*)&_rdram_[address & 0x01FFFFFF] = value;
        return;
    }

    if (address == 0x1000f180) {
        output_to_console(value);
        return;
    };
    
    if (address >= 0x1C000000 && address < 0x1C200000) {
       *(u16*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory16() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_32 (u32 address, u32 value) 
{

    if (address < 0x10000000){
        *(u32*)&_rdram_[address & 0x01FFFFFF] = value;
        return;
    }

    switch (address)
    {
        case 0x10002000:
        {
            ipu_write_32(address, value);
            return;
        } break;

        case 0x10002010:
        {
            ipu_write_32(address, value);
            return;
        } break;

        case 0x1000f180:
        {
            output_to_console(value);
            return;
        } break;

        case 0x1000f000:
        {
            intc_write(address, value);
            return;
        } break;

        case 0x1000f010:
        {
            intc_write(address, value);
            return;
        } break;

        case 0x1000f430:
        {
            uint8_t SA = (value >> 16) & 0xFFF;
            uint8_t SBC = (value >> 6) & 0xF;

            if (SA == 0x21 && SBC == 0x1 && ((MCH_DRD >> 7) & 1) == 0)
                _rdram_sdevid = 0;

            MCH_RICM = value & ~0x80000000;
            return;
        } break;

        case 0x1000f440:
        {
           //printf("Writing to MCH_DRD [%#08x]\n", value);
            MCH_DRD = value;
            return;
        } break;

        return;
    }
    
    if (address >= 0x10008000 && address < 0x1000f000) {
       dmac_write(address, value);
       return;
    }
    
    if ((address >= 0x10003000) && (address <= 0x100030A0)) {
        gif_write(address, value);
        return;
    }
    
    //@@Note: Not sure what is this is
    if (address == 0x1000f500) return;

    if (address >= 0x1000F200 && address <= 0x1000F260) {
        sif_write(address, value);
        return;
    }

    if (address >= 0x12000000 && address < 0x12002000) {
        gs_write_32_priviledged(address, value);
        return;
    }

    if (address >= 0x10000000 && address <= 0x10001840) {
        timer_write(address, value);
        return;
    }

    if (address >= 0x1C000000 && address < 0x1C200000) {
       *(u32*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory32() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_64 (u32 address, u64 value) 
{
    if (address < 0x10000000) {
        *(u64*)&_rdram_[address & 0x01FFFFFF] = value;
        return;
    }

    if (address >= 0x12000000 && address < 0x12002000) {
        gs_write_64_priviledged(address, value);
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10006000 || address == 0x10006008) {
        gif_fifo_write(address);
        return;
    }

    if (address ==  0x10007010 || address == 0x10007018) {
        ipu_fifo_write();
        return;
    }

    if (VU1_CODE_MEMORY.contains(address)) {
        //@HACK         
        *(u64*)&_vu1_code_memory_[address & 0x3FFF] = value;
        return;
    }

    if (VU1_DATA_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu1_data_memory_[address & 0x3FFF] = value;
        return;   
    }

    if (VU0_CODE_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu0_code_memory_[address & 0x3FFF] = value;
        return;
    }
    
    if (VU0_DATA_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu0_data_memory_[address & 0x3FFF] = value;
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10004000 || address == 0x10004008) {
        printf("VIF0 FIFO Write \n");
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10005000 || address == 0x10005008){
        printf("VIF1 FIFO Write \n");
        return;
    }

    errlog("[ERROR]: Could not write store_memory64() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}
void ee_store_128 (u32 address, u128 value) {}
