#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "assert.h"

unsigned long long mTotalAlloc_Count = 0;
unsigned long long mTotalAlloc_Size = 0;

unsigned long long mActive_Count = 0;
unsigned long long mActive_Size = 0;

unsigned long long mFail_Count = 0;
unsigned long long mFail_Size = 0;

listNode* head = NULL;  //pointer to the head of List.

/*create_node()
Purpose: to create a list node.
Arguments:
    ptr: pointer to memory header.
Return:
    A list node.
*/
listNode* create_node(void* ptr)
{
  listNode* newNode = NULL;
  newNode = (listNode*)malloc(sizeof(listNode));
  newNode->memPtr = ptr;
  newNode->IsFree = false;
  newNode->prev = NULL;
  newNode->next = NULL;

  return newNode;
}


/*insert_node()
Purpose: to insert a node in the list.
Arguments:
    current: current pointer to the node to be inserted in list.
Return:
*/
void insert_node(listNode* current)
{
  if(head == NULL)
  {
    head = current;
  }
  else
  {
    current->next = (void*)head;
    current->prev = NULL;
    head = current;
  }
}


/*contains_node()
Purpose: to check if a node is in the list.
Arguments:
    current: current pointer to the node.
Return:
    'true' or 'false'
*/
bool contains_node(listNode* current)
{
  listNode* temp = head;
  while(temp != NULL)
  {
    if(temp == current)
      return true;
    else
      temp = (listNode*)(temp->next);
  }
  return false;
}


/*remove_node()
Purpose: to remove a node from the list.
Arguments:
    current: current pointer to the node to be removed from list.
Return:
*/
void remove_node(listNode* current)
{
    if(contains_node(current))
    {
	listNode *tmp, *q;
	if(head == current)
	{
	  tmp = head;
	  head = head->next;
	  //head->prev = NULL;
	  free((void*)tmp);
	  return;
	}
	q = head;
	while(q->next->next != NULL)
	{
	    if(q->next == current)
	    {
		tmp = q->next;
		q->next = tmp->next;
		tmp->next->prev = q;
		free((void*)tmp);
		  return;

	    }
	    q = q->next;

	}
	if(q->next == current)
	{
	    tmp = q->next;
	    free((void*)tmp);
	    q->next = NULL;
	    return;
	}
    }
}


/*count_node()
Purpose: Count the nodes in list.
Arguments:
    
Return:
integer count.
*/
int count_node()
{
    listNode* temp = head; 
    int cnt = 0;
    while(temp != NULL)
    {
	temp = (listNode*)(temp->next);
	cnt++;
    }
    return cnt;
}


/*find_node()
Purpose: to check a node in the list.
Arguments:
    pLookUpPtr: input memory header pointer to check in list.
Return:
    the node pointer.
*/
listNode* find_node(void* pLookUpPtr)
{
    listNode* temp = head;
    while(temp != NULL)
    {
	if(((MemAllocHeader*)(temp->memPtr)) == (MemAllocHeader*)pLookUpPtr)
	{
	    return temp;
	}
	else
	{
	    temp = (listNode*)(temp->next);
	}
    }
    return NULL;
}


/*check_not_allocated_ptr()
Purpose: to check a pointer not allocated dynamically.
Arguments:
    pLookUpPtr: pointer t look for in the list.
    pNodePtr: output node pointer.
Return:
    '0' for not found, else '1'
*/
static int check_not_allocated_ptr(const void *pLookUpPtr, listNode** pNodePtr) {
  
  const char  *a = (const char *) pLookUpPtr;
  listNode* temp = head;
    while(temp != NULL)
    {
      size_t payLoadSize = ((MemAllocHeader*)temp->memPtr)->payLoadSize;
      char *b = (char*) temp->memPtr;
      if (a >= b && a < (b + sizeof(MemAllocHeader) + payLoadSize) )
	{
	  *pNodePtr = temp;
	  return 1;
	}
   
      temp = (listNode*) temp->next;
    }
      return 0;
}


/*get_footer()
Purpose: get footer pointer.
Arguments:
    ptr: memory payload pointer.
Return:
    footer pointer.
*/
void* get_footer(void* ptr, size_t sz)
{
  char* a = (char*)ptr;
  void* footerPtr = a + sz;
  return footerPtr;
}

