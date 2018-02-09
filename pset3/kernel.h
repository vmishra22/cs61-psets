// vim: set tabstop=8: -*- tab-width: 8 -*-
#ifndef WEENSYOS_KERNEL_H
#define WEENSYOS_KERNEL_H
#include "x86.h"

// kernel.h
//
//    Functions, constants, and definitions for the OS01 kernel.


// Process state type
typedef enum procstate {
    P_FREE = 0,				// free slot
    P_RUNNABLE,				// runnable process
    P_BLOCKED,				// blocked process
    P_BROKEN				// faulted process
} procstate_t;

// Process descriptor type
typedef struct proc {
    pid_t p_pid;			// process ID
    struct registers p_registers;	// process's current registers
    procstate_t p_state;		// process state (see above)
    pageentry_t *p_pagedir;		// process's page directory
} proc;

#define NPROC 16		// maximum number of processes


// Kernel start address
#define KERNEL_START_ADDR	0x40000
// Top of the kernel stack
#define KERNEL_STACK_TOP	0x80000

// First application-accessible address
#define PROC_START_ADDR		0x100000

// Physical memory size
#define MEMSIZE_PHYSICAL	0x200000
// Number of physical pages
#define NPAGES			(MEMSIZE_PHYSICAL / PAGESIZE)

// Virtual memory size
#define MEMSIZE_VIRTUAL		0x300000

// Hardware interrupt numbers
#define INT_HARDWARE		32
#define INT_TIMER		(INT_HARDWARE + 0)


// hardware_init
//    Initialize x86 hardware, including memory, interrupts, and segments.
//    All 8MB of accessible physical memory is initially mapped as readable
//    and writable to both kernel and application code.
void hardware_init(void);

// timer_init(rate)
//    Set the timer interrupt to fire `rate` times a second. Disables the
//    timer interrupt if `rate <= 0`.
void timer_init(int rate);


// kernel page directory (used for virtual memory)
extern pageentry_t kernel_pagedir[];

// virtual_memory_map(pagedir, va, pa, sz, perm)
//    Change the mappings for virtual address range `[va, va+sz)` in
//    kernel page directory `pagedir`.
//
//    Virtual address `va + X` maps to physical address `pa + X` for all
//    `X >= 0 && X < sz`. The mapping has permission bits `perm`.
//
//    `va`, `pa`, and `sz` must be multiples of PAGESIZE (4096). `perm`
//    should be a combination of `PTE_P` (the memory is Present), `PTE_W`
//    (the memory is Writable), and `PTE_U` (the memory may be accessed by
//    User applications).
void virtual_memory_map(pageentry_t *pagedir, uintptr_t va, uintptr_t pa,
			size_t sz, int perm);

// virtual_memory_lookup(pagedir, va)
//    Returns the page entry corresponding to virtual address `va` in page
//    directory `pagedir`. Returns 0 if `va` is not mapped; otherwise,
//    returns some `pte`.
//
//    To extract the physical address of the mapped page, use `PTE_ADDR(pte)`.
//    To extract flags, use `(pte & PTE_U)` and `(pte & PTE_W)`.
pageentry_t virtual_memory_lookup(pageentry_t *pagedir, uintptr_t va);

// page_alloc(pagedir, addr, owner)
//    Allocates the page with physical address `addr` to the given owner,
//    and maps it at the same address in the page directory `pagedir`.
//    The mapping uses permissions `PTE_P | PTE_W | PTE_U` (user-writable).
//    Fails if physical page `addr` was already allocated. Used by the
//    program loader.
int page_alloc(pageentry_t *pagedir, uintptr_t addr, int8_t owner);

int page_alloc_virtual(pageentry_t *pagedir, uintptr_t addr, int8_t owner);

pageentry_t* copy_pagedir(pageentry_t *pagedir, int8_t owner);

// physical_memory_isreserved(pa)
//    Returns non-zero iff `pa` is a reserved physical address.
int physical_memory_isreserved(uintptr_t pa);

// poweroff
//    Turn off the virtual machine.
void poweroff(void) __attribute__((noreturn));

// reboot
//    Reboot the virtual machine.
void reboot(void) __attribute__((noreturn));


// console_show_cursor(cpos)
//    Move the console cursor to position `cpos`, which should be between 0
//    and 80 * 25.
void console_show_cursor(int cpos);


// keyboard_readc
//    Read a character from the keyboard. Returns -1 if there is no character
//    to read, and 0 if no real key press was registered but you should call
//    keyboard_readc() again (e.g. the user pressed a SHIFT key). Otherwise
//    returns either an ASCII character code or one of the special characters
//    listed below.
int keyboard_readc(void);

#define KEY_UP		0300
#define KEY_RIGHT	0301
#define KEY_DOWN	0302
#define KEY_LEFT	0303
#define KEY_HOME	0304
#define KEY_END		0305
#define KEY_PAGEUP	0306
#define KEY_PAGEDOWN	0307
#define KEY_INSERT	0310
#define KEY_DELETE	0311

// check_keyboard
//    Check for the user typing a control key. 'a', 'f', and 'e' cause a soft
//    reboot where the kernel runs the allocator programs, "fork", or
//    "forkexit", respectively. Control-C or 'q' exit the virtual machine.
void check_keyboard(void);


// process_init(p, flags)
//    Initialize special-purpose registers for process `p`. Constants for
//    `flags` are listed below.
void process_init(proc *p, int flags);

#define PROCINIT_ALLOW_PROGRAMMED_IO	0x01
#define PROCINIT_DISABLE_INTERRUPTS	0x02


// program_load(p, programnumber)
//    Load the code corresponding to program `programnumber` into the pross
//    `p` and set `p->p_registers.reg_eip` to its entry point. Calls
//    `page_alloc` to allocate virtual memory for `p` as required. Returns 0
//    on success and -1 on failure (e.g. out-of-memory).
int program_load(proc *p, int programnumber);


// log_printf, log_vprintf
//    Print debugging messages to the host's `log.txt` file. We run QEMU
//    so that messages written to the QEMU "parallel port" end up in `log.txt`.
void log_printf(const char *format, ...) __attribute__((noinline));
void log_vprintf(const char *format, va_list val) __attribute__((noinline));

#endif
