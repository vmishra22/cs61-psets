#ifndef M61_H
#define M61_H 1
#include <stdlib.h>
#include <stdbool.h>

void *m61_malloc(size_t sz, const char *file, int line);
void m61_free(void *ptr, const char *file, int line);
void *m61_realloc(void *ptr, size_t sz, const char *file, int line);
void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line);

struct m61_statistics {
    unsigned long long active_count;	// # active allocations
    unsigned long long active_size;	// # bytes in active allocations
    unsigned long long total_count;	// # total allocations
    unsigned long long total_size;	// # bytes in total allocations
    unsigned long long fail_count;	// # failed allocation attempts
    unsigned long long fail_size;	// # bytes in failed alloc attempts
};

void m61_getstatistics(struct m61_statistics *stats);
void m61_printstatistics(void);
void m61_printleakreport(void);

//Memory Header
typedef struct header
{
  size_t payLoadSize;
  size_t Padding;
}MemAllocHeader;

//Memory Footer
typedef struct footer
{
  size_t footerValue;
}MemAllocFooter;

//A double linked list to hold information
//about the memory allocated and freed.
typedef struct listNode
{
  struct listNode* prev;  //previous list node
  void* memPtr;           //allocated pointer pointing to header of memory.
  bool IsFree;            //bool, to check if memory hasbeen freed.
  int allocLineNum;       //Line Number at which malloc call has been made.
  char* allocFileName;    //File Name where malloc call has been made.
  int lineNum;            //Line Number where free call has been made.
  char* fileName;         //File Name where free call has been made.
  struct listNode* next;  //next pointer to list node.
}listNode;
 


#if !M61_DISABLE
#define malloc(sz)		m61_malloc((sz), __FILE__, __LINE__)
#define free(ptr)		m61_free((ptr), __FILE__, __LINE__)
#define realloc(ptr, sz)	m61_realloc((ptr), (sz), __FILE__, __LINE__)
#define calloc(nmemb, sz)	m61_calloc((nmemb), (sz), __FILE__, __LINE__)
#endif

#endif