void *m61_malloc(size_t sz, const char *file, int line) {
    (void) file, (void) line;	// avoid uninitialized variable warnings

    //Size of the Header.
    size_t headerSize = sizeof(MemAllocHeader);
    size_t footerSize = sizeof(MemAllocFooter);
    
    //Memory allocation of the block.
    void *memBlockPtr = NULL;
    if( sz < SIZE_MAX)
    {   
      size_t newSizeToAllocate = sz + headerSize + footerSize;
      memBlockPtr = malloc(newSizeToAllocate);
    }
    void *payloadPtr = NULL;

    //Memory allocation successful, increment the counters.
    if(memBlockPtr != NULL) 
    {
      listNode* newListNode = create_node(memBlockPtr);
      newListNode->allocLineNum = line;
      newListNode->allocFileName = (char*) malloc(strlen(file)+1);
      strcpy(newListNode->allocFileName, file);
		    
      insert_node(newListNode);

      ++mTotalAlloc_Count;
    
      ((MemAllocHeader*)memBlockPtr)->payLoadSize = sz;
      
      mTotalAlloc_Size += sz;
      mActive_Size += sz;
      ++mActive_Count;
      //Get the payload pointer and return it.
      payloadPtr = ((MemAllocHeader*)memBlockPtr) + 1;

      MemAllocFooter* pFooter = (MemAllocFooter*)get_footer(payloadPtr, sz);
      pFooter->footerValue = (size_t) -1;
    }
    else
    {
      ++mFail_Count;
      mFail_Size += (sz);
    }
    
    return payloadPtr;
}

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;	// avoid uninitialized variable warnings
    
    if(ptr != NULL)
    {
      MemAllocHeader* pHeader =  ((MemAllocHeader*)ptr)-1;
      listNode* current_node = NULL;

      int num_ListNodes = count_node();

      current_node = find_node((void*)pHeader);
      if(current_node != NULL)
      {
	if(current_node->IsFree)
	  {
	    int oldLine = current_node->lineNum;
	    char* oldFileName = current_node->fileName;
	    printf("MEMORY BUG: %s:%d: double free of pointer %p\n  %s:%d: pointer %p previously freed here", file, line, ptr, oldFileName, oldLine, ptr);
	    abort();
	  }
	else
	  {
	    size_t payloadSize = pHeader->payLoadSize;
	    MemAllocFooter* pFooter = NULL;
	    pFooter = (MemAllocFooter*)get_footer(ptr, payloadSize);
	    if(pFooter && pFooter->footerValue != ((size_t) -1))
	      {
		printf("MEMORY BUG %s %d: detected wild write during free of pointer %p", file, line, ptr);
		abort();	     
	      }
	    else
	      {
		bool check = current_node->IsFree;
		current_node->IsFree = true;
		current_node->lineNum = line;
		current_node->fileName = (char*) malloc(strlen(file)+1);
		strcpy(current_node->fileName, file);
		size_t payloadSize = pHeader->payLoadSize;
		mActive_Size -= payloadSize;
		free((void*)pHeader);
		--mActive_Count;   
	      }
	  }
      }
      else
      {
	  listNode* pNode = NULL;
	  if(check_not_allocated_ptr(pHeader, &pNode) == 1)
	  {
	    void* payloadPtr = ((MemAllocHeader*)pNode->memPtr) + 1;
	    size_t sizeDiff = (char*)ptr - (char*)payloadPtr;
	    printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, ptr);
	    printf("  %s:%d: %p is %zu bytes inside a %zu byte region allocated here",  pNode->allocFileName,
		       pNode->allocLineNum, ptr, sizeDiff, ((MemAllocHeader*)pNode->memPtr)->payLoadSize);
	    abort();
		
	  }
	  else
	  {
	    printf("MEMORY BUG %s:%d: invalid free of pointer %p, not in heap", file, line, ptr);
	    abort();
	  }
	}
    }
}

void *m61_realloc(void *ptr, size_t sz, const char *file, int line) {
    (void) file, (void) line;	// avoid uninitialized variable warnings
    void* new_ptr = NULL;
    
    if(sz != 0)
      new_ptr = m61_malloc(sz, file, line);
    
    if(ptr != NULL && new_ptr != NULL)
    {
       MemAllocHeader* pHeader =  ((MemAllocHeader*)ptr)-1;
       size_t oldPayLoadSize = pHeader->payLoadSize;

       if(oldPayLoadSize < sz)
	 memcpy(new_ptr, ptr, oldPayLoadSize);
       else
	 memcpy(new_ptr, ptr, sz);
    }

    m61_free(ptr, file, line);
    return new_ptr;
}

void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line) {
    (void) file, (void) line;	// avoid uninitialized variable warnings
    void *ptr = NULL;

    //calculate size
    if( sz < (SIZE_MAX)/nmemb)
      ptr = m61_malloc(nmemb*sz, file, line);
    else
      ++mFail_Count;

    if(ptr != NULL)
      memset(ptr, 0, nmemb*sz);

    return ptr;
}

void m61_getstatistics(struct m61_statistics *stats) {
    // Stub: set all statistics to 0
    memset(stats, 0, sizeof(struct m61_statistics));
    
    stats->total_count = mTotalAlloc_Count;
    stats->active_count = mActive_Count;
    stats->fail_count = mFail_Count;

    stats->total_size = mTotalAlloc_Size;
    stats->active_size = mActive_Size;
    stats->fail_size = mFail_Size;
}

void m61_printstatistics(void) {
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n\
malloc size:  active %10llu   total %10llu   fail %10llu\n",
	   stats.active_count, stats.total_count, stats.fail_count,
	   stats.active_size, stats.total_size, stats.fail_size);
}

void m61_printleakreport(void) 
{
    listNode* temp = head;
  
    while(temp != NULL)
    {
      if(!(temp->IsFree))
      {
	size_t payLoadSize = ((MemAllocHeader*)temp->memPtr)->payLoadSize;
	void* payLoadPtr = ((MemAllocHeader*)temp->memPtr) + 1;
	char* fileName = temp->allocFileName;
	int lineNum = temp->allocLineNum;
	  
	printf("LEAK CHECK: %s:%d: allocated object %p with size %zu\n", fileName, lineNum, payLoadPtr, payLoadSize);
      }
      temp = (listNode*)(temp->next);
    }
}
