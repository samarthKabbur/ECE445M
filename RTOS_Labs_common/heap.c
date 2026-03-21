// filename *************************heap.c ************************
// starter

#include <stdint.h>
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common//OS.h"

long StartCritical(void);
void EndCritical(long);
#define  OSCRITICAL_ENTER(sr) { sr = StartCritical(); }
#define  OSCRITICAL_EXIT(sr)  { EndCritical(sr); }

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
void* Heap_Malloc(int32_t desiredBytes) { // wrapper function
  return Heap_Malloc_Logic(desiredBytes, OS_Id());  // all threads in one process will have to share the same PID, set by add process

  // Heap is given access to the OS here. Not sure if that's a good or bad thing...
}

void* Heap_Malloc_Logic(int32_t desiredBytes, uint8_t pid){ 
  
  int sr;
  OSCRITICAL_ENTER(sr);
  // going with first fit for now
  // Heap functions will expect the calling process to pass in its PID.

  // 1. Find first free block that has enough size
  int32_t desiredWords = (desiredBytes + 3) / 4;  // round up to nearest word

  if (desiredWords <= 0) {
    OSCRITICAL_EXIT(sr);
    return 0;
  }

  int32_t i = 0;
  while (i < HEAP_SIZE_IN_WORDS) {
    int32_t blockSize = heap[pid][i]; // block size in words
    int isFree = (blockSize < 0);
    
    int32_t absBlockSize;
    if (isFree) {
      absBlockSize = -blockSize;  // if free, then the blocksize would've been negative
    } else {
      absBlockSize = blockSize;
    }

    if(isFree && (absBlockSize >= desiredWords)) {
      // if block is big enough, check if we should split
      if (absBlockSize >= (desiredWords + 2)) {
        // split block
        heap[pid][i] = desiredWords;  // header
        heap[pid][i + desiredWords + 1] = desiredWords; // trailer

        int32_t remainingBlock = absBlockSize - desiredWords - 2;
        heap[pid][i + desiredWords + 2] = -remainingBlock;  // header
        heap[pid][i + absBlockSize + 1] = -remainingBlock;
      } else {
        // allocate entire block without splitting
        heap[pid][i] = absBlockSize;  // header. make positive to indicate its being used
        heap[pid][i + absBlockSize + 1] = absBlockSize; // trailer
      }
      
      OSCRITICAL_EXIT(sr);
      return (void*)&heap[pid][i + 1];  // ptr to first word after the header
    }

    // move to next block if not free
    i += absBlockSize + 2;
  }

  OSCRITICAL_EXIT(sr);
  return 0; // none found
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){  
  
  int32_t *data = (int32_t *)Heap_Malloc(desiredBytes); // allocate
  int32_t desiredWords = (desiredBytes + 3) / 4;  // round up to nearest word

  if (data == 0) {
    return 0;  // failed allocation
  }

  for (int i = 0; i < desiredWords; i++) {
    data[i] = 0;  // initialize data to zero
  }

  return data;
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



