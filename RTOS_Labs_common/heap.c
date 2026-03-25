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
#define MAX_PROCESSES 3
#define HEAP_SIZE_IN_WORDS 1900
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

  for (int i = 0; i < MAX_PROCESSES; i++){
    heap[i][0] = -(HEAP_SIZE_IN_WORDS - 2);  // Header
    heap[i][HEAP_SIZE_IN_WORDS - 1] = -(HEAP_SIZE_IN_WORDS - 2); // Trailer
  }
  
  return 0; // 0 is apparently success for heap init
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes) { // wrapper function
  return Heap_Malloc_Logic(desiredBytes, OS_PId());  // all threads in one process will have to share the same PID, set by add process

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
        heap[pid][i] = desiredWords;                    // header
        heap[pid][i + desiredWords + 1] = desiredWords; // trailer

        int32_t remainingBlock = absBlockSize - desiredWords - 2;
        heap[pid][i + desiredWords + 2] = -remainingBlock;  // header
        heap[pid][i + absBlockSize + 1] = -remainingBlock;  // trailer
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
 
  // 1. Allocate New Block
  int32_t *newBlock = (int32_t *)Heap_Malloc(desiredBytes);
  if (newBlock == 0) {
    return 0; // failed allocation
  }

  // 2. Copy Contents of Old Block to New Block
  int32_t *old_block_data = (int32_t *)oldBlock;
  int32_t oldSize = old_block_data[-1]; // go back one word to access the size

  for (int i = 0; i < oldSize; i++) {
    newBlock[i] = old_block_data[i];
  }

  // 3. Deallocate Old Block
  Heap_Free(oldBlock);

  return (void*)newBlock;
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: HEAP_OK if everything is ok;
//  HEAP_ERROR_POINTER_OUT_OF_RANGE if pointer points outside the heap;
//  HEAP_ERROR_CORRUPTED_HEAP if heap has been corrupted or trying to
//  unallocate memory that has already been unallocated;
int32_t Heap_Free(void* pointer){ int sr;
  return Heap_Free_Logic(pointer, OS_PId());
}

int32_t Heap_Free_Logic(void* pointer, uint8_t pid) {
  if (pointer == 0) {
    return 1; // fail on invalid free, 1 is apparently fail for heap_free
  }

  uint8_t merge_above = 0;
  uint8_t merge_below = 0;

  int32_t *block_data = (int32_t *)pointer;
  int32_t block_size = block_data[-1];  // block size is positive, since it is assumed to be previously allocated
  int32_t header_index = block_data - &heap[pid][0] - 1;  // index of the header relative to heap
  
  // Four cases:
  // No merge, Merge Above, Merge Below, and Merge both Above and Below
  // Two special cases:
  // If the block is the first block in the heap, cannot merge it above
  // If block is the last block in the heap, cannot merge it below

  // 1. Check for merge above
  if ((header_index > 0) && (block_data[-2] < 0)) {
    merge_above = 1;
  }

  // 2. Check for merge below
  if (((header_index + block_size + 2) < HEAP_SIZE_IN_WORDS) 
        && (block_data[block_size + 1] < 0)) {
          merge_below = 1;
  }

  // 3. Handle merges
  if (merge_above && merge_below) {
    // Merge both above and below
    int32_t prev_size = -block_data[-2];
    int32_t next_size = -block_data[block_size + 1];
    int32_t merged_size = prev_size + block_size + next_size + 4;  // +4 for both pairs of counters
    
    // Update prev block's header and new trailer
    block_data[-(prev_size + 3)] = -merged_size;  // prev block header
    block_data[block_size + next_size + 2] = -merged_size;  // next block's trailer
    
  } else if (merge_above) {
    // Merge above only
    int32_t prev_size = -block_data[-2];
    int32_t merged_size = prev_size + block_size + 2;
    
    block_data[-(prev_size + 3)] = -merged_size;  // prev header
    block_data[block_size] = -merged_size;  // current trailer
    
  } else if (merge_below) {
    // Merge below only
    int32_t next_size = -block_data[block_size + 1];
    int32_t merged_size = block_size + next_size + 2;
    
    block_data[-1] = -merged_size;  // current header
    block_data[block_size + next_size + 2] = -merged_size;  // next trailer
    
  } else {
    // 4. No merge, just deallocate
    block_data[-1] = -block_size;
    block_data[block_size] = -block_size;
  }

return 0; // 0 is apparently success for heap_free
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
  return Heap_Stats_Logic(stats, OS_PId());
}

int32_t Heap_Stats_Logic(heap_stats_t *stats, uint8_t pid) {
  stats->size = HEAP_SIZE_IN_WORDS * 4; // heap size in bytes

  // Find number of bytes used and number of bytes free
  uint32_t used_bytes = 0;
  uint32_t free_bytes = 0;

  for (int i = 0; i < HEAP_SIZE_IN_WORDS;) {
    int32_t block_size = heap[pid][i];  // read header
    int32_t abs_block_size;
    
    if (block_size < 0) {
      abs_block_size = -block_size;
      free_bytes += abs_block_size * 4;
    } else {
      abs_block_size = block_size;
      used_bytes += abs_block_size * 4;
    }

    i+= abs_block_size + 2; // move to next block
  }

  stats->used = used_bytes;
  stats->free = free_bytes;
  
  return 0;
}



