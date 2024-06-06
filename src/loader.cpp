/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/loader.h"
#include <iostream>
#include <assert.h>

bool 
load_elf(R5900_Core *ee, const char *path)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	u8 *elf;
	u64 addr = 0;

	assert(f);

	fseek(f, 0, SEEK_END);
	size_t file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	elf = (u8 *)malloc(file_size * sizeof(u8));

	if (fread(&buf, sizeof(unsigned char), file_size, f) == 1) {
        printf("[ERROR]: Could not read ELF file \n");
        fclose(f);
        return false;
    }

    while (fread(&buf, sizeof(unsigned char), sizeof(unsigned char), f) == 1) 
        elf[addr++] = reinterpret_cast<uint8_t>(buf);
    
    fclose(f);

	if (elf[0] == 0x7F && elf[1] == 'E' && elf[2] == 'L' && elf[3] == 'F') {
		printf("Valid ELF found...\n");
	} else {
		printf("[ERROR]: ELF file is not valid\n");
		return false;
	}

	const char *name = strip_file_path(path);
	Elf32_Ehdr *hdr = reinterpret_cast<Elf32_Ehdr *>(&elf[0]);
	printf("\n=============================================\n");

	printf("Current ELF File Properties\n");
	printf("Current ELF Filename: [%s]\n", name);
	printf("Header type: [%#08x]\n", hdr->e_type);
	printf("Instruction set: [%#08x]\n", hdr->e_machine);
	printf("ELF Version: [%#08x]\n", hdr->e_version);
	printf("Program entry offset: [%#08x]\n", hdr->e_entry);	
	printf("Header table offset: [%#08x]\n", hdr->e_phoff);
	printf("Section table offset: [%#08x]\n", hdr->e_shoff);
	printf("Flags: [%#08x]\n", hdr->e_flags);
	printf("Header size: [%#08x]\n", hdr->e_ehsize);	
	printf("Program header entry size: [%#08x]\n", hdr->e_phentsize);
	printf("Program header entry count: [%#08x]\n", hdr->e_phnum);
	printf("Section header entry size: [%#08x]\n", hdr->e_shentsize);
	printf("Section header entry count: [%#08x]\n", hdr->e_shnum);
	printf("Section header string ndex: [%#08x]\n", hdr->e_shstrndx);	

	printf("\n\n");
	for (int i = hdr->e_phoff; i < hdr->e_phoff + (hdr->e_phnum * hdr->e_phentsize); i += hdr->e_phentsize) {
		Elf32_Phdr *phdr = reinterpret_cast<Elf32_Phdr *>(&elf[i]);
		printf("Segment offset: [%#08x]\n", i);	
		printf("Program segment type: [%#08x]\n", phdr->p_type);
		printf("Segment data offset type: [%#08x]\n", phdr->p_offset);
		printf("Virtual memory start Address: [%#08x]\n", phdr->p_vaddr);
		printf("Segment physical address: [%#08x]\n", phdr->p_paddr);
		printf("Segement size in file: [%#08x]\n", phdr->p_filesz);
		printf("Segment size in memory: [%#08x]\n", phdr->p_memsz);
		printf("Flags: [%#08x]\n", phdr->p_flags);
		printf("Segment alignment: [%#08x]\n", phdr->p_align);
	}
	
	printf("\n=============================================\n");
	//*Entry point goes here

	ee->pc = hdr->e_entry;

	free(hdr);
	return true;
}