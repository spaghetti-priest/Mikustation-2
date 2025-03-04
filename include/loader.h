#pragma once

#ifndef LOADER_H
#define LOADER_H

#include "ps2types.h"
#include "ee/r5900Interpreter.h"

// From: https://wiki.osdev.org/ELF_Tutorial
#define ELF_NIDENT	16
typedef struct _Elf32_Ehdr_ {
	u8	e_ident[ELF_NIDENT];
	u16	e_type;
	u16	e_machine;
	u32	e_version;
	u32	e_entry;
	u32	e_phoff;
	u32	e_shoff;
	u32	e_flags;
	u16	e_ehsize;
	u16	e_phentsize;
	u16	e_phnum;
	u16	e_shentsize;
	u16	e_shnum;
	u16	e_shstrndx;
} Elf32_Ehdr;

typedef struct _Elf32_Phdr_ {
	u32	p_type;
	u32	p_offset;
	u32	p_vaddr;
	u32	p_paddr;
	u32	p_filesz;
	u32	p_memsz;
	u32	p_flags;
	u32	p_align;
} Elf32_Phdr;

typedef struct _Elf32_Shdr_ {
	u32	sh_name;
	u32	sh_type;
	u32	sh_flags;
	u32	sh_addr;
	u32	sh_offset;
	u32	sh_size;
	u32	sh_link;
	u32	sh_info;
	u32	sh_addralign;
	u32	sh_entsize;
} Elf32_Shdr;

bool 	load_elf(R5900_Core *ee, const char *path);
bool 	read_bios(const char *filename, u8 *bios_memory);
#endif