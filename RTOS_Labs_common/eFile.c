// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 12/27/25
// Solution to lab 4
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include <stdio.h>
//should have no file with starting index 0, 1, or 2

#define MAXFILES 60
#define MAXBLOCKS 256
#define DATA_START_BLOCK 3
#define NONAME {0,0,0,0,0,0,0,0}
#define DATASIZE 510  // = 512 - 2 (data counter)
#define BLOCKSIZE 512 // bytes per block
#define SUCCESS 0
#define FAIL 1
#define DBLOCKNUM 0
#define DSIZE 2
#define FSIZE 3
#define NULLINDEX 0
#define NOT_OPEN 255

typedef struct Block {
  uint16_t size; // number of bytes in this block (0 to DATASIZE)
  char data[DATASIZE];
} Block_t;  // 512 bytes = 1 block

typedef struct Entry {
  char Name[8]; // name of the file, 8 bytes
  uint8_t First;  // index of the first location, 1 byte
} Entry_t;  // 12 bytes

typedef struct Directory {
  Entry_t File[MAXFILES]; // 12 bytes * 60 files = 720 bytes
} Directory_t;  // 724 bytes = 2 blocks

uint8_t FAT[MAXBLOCKS]; // File Allocation Table = 1/2 block

typedef struct Filesystem {
  Directory_t Directory;          // file directory
  uint8_t Bitmap[MAXBLOCKS/8];    // 32-byte free-space bitmap
  uint8_t FAT[MAXBLOCKS];         // FAT table (256 bytes)
} Filesystem_t; // 3 blocks ?

//will probably put this in header file
const Filesystem_t BlankFilesystem = {
    // Directory
    {
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},
      {NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0},{NONAME,0}
    },
    
    // Bitmap: first 3 blocks (0–2) used, rest free
    {
        0x03, // blocks 0–2 used (bits 0–2)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00
    },
    // FAT: all zeros
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    }
  
};

int OpenFlag = 0; // 0 means file system not initialized
int FilesystemIn;

Filesystem_t Filesystem; // RAM Copy of filesystem and directory

int DCurrentEntry;
int WOpenFile; // directory index of the currently open file for writing (0 to 60)
Block_t WCurrentBlock; // RAM copy of block current open for writing
unsigned long WBlockNum; // which block is stored in WCurrentBlock

int ROpenFile; // directory index of the currently open file for reading (0 to 60)
Block_t RCurrentBlock; // 512 bytes of RAM copy of block used during reading
unsigned long RBlockNum; // which block is stored in RCurrentBlock
unsigned long RByteCnt; // which byte will be read next (0 to DATASIZE-1)

uint16_t TempBlock[128]; // 512 byte block used temporarily

//---------- eFile_Init-----------------
// Activate the file system, without formatting
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
  if (OpenFlag) return SUCCESS;

  eDisk_Init(0); // initialize hardware, drive 0
  OpenFlag = 1;
  WOpenFile = NOT_OPEN; // not open WCurrentBlock is unused
  ROpenFile = NOT_OPEN; // not open RCurrentBlock is unused
  FilesystemIn = 0; // filesystem not loaded

  return SUCCESS;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format

  uint32_t scheduler_lock = OS_LockScheduler();
  if (!OpenFlag) {
    OS_UnLockScheduler(scheduler_lock);
    return FAIL;  // file system not initialized
  }

  // ERASE ALL FILES
  if (eDisk_Write(0,(const BYTE *)&BlankFilesystem, DBLOCKNUM, FSIZE)) {
    OS_UnLockScheduler(scheduler_lock);
    return FAIL;  // write block error
  }
   OS_UnLockScheduler(scheduler_lock);
   return SUCCESS;
}

//bring filesystem from flash to ram
int FetchFilesystem(void){
   FilesystemIn = 1;  // Filesystem
  if( eDisk_Read(0,(BYTE *)&Filesystem, DBLOCKNUM, FSIZE)){ // first block is directory
    FilesystemIn = 0; 
    return FAIL; 
  } 
  FilesystemIn = 1;  
  return SUCCESS; 

}

// save RAM-copy of filesystem out to flash
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int BackupFilesystem(void){
  return eDisk_Write(0, (const BYTE *)&Filesystem,DBLOCKNUM, FSIZE); // first 3 blocks are filesystem
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
if(!OpenFlag){
    return FAIL; // not initialized
  }  
 if(FilesystemIn){
   return FAIL; // already mounted
 }
  if(FetchFilesystem()){
    return FAIL;        // problem fetching directory
  }
  return SUCCESS;

}

