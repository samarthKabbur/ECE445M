// filename *************************heap.c ************************
// starter

#include <stdint.h>
#include "../RTOS_Labs_common/heap.h"

long StartCritical(void);
void EndCritical(long);
#define  OSCRITICAL_ENTER() { sr = StartCritical(); }
#define  OSCRITICAL_EXIT()  { EndCritical(sr); }

#define TRUE 1
#define FALSE 0

/* HEAP DEFINITION */
#define MAX_PROCESSES 32
#define HEAP_SIZE_IN_WORDS 64
static int32_t heap[MAX_PROCESSES][HEAP_SIZE_IN_WORDS];
  // Each process has its own section of heap to prevent overflow, so 32 virtual heaps.
  // Each virtual heap has 64, 32-bit words of space.

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always HEAP_OK
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Heap_Init(void){

  // Here I am using valvano's version of the heap,
  // where the heap is divided into blocks of variable size.

  // Each block has one word at the beginning (Header) and end (Trailer) of itself
  // that stores its size (size counter).
  // This creates two words of overhead.

  // If the counter is negative, the block is free.
  // If the counter is positive, the block is being used.

  // The value of the counter determines the size of the block in 32 bit words,
  // not including the two counters themselves.

  // The number of bytes in a block will be divisible by four, so blocks are aligned to 32-bit boundaries.
  // If the user asks for 17 bytes, 20 bytes will be allocated.

  // Heap_Init() will init two counters at the header and trailer,
  // such that each process' heap will have one giant free block to start with.

  for (int i = 0; i < MAX_PROCESSES - 1; i++){
    heap[i][0] = -(HEAP_SIZE_IN_WORDS - 2);  // Header
    heap[i][HEAP_SIZE_IN_WORDS - 1] = -(HEAP_SIZE_IN_WORDS - 2); // Trailer
  }
  
  return 0;
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes){ //, uint8_t pid){ 

  uint8_t pid = 0;  // temp since idk how to pass in the pid yet
  
  int sr;

  // going with first fit for now
  // Four special cases:
  // no merge, merge above, merge below, merge both above and below

  // Heap functions will expect the calling process to pass in its PID.

  // 1. Find first free block that has enough size
  int32_t block_size;
  uint8_t index = 0;
  uint8_t free;
  do {
    block_size = heap[pid][index];  // load size

    // check if free
    if (block_size < 0) {
      free = TRUE; // free
      block_size = block_size * -1; // make positive
    } else {
      free = FALSE; // not free
    }

    index += block_size + 2;  // move to next block. + 2 to account for the counters.
  } while ((block_size < desiredBytes) && (index < HEAP_SIZE_IN_WORDS) && (free == FALSE)); 
  // ^ keep searching while within bounds, not free, and not big enough

  // 2. Return early if no space available
  if (index == HEAP_SIZE_IN_WORDS) {
    return 0;  // NULL, no space left
  }

  // 3. Check if 
  if (block_size == desiredBytes) {
    
  }

  index -= (block_size + 2); // move index back to block of interest

  // 3. Mark block as allocated
  heap[pid][index] = block_size;  // update header
  heap[pid][index + block_size + 1] = block_size; // update trailer

  // 4. Check for Merge Above
  uint8_t merge_above = FALSE;
  if (heap[pid][index - 1] < 0) { // negative means free
    merge_above = TRUE;
  } else {
    merge_above = FALSE;
  }

  // 5. Check for Merge Below
  uint8_t merge_below = FALSE;
  if ()
  
  return (void*)heap[pid][index - 2]; // move back one block
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){  
  
    return 0; //NULL

}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
//    where the contents of the old block will be copied to
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block will be unallocated after its contents
//   are copied to the new block
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
 
    return 0; // NULL

}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: HEAP_OK if everything is ok;
//  HEAP_ERROR_POINTER_OUT_OF_RANGE if pointer points outside the heap;
//  HEAP_ERROR_CORRUPTED_HEAP if heap has been corrupted or trying to
//  unallocate memory that has already been unallocated;
int32_t Heap_Free(void* pointer){ int sr;
 
  return 0;
}


//******** Heap_Test *************** 
// Test the heap
// input: none
// output: validity of the heap - either HEAP_OK or HEAP_ERROR_HEAP_CORRUPTED
int32_t Heap_Test(void){
 
  return 0;
}


//******** Heap_Stats *************** 
// return the current status of the heap
// input: none
// output: a heap_stats_t that describes the current usage of the heap
int32_t Heap_Stats(heap_stats_t *stats){
  
  
  return 0;
}



