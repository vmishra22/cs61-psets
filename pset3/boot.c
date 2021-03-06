// vim: set tabstop=8: -*- tab-width: 8 -*-
#include "x86.h"
#include "elf.h"

// boot.c
//
//   WeensyOS boot loader. Loads the kernel at address 0x40000 from
//   the first IDE hard disk.
//
//   A BOOT LOADER is a tiny program that loads an operating system into
//   memory. It has to be tiny because it can contain no more than 510 bytes
//   of instructions: it is stored in the disk's first 512-byte sector.
//
//   When the CPU boots it loads the BIOS into memory and executes it. The
//   BIOS intializes devices and CPU state, reads the first 512-byte sector of
//   the boot device (hard drive) into memory at address 0x7C00, and jumps to
//   that address.
//
//   The OS01 boot loader is contained in bootstart.S and boot.c. Control
//   starts in bootstart.S, which initializes the CPU and sets up a stack,
//   then transfers here. This code reads in the kernel image and calls the
//   kernel.
//
//   The main kernel is stored as an ELF executable image starting in the
//   disk's sector 1.

#define SECTORSIZE	512
#define ELFHDR		((struct elf_header *) 0x10000) // scratch space

void readsect(void *dst, uint32_t src_sect);
void readseg(void *dst, uint32_t src_sect, uint32_t filesz, uint32_t memsz);


// bootmain
//    Load the kernel and jump to it.
void bootmain(void) {
    // read 1st page off disk (should include programs as well as header)
    // and check validity
    readseg(ELFHDR, 1, PAGESIZE, PAGESIZE);
    if (ELFHDR->e_magic != ELF_MAGIC)
	return;

    // load each program segment
    elf_program *ph = (elf_program *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
    elf_program *eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph++)
	readseg(ph->p_va, ph->p_offset / SECTORSIZE + 1,
		ph->p_filesz, ph->p_memsz);

    // jump to the kernel, clearing %eax
    __asm __volatile("movl %0, %%esp; ret" : : "r" (&ELFHDR->e_entry), "a" (0));
}


// readseg(dst, src_sect, filesz, memsz)
//    Load an ELF segment at virtual address `dst` from the IDE disk's sector
//    `src_sect`. Copies `filesz` bytes into memory at `dst` from sectors
//    `src_sect` and up, then clears memory in the range
//    `[dst+filesz, dst+memsz)`.
void readseg(void *dst, uint32_t src_sect, uint32_t filesz, uint32_t memsz) {
    uintptr_t ptr = (uintptr_t) dst;
    uintptr_t end_ptr = ptr + filesz;
    memsz += ptr;

    // round down to sector boundary
    ptr &= ~(SECTORSIZE - 1);

    // read sectors
    for (; ptr < end_ptr; ptr += SECTORSIZE, ++src_sect)
	readsect((uint8_t *) ptr, src_sect);

    // clear bss segment
    for (; end_ptr < memsz; ++end_ptr)
	*(uint8_t *) end_ptr = 0;
}


// waitdisk
//    Wait for the disk to be ready.
void waitdisk(void) {
    while ((inb(0x1F7) & 0xC0) != 0x40)
	/* do nothing */;
}


// readsect(dst, src_sect)
//    Read disk sector number `src_sect` into address `dst`.
void readsect(void *dst, uint32_t src_sect) {
    // programmed I/O for "read sector": first say what sector to read
    waitdisk();
    outb(0x1F2, 1);		// count = 1
    outb(0x1F3, src_sect);
    outb(0x1F4, src_sect >> 8);
    outb(0x1F5, src_sect >> 16);
    outb(0x1F6, (src_sect >> 24) | 0xE0);
    outb(0x1F7, 0x20);		// cmd 0x20 - read sectors

    // then move the data into memory
    waitdisk();
    insl(0x1F0, dst, SECTORSIZE/4);
}
