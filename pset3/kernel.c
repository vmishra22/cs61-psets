// vim: set tabstop=8: -*- tab-width: 8 -*-
#include "kernel.h"
#include "lib.h"

// kernel.c
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x040000	// initial state only

static proc processes[NPROC];	// array of process descriptors
				// Note that `processes[0]` is never used.
proc *current;			// pointer to currently executing proc

#define HZ 100			// timer interrupt frequency (interrupts/sec)
static unsigned ticks;		// # timer interrupts so far

void schedule(void);
void run(proc *p) __attribute__((noreturn));


// PAGEINFO
//
//    The pageinfo[] array keeps track of information about each physical page.
//    There is one entry per physical page, `NPAGES` total.
//    `pageinfo[pn]` holds the information for physical page number `pn`,
//    which is at address `(pn << PAGESHIFT)`.
//    You can get a physical page number from a physical address `pa` using
//    `PAGENUMBER(pa)`. This also works for page table entries.
//
//    pageinfo[pn].refcount is the number of times physical page `pn` is
//      currently referenced. 0 means it's free.
//    pageinfo[pn].owner is a constant indicating who owns the page.
//      PO_KERNEL means the kernel, PO_RESERVED means reserved memory (such
//      as the console), and a number >=0 means that process ID.
//
//    pageinfo_init() sets up the initial pageinfo[] state.

typedef struct pageinfo {
    int8_t owner;
    int8_t refcount;
} pageinfo_t;

static pageinfo_t pageinfo[NPAGES];

typedef enum pageowner {
    PO_FREE = 0,		// this page is free
    PO_RESERVED = -1,		// this page is reserved memory
    PO_KERNEL = -2		// this page is used by the kernel
} pageowner_t;

static void pageinfo_init(void);


// Memory functions

void virtual_memory_check(void);
void memshow_physical(void);
void memshow_virtual(pageentry_t *pagedir, const char *name);
void memshow_virtual_animate(void);


// start(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, int program_number);

void start(const char *command) {
    hardware_init();
    pageinfo_init();
    console_clear();
    timer_init(HZ);

    //STEP1 : save the kernel and resevred space
/*    size_t kernel_memory_size = MEMSIZE_PHYSICAL - (4*PROC_SIZE);*/
/*    uintptr_t ker_va = 0x00000000;*/
/*    uintptr_t ker_pa = 0x00000000;*/
    
    for (uintptr_t VirAddr = 0; VirAddr < MEMSIZE_VIRTUAL; VirAddr += PAGESIZE) {
        pageentry_t pageEntry = virtual_memory_lookup(kernel_pagedir, VirAddr);
        if(pageEntry != 0)
        {
            if(VirAddr < PROC_START_ADDR )
            {
                if(VirAddr == 0x000B8000)
                    virtual_memory_map(kernel_pagedir, VirAddr, VirAddr, PAGESIZE, PTE_P | PTE_W | PTE_U);
                else
                    virtual_memory_map(kernel_pagedir, VirAddr, VirAddr, PAGESIZE, PTE_P | PTE_W);
            }
            else
                virtual_memory_map(kernel_pagedir, VirAddr, VirAddr, PAGESIZE, PTE_P | PTE_W | PTE_U);
        }
    }
    
    // Set up process descriptors
    memset(processes, 0, sizeof(processes));
    for (pid_t i = 0; i < NPROC; i++) {
	processes[i].p_pid = i;
	processes[i].p_state = P_FREE;
    }

    if (command && strcmp(command, "fork") == 0)
	process_setup(1, 4);
    else if (command && strcmp(command, "forkexit") == 0)
	process_setup(1, 5);
    else
	for (pid_t i = 1; i <= 4; ++i)
	    process_setup(i, i - 1);

    // Switch to the first process using run()
    run(&processes[1]);
}

