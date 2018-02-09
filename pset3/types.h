// vim: set tabstop=8: -*- tab-width: 8 -*-
#ifndef WEENSYOS_TYPES_H
#define WEENSYOS_TYPES_H
#ifndef __ASSEMBLER__

#ifndef NULL
#define NULL ((void*) 0)
#endif

// Explicitly-sized versions of integer types
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

// Pointers and addresses are 32 bits long.
// We use pointer types to represent virtual addresses,
// uintptr_t to represent the numerical values of virtual addresses,
// and physaddr_t to represent physical addresses.
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t physaddr_t;

typedef uint32_t pageentry_t;
typedef pageentry_t *pagedirectory_t;

// Page numbers are 32 bits long.
typedef uint32_t ppn_t;

// size_t is used for memory object sizes.
typedef uint32_t size_t;
// ssize_t is a signed version of ssize_t, used in case there might be an
// error return.
typedef int32_t ssize_t;

// off_t is used for file offsets and lengths.
typedef int32_t off_t;

// pid_t is used for process IDs.
typedef int32_t pid_t;

// Efficient min and max operations
#define MIN(_a, _b)						\
({								\
	typeof(_a) __a = (_a);					\
	typeof(_b) __b = (_b);					\
	__a <= __b ? __a : __b;					\
})
#define MAX(_a, _b)						\
({								\
	typeof(_a) __a = (_a);					\
	typeof(_b) __b = (_b);					\
	__a >= __b ? __a : __b;					\
})

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)						\
({								\
	uint32_t __a = (uint32_t) (a);				\
	(typeof(a)) (__a - __a % (n));				\
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)						\
({								\
	uint32_t __n = (uint32_t) (n);				\
	(typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n));	\
})

// Return the offset of 'member' relative to the beginning of a struct type
#define offsetof(type, member)  ((size_t) (&((type*)0)->member))

// Varargs
typedef struct {
    char *va_ptr;
} va_list;
#define	__va_size(type)							\
	(((sizeof(type) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))
#define	va_start(val, lastarg)						\
	((val).va_ptr = (char *)&(lastarg) + __va_size(lastarg))
#define	va_arg(val, type)						\
	(*(type *)((val).va_ptr += __va_size(type),			\
		   (val).va_ptr - __va_size(type)))
#define	va_end(val)	((void)0)

#endif /* !__ASSEMBLER__ */
#endif /* !WEENSYOS_TYPES_H */