//****************Bitmap Helper Functions*****************************//

void Bitmap_SetFree(uint16_t block){
  Filesystem.Bitmap[block/8] &= ~(1 << (block%8));
}


int Bitmap_IsFree(uint16_t block){
  return !(Filesystem.Bitmap[block/8] & (1 << (block%8)));
}

void Bitmap_SetUsed(uint16_t block){
  Filesystem.Bitmap[block/8] |= (1 << (block%8));
}

int FindFreeBlock(void){
  for(int i = DATA_START_BLOCK; i < MAXBLOCKS; i++){
    if(Bitmap_IsFree(i)){
      return i;
    }
  }
  return -1;
}

//********************************************************************//


// get rid of a free block, return a block index of a free block
// assumes directory is loaded into RAM
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int AllocateBlock(uint8_t *pt){
    int block = FindFreeBlock();   // find first free block
    if(block == -1){
        return FAIL; // failure, no free blocks
    }

    Bitmap_SetUsed(block);         // mark it as used
    *pt = block;                   // return block index

    // Optional: write bitmap back to SD card
    WCurrentBlock.size = 0;

     return eDisk_WriteBlock((const BYTE *)&WCurrentBlock,*pt); // update new block size

}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
  uint8_t first;

  // CHECKS
  if (!OpenFlag) return FAIL; // not initialized
  if (strlen(name) > 7) return FAIL;  // name too long
  if (FilesystemIn == 0) { // read if not already in memory
    if (FetchFilesystem()) return FAIL;  // read error
  }
  for (int i = 0; i < MAXFILES; i++) {
    if (strcmp(Filesystem.Directory.File[i].Name, name) == 0) return FAIL; // file with name match alr exists
  }
    
  int free_file_entry = 0;        // search for free directory entry spot which is different than searching for free block(cant use findfreeblock)
  while((free_file_entry<MAXFILES)&&(Filesystem.Directory.File[free_file_entry].Name[0])){
    free_file_entry++;
  }
  if(free_file_entry==MAXFILES){
    return FAIL;   // full directory, up to 60 files
  }

  // BEGIN ALLOCATION
  if (AllocateBlock(&first)) return FAIL; // allocation fail
  //first gives free block index

  // update directory
  strcpy(Filesystem.Directory.File[free_file_entry].Name, name);
  Filesystem.Directory.File[free_file_entry].First = first;

  // update FAT
  Filesystem.FAT[first] = NULLINDEX; // first block in the file so it points to null

  return BackupFilesystem(); // restore filesystem back to flash
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
  int open_file_idx;
  if (!OpenFlag) return FAIL; // not initialized
  if (WOpenFile!=NOT_OPEN) return FAIL; // already open
  if (!FilesystemIn) {
    if (FetchFilesystem()) return FAIL; // problem fetching filesystem
  }
  // search for matching filename
  int file_index = 0; 
  while ((file_index < MAXFILES) && (strcmp(Filesystem.Directory.File[file_index].Name, name))) {
    file_index++;
  }
  if ((file_index == MAXFILES) || (file_index == ROpenFile)) {  // can't have the same file open for read and write
    return FAIL;
  }

  WOpenFile = file_index;
  WBlockNum = Filesystem.Directory.File[file_index].First;
  int next_file_index = Filesystem.FAT[file_index]; // fat contains index to next file
  while (next_file_index != NULLINDEX) {  // keep going till end of file
    WBlockNum = next_file_index;
    next_file_index = Filesystem.FAT[next_file_index];  // traverse
  }

  if (eDisk_ReadBlock((BYTE *)&WCurrentBlock, WBlockNum)) {
    WOpenFile = NOT_OPEN; // set to not open
    return FAIL;  // failed to read the data block
  }

  return SUCCESS;
}

//---------- eFile_Write-----------------
// Save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){uint8_t newBlock;
if(!OpenFlag){
    return FAIL;   // not initialized
  }
  if(WOpenFile==NOT_OPEN){
    return FAIL;   // not open
  }
  if(WCurrentBlock.size >= DATASIZE){ // this block full?
    if(AllocateBlock(&newBlock)){
      eDisk_WriteBlock((const BYTE *)&WCurrentBlock,WBlockNum); // save full block to disk
      WOpenFile = NOT_OPEN;       // disk full, close
      BackupFilesystem();
      return FAIL;            // problem allocating next block
    }
    Filesystem.FAT[WBlockNum] = newBlock;   // link previous to new one
    if(eDisk_WriteBlock((const BYTE *)&WCurrentBlock,WBlockNum)){ // save full block to disk
      WOpenFile = NOT_OPEN;
      return FAIL;   //trouble writing a data block
    }
    WBlockNum = newBlock; // new one becomes current
    Filesystem.FAT[WBlockNum] = NULLINDEX;  // new one has null pointer
    WCurrentBlock.size = 0;     // new one is empty
    
  }
  WCurrentBlock.data[WCurrentBlock.size] = data; // save into RAM buffer
  WCurrentBlock.size++;
  return SUCCESS;
}