pageentry_t* copy_pagedir(pageentry_t *pagedir, int8_t owner)
{
    int IsFoundForPageDir = 0;
    pageentry_t* newPageAddr = NULL;
    pageentry_t* newPageTableAddr = NULL;
    for (int pn = 0; pn < NPAGES; ++pn)
    {
        if(pageinfo[pn].refcount == 0)
        {
            IsFoundForPageDir = 1;
            uintptr_t paddr = pn * (PAGESIZE);
            newPageAddr = (pageentry_t*)(paddr);
            pageinfo[pn].refcount = 1;
            pageinfo[pn].owner = owner;
            break;
        }
    }    
    
    int IsFoundForPageTable = 0;
    for (int pn = 0; pn < NPAGES; ++pn)
    {
        if(pageinfo[pn].refcount == 0)
        {
            IsFoundForPageTable = 1;
            uintptr_t paddr = pn * (PAGESIZE);
            newPageTableAddr = (pageentry_t*)(paddr);
            pageinfo[pn].refcount = 1;
            pageinfo[pn].owner = owner;
            break;
        }
    }    
    
    if(!IsFoundForPageDir || !IsFoundForPageTable)
        return NULL;
     
    memset(newPageAddr, 0, PAGESIZE);
    memset(newPageTableAddr, 0, PAGESIZE);
    
    uintptr_t paAddr1 = (uintptr_t)newPageTableAddr;
    newPageAddr[0] = (pageentry_t)(paAddr1| PTE_P | PTE_W | PTE_U); 
    
    for (uintptr_t VirAddr = 0; VirAddr < MEMSIZE_VIRTUAL; VirAddr += PAGESIZE) {
    
        pageentry_t pageEntry = virtual_memory_lookup(pagedir, VirAddr);
        if(pageEntry != 0 )
        {
            uintptr_t physAddr = (uintptr_t)PTE_ADDR(pageEntry);

            if(VirAddr < PROC_START_ADDR )
            {
    
                virtual_memory_map(newPageAddr, VirAddr, physAddr, PAGESIZE, pageEntry & (PTE_P | PTE_W | PTE_U));
            }
            else
            {
                virtual_memory_map(newPageAddr, VirAddr, 0, PAGESIZE, PTE_W | PTE_U );
            }
                    
                //log_printf("\nV1B: proc %p: address %d\n", physAddr, owner);
        }
    }

    return newPageAddr;
}

// process_setup(pid, program_number)
//    Load application program `program_number` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %eip and %esp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, int program_number) {
    process_init(&processes[pid], 0);
    
    pageentry_t* process_pageDir = copy_pagedir(kernel_pagedir, pid);
    processes[pid].p_pagedir = process_pageDir;
    
    //processes[pid].p_pagedir = kernel_pagedir;
    //++pageinfo[PAGENUMBER(kernel_pagedir)].refcount;
    int r = program_load(&processes[pid], program_number);
    assert(r >= 0);
    processes[pid].p_registers.reg_esp = MEMSIZE_VIRTUAL - PROC_SIZE * pid;
    page_alloc_virtual(processes[pid].p_pagedir,
	       processes[pid].p_registers.reg_esp - PAGESIZE, pid);
    processes[pid].p_state = P_RUNNABLE;
}


// page_alloc(pagedir, addr, owner)
//    Allocates the page with physical address `addr` to the given owner,
//    and maps it at the same address in the page directory `pagedir`.
//    The mapping uses permissions `PTE_P | PTE_W | PTE_U` (user-writable).
//    Fails if physical page `addr` was already allocated. Used by the
//    program loader.

int page_alloc(pageentry_t *pagedir, uintptr_t addr, int8_t owner) {
    if ((addr & 0xFFF) != 0 || addr >= MEMSIZE_PHYSICAL
	|| pageinfo[PAGENUMBER(addr)].refcount != 0)
	return -1;
    else {
	pageinfo[PAGENUMBER(addr)].refcount = 1;
	pageinfo[PAGENUMBER(addr)].owner = owner;
	virtual_memory_map(pagedir, addr, addr, PAGESIZE,
			   PTE_P | PTE_W | PTE_U);
	return 0;
    }
}

int page_alloc_virtual(pageentry_t *pagedir, uintptr_t addr, int8_t owner) {
    if ((addr & 0xFFF) != 0 )
	    return -1;
    else 
    {
        int IsFreePageFound = 0;
        for (int pn = 0; pn < NPAGES; ++pn)
        {
            if(pageinfo[pn].refcount == 0)
            {
                pageinfo[pn].refcount = 1;
	            pageinfo[pn].owner = owner;
	            uintptr_t paddr = pn * (PAGESIZE);
                virtual_memory_map(pagedir, addr, paddr, PAGESIZE, PTE_P | PTE_W | PTE_U);
                IsFreePageFound = 1;
                break;
            }
        }
        
        if(!IsFreePageFound)
            return -1;
    }
    
    return 0;
}

