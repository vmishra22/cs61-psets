// vim: set tabstop=8: -*- tab-width: 8 -*-
#include "x86.h"
#include "elf.h"
#include "lib.h"
#include "kernel.h"

// k-loader.c
//
//    Load a weensy application into memory from a RAM image.

#define SECTORSIZE		512

extern uint8_t _binary_obj_p_allocator_start[];
extern uint8_t _binary_obj_p_allocator_end[];
extern uint8_t _binary_obj_p_allocator2_start[];
extern uint8_t _binary_obj_p_allocator2_end[];
extern uint8_t _binary_obj_p_allocator3_start[];
extern uint8_t _binary_obj_p_allocator3_end[];
extern uint8_t _binary_obj_p_allocator4_start[];
extern uint8_t _binary_obj_p_allocator4_end[];
extern uint8_t _binary_obj_p_fork_start[];
extern uint8_t _binary_obj_p_fork_end[];
extern uint8_t _binary_obj_p_forkexit_start[];
extern uint8_t _binary_obj_p_forkexit_end[];

struct ramimage {
    void *begin;
    void *end;
} ramimages[] = {
    { _binary_obj_p_allocator_start, _binary_obj_p_allocator_end },
    { _binary_obj_p_allocator2_start, _binary_obj_p_allocator2_end },
    { _binary_obj_p_allocator3_start, _binary_obj_p_allocator3_end },
    { _binary_obj_p_allocator4_start, _binary_obj_p_allocator4_end },
    { _binary_obj_p_fork_start, _binary_obj_p_fork_end },
    { _binary_obj_p_forkexit_start, _binary_obj_p_forkexit_end }
};

static int copyseg(proc *p, const elf_program *ph, const uint8_t *src);

// program_load(p, program_id)
//    Load the code corresponding to program `programnumber` into the process
//    `p` and set `p->p_registers.reg_eip` to its entry point. Calls
//    `page_alloc` to allocate virtual memory for `p` as required. Returns 0
//    on success and -1 on failure (e.g. out-of-memory).
int program_load(proc *p, int program_id) {
    // is this a valid program?
    int nprograms = sizeof(ramimages) / sizeof(ramimages[0]);
    assert(program_id >= 0 && program_id < nprograms);
    elf_header *eh = (elf_header *) ramimages[program_id].begin;
    assert(eh->e_magic == ELF_MAGIC);

    // load each loadable program segment into memory
    elf_program *ph = (elf_program *) ((const uint8_t *) eh + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; ++i)
	if (ph[i].p_type == ELF_PTYPE_LOAD)
	    if (copyseg(p, &ph[i], (const uint8_t *) eh + ph[i].p_offset) < 0)
		return -1;

    // set the entry point from the ELF header
    p->p_registers.reg_eip = eh->e_entry;
    return 0;
}


// copyseg(p, ph, src)
//    Load an ELF segment at virtual address `ph->p_va` in process `p`. Copies
//    `[src, src + ph->p_filesz)` to `dst`, then clears
//    `[ph->p_va + ph->p_filesz, ph->p_va + ph->p_memsz)` to 0.
//    Calls `page_alloc` to allocate pages in `p->p_pagedir`. Returns 0
//    on success and -1 on failure.
static int copyseg(proc *p, const elf_program *ph, const uint8_t *src) {
    uintptr_t va = (uintptr_t) ph->p_va;
    uintptr_t end_file = va + ph->p_filesz, end_mem = va + ph->p_memsz;
    va &= ~(PAGESIZE - 1);		// round to page boundary

    // allocate memory
    for (uintptr_t page_va = va; page_va < end_mem; page_va += PAGESIZE)
	if (page_alloc(p->p_pagedir, page_va, p->p_pid) < 0)
	    return -1;
    // ensure new memory mappings are active
    lcr3(p->p_pagedir);

    memcpy((uint8_t *) va, src, end_file - va);
    memset((uint8_t *) end_file, 0, end_mem - end_file);
    
    if( (ph->p_flags & ELF_PFLAG_WRITE) == 0 )
    {
        for (uintptr_t page_va = va; page_va < end_mem; page_va += PAGESIZE)
        {
            pageentry_t pageEntry = virtual_memory_lookup(p->p_pagedir, page_va);
            if(pageEntry != 0)
            {
                uintptr_t physAddr = (uintptr_t)PTE_ADDR(pageEntry);
                virtual_memory_map(p->p_pagedir, page_va, physAddr, PAGESIZE, PTE_P | PTE_U);
            }
        }
    }
    return 0;
}