//---------- eFile_WriteString-----------------
// Save at end of the open file
// Input: pointer to string to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WriteString(const char *pt){ int max=512; 
  while(*pt){
    if(eFile_Write(*pt)) return FAIL;   //trouble writing
    pt++;
    max--;
    if(max==0)return FAIL;   //buffer overflow
  }
  return SUCCESS;
}

//-----------------------eFile_WriteUDec-----------------------
// Write a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Variable format 1-10 digits with space before and no space after
int eFile_WriteUDec(uint32_t n){
  char eOutBuf[12];
  eOutBuf[11] = 0;
  int i=10;
  do{
    eOutBuf[i] = '0'+n%10;
    n = n/10;
    i--;
  }while(n);
  eOutBuf[i] = ' ';
  return eFile_WriteString(&eOutBuf[i]);
}

//-----------------------eFile_WriteSDec-----------------------
// Write a 32-bit number in signed decimal format
// Input: 32-bit number to be transferred
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Variable format 1-10 digits with space before and no space after
int eFile_WriteSDec(int32_t num){
  char eOutBuf[12]; 
  int32_t n;
  if(num<0){
    n = -num;
  } else{
    n = num;
  }
  eOutBuf[11] = 0;
  int i=10;
  do{
    eOutBuf[i] = '0'+n%10;
    n = n/10;
    i--;
  }while(n);
  if(num<0){
    eOutBuf[i] = '-';
  } else{
    eOutBuf[i] = ' ';
  }  
  eOutBuf[i-1] = ' ';
  return eFile_WriteString(&eOutBuf[i-1]);
}

//-----------------------eFile_WriteSFix2-----------------------
// Write a 32-bit number in signed fixed point format
// signed 32-bit with resolution 0.01
// range -999.99 to +999.99
// Input: signed 32-bit integer part of fixed point number
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Examples
//   72345 to " 723.45"  
//  -22100 to "-221.00"
//    -102 to "  -1.02" 
//      31 to "   0.31" 
// -100000 to " ***.**"  
int eFile_WriteSFix2(int32_t num){
  char eOutBuf[8];
  int32_t n;
  if((num>99999)||(num<-99999)){
     return eFile_WriteString(" ***.**");
  }
  if(num<0){
    n = -num;
    eOutBuf[0] = '-';
  } else{
    n = num;
    eOutBuf[0] = ' ';
  }
  if(n>9999){
    eOutBuf[1] = '0'+n/10000;
    n = n%10000;
    eOutBuf[2] = '0'+n/1000;
  } else{
    if(n>999){
      if(num<0){
        eOutBuf[0] = ' ';
        eOutBuf[1] = '-';
      } else {
        eOutBuf[1] = ' ';
      }
      eOutBuf[2] = '0'+n/1000;
    } else{
      if(num<0){
        eOutBuf[0] = ' ';
        eOutBuf[1] = ' ';
        eOutBuf[2] = '-';
      } else {
        eOutBuf[1] = ' ';
        eOutBuf[2] = ' ';
      }
    }
  }
  n = n%1000;
  eOutBuf[3] = '0'+n/100;
  n = n%100;
  eOutBuf[4] = '.';
  eOutBuf[5] = '0'+n/10;
  n = n%10;
  eOutBuf[6] = '0'+n;
  eOutBuf[7] = 0;
  return eFile_WriteString(eOutBuf);
}


//-----------------------eFile_WriteUFix2-----------------------
// Write a 32-bit number in signed fixed point format
// unsigned 32-bit with resolution 0.01
// range  0.00 to 999.99
// Input: unsigned 32-bit integer part of fixed point number
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Examples
//   72345 to " 723.45"  
//   22100 to " 221.00"
//     102 to "   1.02" 
//      31 to "   0.31" 
//  100000 to " ***.**"  
int eFile_WriteUFix2(uint32_t num){
  if(num>99999){
     return eFile_WriteString(" ***.**");
  }
  return eFile_WriteSFix2((int32_t) num);
}