void fork_exec()
{
  //look for free slot in process array
    int free_process_found = 0;
    for (pid_t i = 1; i < NPROC; i++) 
    {
        if(processes[i].p_state == P_FREE)
        {
            free_process_found = 1;
            //processes[i].p_pid = i;
            pageentry_t* newPageDir = copy_pagedir(current->p_pagedir, processes[i].p_pid);
            
            processes[i].p_pagedir = newPageDir;
            
            for (uintptr_t addr = PROC_START_ADDR; addr < MEMSIZE_VIRTUAL; addr += PAGESIZE) 
            {
                pageentry_t pageEntry = virtual_memory_lookup(current->p_pagedir, addr);
                if(pageEntry != 0 )
                {
                    uintptr_t pysAddr = (uintptr_t)PTE_ADDR(pageEntry);
                    if(pageEntry & (PTE_W) )
                    {
                        uintptr_t newPaddr = 0;
                        
                        for (int pn = 0; pn < NPAGES; ++pn)
                        {
                            if(pageinfo[pn].refcount == 0)
                            {
                                pageinfo[pn].refcount = 1;
	                            pageinfo[pn].owner = processes[i].p_pid;
	                            newPaddr = pn * (PAGESIZE);
	                            memcpy((pageentry_t*)newPaddr, (pageentry_t*)pysAddr, PAGESIZE);
	                            break;
                            }
                        }
                        
                        if(newPaddr!=0)
                        {
                            virtual_memory_map(newPageDir, addr, newPaddr, PAGESIZE, PTE_P | PTE_W | PTE_U);
                        }
                    }
                    else
                    {
                        log_printf("V1A: proc %d: address %p\n", pageinfo[PAGENUMBER(pysAddr)].owner, pysAddr);
                        if (pageinfo[PAGENUMBER(pysAddr)].refcount > 0 && pageinfo[PAGENUMBER(pysAddr)].owner > 0)
                        {
                            log_printf("V1A: proc %d: address %p\n", pageinfo[PAGENUMBER(pysAddr)].owner, pysAddr);
                            pageinfo[PAGENUMBER(pysAddr)].refcount++;
                            virtual_memory_map(newPageDir, addr, pysAddr, PAGESIZE, PTE_P | PTE_U);
                        }
                    }
                }
                
            }
            processes[i].p_state = P_RUNNABLE;
            processes[i].p_registers = current->p_registers;
            processes[i].p_registers.reg_eax = 0;
            current->p_registers.reg_eax = processes[i].p_pid;
            break;
        }
    }
    
    if(!free_process_found)
    {
    }

//how to return -1 from here?
}

// interrupt(reg)
//    Interrupt handler.
//
//    The register values from interrupt time are stored in `reg`.
//    The processor responds to an interrupt by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-interrupt.S). That code saves more registers on the kernel's stack,
//    then calls interrupt().
//
//    Note that hardware interrupts are disabled for as long as the OS01
//    kernel is running.

