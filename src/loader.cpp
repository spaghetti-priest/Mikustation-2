/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/loader.h"
#include "../include/common.h"
#include "../include/ps2.h"
#include <iostream>
#include <assert.h>

//@Cleanup: This resides here since there is no dedicated file IO
inline const char *strip_file_path (const char *s) 
{
#if _WIN32 || _WIN64
    const char* last = strrchr(s, '\\');
#else
    const char* last = strrchr(s, '/');
#endif
   last += 1;
   return last;
}

bool 
load_elf(R5900_Core *ee, const char *path)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	u8 *elf;
	u64 addr = 0;

	assert(f && "File name or Path for ELF file cannot be found");

	fseek(f, 0, SEEK_END);
	size_t file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	elf = (u8 *)malloc(file_size * sizeof(u8));

	if (fread(&buf, sizeof(unsigned char), file_size, f) == 1) {
        errlog("[ERROR]: Could not read ELF file \n");
        fclose(f);
        return false;
    }

    while (fread(&buf, sizeof(unsigned char), sizeof(unsigned char), f) == 1) 
        elf[addr++] = reinterpret_cast<uint8_t>(buf);
    
    fclose(f);

	if (elf[0] == 0x7F && elf[1] == 'E' && elf[2] == 'L' && elf[3] == 'F') {
		printf("Valid ELF found...\n");
	} else {
		errlog("[ERROR]: ELF file is not valid\n");
		return false;
	}

	const char *name = strip_file_path(path);
	Elf32_Ehdr *hdr = reinterpret_cast<Elf32_Ehdr *>(&elf[0]);
	printf("\n=============================================\n");

	printf("Current ELF File Properties\n");
	printf("Current ELF Filename: [%s]\n", 			name);
	printf("Header type: [%#08x]\n", 				hdr->e_type);
	printf("Instruction set: [%#08x]\n", 			hdr->e_machine);
	printf("ELF Version: [%#08x]\n", 				hdr->e_version);
	printf("Program entry offset: [%#08x]\n", 		hdr->e_entry);	
	printf("Header table offset: [%#08x]\n", 		hdr->e_phoff);
	printf("Section table offset: [%#08x]\n", 		hdr->e_shoff);
	printf("Flags: [%#08x]\n", 						hdr->e_flags);
	printf("Header size: [%#08x]\n", 				hdr->e_ehsize);	
	printf("Program header entry size: [%#08x]\n", 	hdr->e_phentsize);
	printf("Program header entry count: [%#08x]\n", hdr->e_phnum);
	printf("Section header entry size: [%#08x]\n", 	hdr->e_shentsize);
	printf("Section header entry count: [%#08x]\n", hdr->e_shnum);
	printf("Section header string ndex: [%#08x]\n", hdr->e_shstrndx);	

	printf("\n\n");
	for (int i = hdr->e_phoff; i < hdr->e_phoff + (hdr->e_phnum * hdr->e_phentsize); i += hdr->e_phentsize) {
		Elf32_Phdr *phdr = reinterpret_cast<Elf32_Phdr *>(&elf[i]);
		printf("Segment offset: [%#08x]\n", 				i);	
		printf("Program segment type: [%#08x]\n", 			phdr->p_type);
		printf("Segment data offset type: [%#08x]\n", 		phdr->p_offset);
		printf("Virtual memory start Address: [%#08x]\n", 	phdr->p_vaddr);
		printf("Segment physical address: [%#08x]\n", 		phdr->p_paddr);
		printf("Segement size in file: [%#08x]\n", 			phdr->p_filesz);
		printf("Segment size in memory: [%#08x]\n", 		phdr->p_memsz);
		printf("Flags: [%#08x]\n", 							phdr->p_flags);
		printf("Segment alignment: [%#08x]\n", 				phdr->p_align);

		int mem_write_cursor 	= phdr->p_vaddr;
		int file_write_cursor 	= phdr->p_offset;

		// This could be just one memset
		while(file_write_cursor < (phdr->p_offset + phdr->p_filesz))
		{
			u32 value = *(u32*)&elf[file_write_cursor];
			ee_store_32(mem_write_cursor, value);

			file_write_cursor 	+= 4;
			mem_write_cursor 	+= 4;
		} 
	}
	
	printf("\n=============================================\n");
	ee->pc = hdr->e_entry;

	free(hdr);
	return true;
}


struct romdir_entry {
	char name[10]; 		//File name, must be null terminated
	u16 extinfo_size; 	//Size of the file's extended info in EXTINFO
	u32 file_size; 	 	//Size of the file itself
};

int 
read_bios (const char *filename, u8 *bios_memory) 
{
    size_t file_size = 0;
    unsigned char *c;

#if _WIN32 || _WIN64
    FILE* f = fopen(filename, "rb");
    if (!f) {
        errlog("[ERROR]: Could not open BIOS file. \n");
        fclose(f);
        return 0;
    }
    printf("Found BIOS file...\n");

    fseek(f, 0, SEEK_END); // @Incomplete: Add 64 bit version compatibility
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(fread(&c, sizeof(unsigned char), file_size, f) == 1) {
        errlog("[ERROR]: Could not read BIOS file \n");
        fclose(f);
        return 0;
    }

    printf("Copying BIOS to main memory\n");
    // Copy BIOS to memory
    uint64_t addr = 0;
    while (fread(&c, sizeof(unsigned char), sizeof(unsigned char), f) == 1) 
        bios_memory[addr++] = reinterpret_cast<uint8_t>(c);
        //_bios_memory_[addr++] = reinterpret_cast<uint8_t>(c);

    fclose(f);
#else 
    std::ifstream reader;
    reader.open(filename, std::ios::in | std::ios::binary);
    if (!reader.is_open())
        errlog("[ERROR]: Could not read BIOS file!\n");

    reader.seekg(0);
    reader.read((char*)_bios_memory_, 4 * 1024 * 1024);
    reader.close();
#endif

    printf("Sucessfully read BIOS file...\n");

    return 1;
}