//---------- eFile_WClose-----------------
// Close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
if(!OpenFlag){
    return FAIL;     // not initialized
  }
  if(WOpenFile==NOT_OPEN){
    return FAIL;     // not open
  }
  WOpenFile = NOT_OPEN; // Now closed for writing
  if(eDisk_WriteBlock((const BYTE *)&WCurrentBlock,WBlockNum)){ // save full block to disk
    return FAIL;   // trouble writing a data block
  }
  return BackupFilesystem();    // restore directory back to flash
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
int i; 
  if(!OpenFlag){
    return FAIL;   // not initialized
  }
  if(ROpenFile!=NOT_OPEN){
    return FAIL;   // already open
  }
  if(!FilesystemIn){ // load if not previously loaded
    if(!FetchFilesystem()){
      return FAIL;   // problem fetching directory
    }
  }
  i = 0;          // search for matching filename
  while((i<60) && (strcmp(Filesystem.Directory.File[i].Name,name))){
    i++;
  }
  if((i==60)||(i==WOpenFile)){   // can't have the same file open for read and write
    return FAIL;   // file does not exist
  }
  ROpenFile = i;
  RBlockNum = Filesystem.Directory.File[i].First;
  if(eDisk_ReadBlock((BYTE *)&RCurrentBlock,RBlockNum)){  // fetch data block
    ROpenFile = NOT_OPEN;
    return 1;   // trouble reading a data block
  }                              
  RByteCnt = 0; // start at the top of the block
  return SUCCESS;     
}

//---------- eFile_ReadNext-----------------
// Retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
  if(!OpenFlag){
    return FAIL;   // not initialized
  }
  if(ROpenFile==NOT_OPEN){
    return FAIL;   // not open
  }
  if(RByteCnt < RCurrentBlock.size){ // this block have data?
    *pt = RCurrentBlock.data[RByteCnt];
    RByteCnt++;
    return SUCCESS; 
  }
  uint8_t nextBlock = Filesystem.FAT[RBlockNum];
    if(nextBlock == 0){  // 0 = end of file
        return FAIL;      // no more data
    }
    RBlockNum = nextBlock;
  if(eDisk_ReadBlock((BYTE *)&RCurrentBlock,RBlockNum)){  // fetch data block
    ROpenFile = NOT_OPEN;
    return FAIL;   // trouble reading a data block
  }                              
  RByteCnt = 0; // start at the top of the block
  if(RCurrentBlock.size){ // this block have any data?
    *pt = RCurrentBlock.data[0];
    RByteCnt++;
    return SUCCESS; 
  }
  return FAIL; // end of file
}
//---------- eFileReadNextWord-----------------
// Retreive 32-bit little endian word from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
uint32_t eFileReadNextWord(uint32_t *pt){char data; int status; *pt=0;
  for(int i=0; i<32; i=i+8){
    status = eFile_ReadNext(&data);
    if(status==0){
      (*pt) |= data<<i; // little endian
    }
    else return FAIL;
  }
  return SUCCESS;
}
//---------- eFile_RClose-----------------
// Close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  if(!OpenFlag){
    return FAIL;   // not initialized
  }
  if(ROpenFile==NOT_OPEN){
    return FAIL;   // not open
  }
  ROpenFile = NOT_OPEN; // Now closed for reading
  return SUCCESS;
}


//---------- eFile_Delete-----------------
// Delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
  // TODO: delete file
  return BackupFilesystem();    // restore filesystem back to flash
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
  if(!OpenFlag){
    return FAIL;       // not initialized
  }
  if(!FilesystemIn){ // load if not previously loaded
    if(FetchFilesystem()){
      return FAIL;     // problem fetching directory
    }
  }  
  DCurrentEntry = 0;
  return SUCCESS;
}

//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
  if(!OpenFlag){
    return FAIL;       // not initialized
  }
  if(!FilesystemIn){ 
    return FAIL;       // not opened
  }  
  while(DCurrentEntry<60){
    if(Filesystem.Directory.File[DCurrentEntry].Name[0]){  // file exists, if name is nonzero
      *name = Filesystem.Directory.File[DCurrentEntry].Name;
      //*size = Filesystem.Directory.File[DCurrentEntry].Size;
      DCurrentEntry++;
      return SUCCESS;
    }
    DCurrentEntry++;
  }
  return FAIL;  
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
  return SUCCESS;  // nothing to do here
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){ 
  if(OpenFlag){
    OpenFlag = 0;    // closed
    WOpenFile = NOT_OPEN; // not open
    ROpenFile = NOT_OPEN; // not open
    FilesystemIn = 0; // directory not loaded
    return SUCCESS;  
  }
  return FAIL;          // error, because not open
}