void interrupt(struct registers *reg) {
    // Copy the saved registers into the `current` process descriptor
    // and always use the kernel's page directory.
    current->p_registers = *reg;
    lcr3(kernel_pagedir);

    // It can be useful to log events using `log_printf` (see host's `log.txt`).
    /*log_printf("proc %d: interrupt %d\n", current->p_pid, reg->reg_intno);*/

    // Show the current cursor location and memory state.
    console_show_cursor(cursorpos);
    virtual_memory_check();
    memshow_physical();
    memshow_virtual_animate();

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the interrupt.
    switch (reg->reg_intno) {

    case INT_SYS_PANIC:
	panic("%s", (char *) current->p_registers.reg_eax);

    case INT_SYS_GETPID:
	current->p_registers.reg_eax = current->p_pid;
	run(current);

    case INT_SYS_YIELD:
	schedule();

    case INT_SYS_FORK:
    {
        fork_exec();
        schedule();
    }
    
    case INT_SYS_EXIT:
    {
        pageentry_t* pageDir = current->p_pagedir;
        current->p_state = P_FREE;
        
    }
    
    case INT_SYS_PAGE_ALLOC:{
    uintptr_t addr = current->p_registers.reg_eax;
    if(addr >= PROC_START_ADDR || addr == 0x000B8000)
    {
	    current->p_registers.reg_eax =
	        page_alloc_virtual(current->p_pagedir, current->p_registers.reg_eax,
		           current->p_pid);
	    run(current);
	}
	else
	    panic("Page allocation not allowed in Kernel Area\n");
	}

    case INT_TIMER:
	++ticks;
	schedule();

    case INT_PAGEFAULT: {
	// Analyze faulting address and access type.
	uint32_t addr = rcr2();
	const char *operation = (reg->reg_err & PFERR_WRITE ? "write" : "read");
	const char *problem = (reg->reg_err & PFERR_PRESENT ? "protection problem" : "missing page");

	if (!(reg->reg_err & PFERR_USER))
	    panic("Kernel page fault for %08X (%s %s, eip=%p)!\n",
		  addr, operation, problem, reg->reg_eip);
	console_printf(CPOS(24, 0), 0x0C00,
		       "Process %d page fault for %08X (%s %s, eip=%p)!\n",
		       current->p_pid, addr, operation, problem, reg->reg_eip);
	current->p_state = P_BROKEN;
	schedule();
    }

    default:
	panic("Unexpected interrupt %d!\n", reg->reg_intno);

    }
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule(void) {
    pid_t pid = current->p_pid;
    while (1) {
	pid = (pid + 1) % NPROC;
	if (processes[pid].p_state == P_RUNNABLE)
	    run(&processes[pid]);
	// If Control-C was typed, exit the virtual machine.
	check_keyboard();
    }
}



// run(p)
//    Run process `p`. This means reloading all the registers from
//    `p->p_registers` using the `popal`, `popl`, and `iret` instructions.
//
//    As a side effect, sets `current = p`.

void run(proc *p) {
    assert(p->p_state == P_RUNNABLE);
    current = p;

    lcr3(p->p_pagedir);
    asm volatile("movl %0,%%esp\n\t"
		 "popal\n\t"
		 "popl %%es\n\t"
		 "popl %%ds\n\t"
		 "addl $8, %%esp\n\t"
		 "iret"
		 :
		 : "g" (&p->p_registers)
		 : "memory");

 loop: goto loop;		/* should never get here */
}


// pageinfo_init
//    Initialize the `pageinfo[]` array.

void pageinfo_init(void) {
    extern char end[];

    for (uintptr_t addr = 0; addr < MEMSIZE_PHYSICAL; addr += PAGESIZE) {
	int owner;
	if (physical_memory_isreserved(addr))
	    owner = PO_RESERVED;
	else if ((addr >= KERNEL_START_ADDR && addr < (uintptr_t) end)
		 || addr == KERNEL_STACK_TOP - PAGESIZE)
	    owner = PO_KERNEL;
	else
	    owner = PO_FREE;
	pageinfo[PAGENUMBER(addr)].owner = owner;
	pageinfo[PAGENUMBER(addr)].refcount = (owner != PO_FREE);
    }
}


// virtual_memory_check
//    Check operating system invariants about virtual memory. Panic if any
//    of the invariants are false.

void virtual_memory_check(void) {
    // Process 0 must never be used.
    assert(processes[0].p_state == P_FREE);

    // The kernel page directory should be owned by the kernel.
    // Its reference count should equal 1 plus the number of processes that
    // share the directory.
    // Active processes have their own page directories. A process page
    // directory should be owned by that process and have reference count 1.
    // All page table pages must have reference count 1.

    // Calculate expected kernel refcount
    int expected_kernel_refcount = 1;
    for (int pid = 0; pid < NPROC; ++pid)
	if (processes[pid].p_state != P_FREE
	    && processes[pid].p_pagedir == kernel_pagedir)
	    ++expected_kernel_refcount;

    for (int pid = -1; pid < NPROC; ++pid) {
	if (pid >= 0 && processes[pid].p_state == P_FREE)
	    continue;

	pageentry_t *pagedir;
	int expected_owner, expected_refcount;
	if (pid < 0 || processes[pid].p_pagedir == kernel_pagedir) {
	    pagedir = kernel_pagedir;
	    expected_owner = PO_KERNEL;
	    expected_refcount = expected_kernel_refcount;
	} else {
	    pagedir = processes[pid].p_pagedir;
	    expected_owner = pid;
	    expected_refcount = 1;
	}

	// Check page directory itself
	assert(pageinfo[PAGENUMBER(pagedir)].owner == expected_owner);
	assert(pageinfo[PAGENUMBER(pagedir)].refcount == expected_refcount);

	// Check linked page table pages
	for (int pn = 0; pn < PAGETABLE_NENTRIES; ++pn)
	    if (pagedir[pn] & PTE_P) {
		pageentry_t pte = pagedir[pn];
		assert(pageinfo[PAGENUMBER(pte)].owner == expected_owner);
		assert(pageinfo[PAGENUMBER(pte)].refcount == 1);
	    }
    }

    // Check that all referenced pages refer to active processes
    for (int pn = 0; pn < NPAGES; ++pn)
	if (pageinfo[pn].refcount > 0 && pageinfo[pn].owner >= 0)
	    assert(processes[pageinfo[pn].owner].p_state != P_FREE);
}


// memshow_physical
//    Draw a picture of physical memory on the CGA console.

static const uint16_t memstate_colors[] = {
    'K' | 0x0D00, 'R' | 0x0700, '.' | 0x0700,
    '1' | 0x0C00, '2' | 0x0A00, '3' | 0x0900, '4' | 0x0E00,
    '5' | 0x0F00, '6' | 0x0C00, '7' | 0x0A00, '8' | 0x0900, '9' | 0x0E00,
    'A' | 0x0F00, 'B' | 0x0C00, 'C' | 0x0A00, 'D' | 0x0900, 'E' | 0x0E00,
    'F' | 0x0F00
};

void memshow_physical(void) {
    console_printf(CPOS(0, 32), 0x0F00, "PHYSICAL MEMORY");
    for (int pn = 0; pn < NPAGES; ++pn) {
	if (pn % 64 == 0)
	    console_printf(CPOS(1 + pn / 64, 3), 0x0F00, "%08X ", pn << 12);

	int owner = pageinfo[pn].owner;
	if (pageinfo[pn].refcount == 0)
	    owner = PO_FREE;
	uint16_t color = memstate_colors[owner - PO_KERNEL];
	// darker color for shared pages
	if (pageinfo[pn].refcount > 1)
	    color &= 0x77FF;

	console[CPOS(1 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual(pagedir, name)
//    Draw a picture of the virtual memory map `pagedir` (named `name`) on
//    the CGA console.

void memshow_virtual(pageentry_t *pagedir, const char *name) {
    assert((uintptr_t) pagedir == PTE_ADDR(pagedir));

    console_printf(CPOS(10, 26), 0x0F00, "VIRTUAL ADDRESS SPACE FOR %s", name);
    for (uintptr_t va = 0; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
	pageentry_t pte = virtual_memory_lookup(pagedir, va);
	uint16_t color;
	if (!pte)
	    color = ' ';
	else {
	    int owner = pageinfo[PAGENUMBER(pte)].owner;
	    if (pageinfo[PAGENUMBER(pte)].refcount == 0)
		owner = PO_FREE;
	    color = memstate_colors[owner - PO_KERNEL];
	    // reverse video for user-accessible pages
	    if (pte & PTE_U)
		color = ((color & 0x0F00) << 4) | ((color & 0xF000) >> 4)
		    | (color & 0x00FF);
	    // darker color for shared pages
	    if (pageinfo[PAGENUMBER(pte)].refcount > 1)
		color &= 0x77FF;
	}
	uint32_t pn = PAGENUMBER(va);
	if (pn % 64 == 0)
	    console_printf(CPOS(11 + pn / 64, 3), 0x0F00, "%08X ", va);
	console[CPOS(11 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual_animate
//    Draw a picture of process virtual memory maps on the CGA console.
//    Starts with process 1, then switches to a new process every 0.25 sec.

void memshow_virtual_animate(void) {
    static unsigned last_ticks = 0;
    static int showing = 1;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 4) {
	last_ticks = ticks;
	++showing;
    }

    // the current process may have died -- don't display it if so
    while (showing <= 2*NPROC && processes[showing % NPROC].p_state == P_FREE)
	++showing;
    showing = showing % NPROC;

    if (processes[showing].p_state != P_FREE) {
	char s[4];
	snprintf(s, 4, "%d ", showing);
	memshow_virtual(processes[showing].p_pagedir, s);
    }
}
