#ifndef BUS_H

typedef struct Range 
{
    uint32_t start;
    uint32_t size;
    
    inline Range(uint32_t _start, uint32_t _size) : start(_start), size(_size) {}
    // inline bool contains (uint32_t addr);
} Range;

/*inline bool Range::contains (uint32_t addr) 
{
    uint32_t range  = start + size;
    bool contains   = (addr >= start) && (addr < range);

    return contains;
}
*/
Range BIOS              = Range(0x1FC00000, MEGABYTES(4));
Range RDRAM             = Range(0x00000000, MEGABYTES(32));
Range IO_REGISTERS      = Range(0x10000000, KILOBYTES(64));
Range VU0_CODE_MEMORY   = Range(0x11000000, KILOBYTES(4));
Range VU0_DATA_MEMORY   = Range(0x11004000, KILOBYTES(4));
Range VU1_CODE_MEMORY   = Range(0x11008000, KILOBYTES(16));
Range VU1_DATA_MEMORY   = Range(0x1100C000, KILOBYTES(16));
Range GS_REGISTERS      = Range(0x12000000, KILOBYTES(8));
Range IOP_RAM           = Range(0x1C000000, MEGABYTES(2));
//Range SCRATCHPAD        = Range(0x70000000, KILOBYTES(16));

// IOP Physical Memory Map.          From:Ps2tek
Range PSX_RAM           = Range(0x00000000, MEGABYTES(2));
Range PSX_IO_REGISTERS  = Range(0x1F800000, KILOBYTES(64));
Range SPU2_REGISTERS    = Range(0x1F900000, KILOBYTES(1));

uint8_t     ee_load_8  (uint32_t address); 
uint16_t    ee_load_16 (uint32_t address); 
uint32_t    ee_load_32 (uint32_t address); 
uint64_t    ee_load_64 (uint32_t address);
//void      ee_load_128() {return;}

void        ee_store_8  (uint32_t address, uint8_t value);
void        ee_store_16 (uint32_t address, uint16_t value);
void        ee_store_32 (uint32_t address, uint32_t value);
void        ee_store_64 (uint32_t address, uint64_t value);

uint8_t     iop_load_8 (uint32_t address);
uint16_t    iop_load_16 (uint32_t address);
uint32_t    iop_load_32 (uint32_t address);

void        iop_store_8 (uint32_t address, uint8_t value);
void        iop_store_16 (uint32_t address, uint16_t value);
void        iop_store_32 (uint32_t address, uint32_t value);
#define BUS_H
#endif