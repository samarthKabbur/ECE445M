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

#define SUCCESS 0
#define FAIL 1

// Block 0 → Directory
// Block 1 → FAT
// Block 2+ → Data blocks


#define MAXBLOCK 256
#define MAXFILES 60
#define DATASIZE 510         // 512 (-2 data counter)
struct aBlock{
  unsigned short size;   // number of bytes in this block 0 to DATASIZE
  char data[DATASIZE];   // blockes are exactly 512 bytes
};                                
typedef struct aBlock BlockType;

uint8_t FAT[MAXBLOCK]; //FAT file system

uint8_t FreeList[MAXBLOCK]; //indexed free space management 
int FreeCount;


struct Entry{                // size = 16 bytes/file
  char Name[8];              // file name, up to 7 characters
  uint8_t First;
};  
typedef struct Entry EntryType;

struct aDirectory{
  EntryType File[MAXFILES];    // up to 60 files 
}; 

typedef struct aDirectory DirectoryType;
#define NONE {0,0,0,0,0,0,0,0}            
const DirectoryType BlankDirectory = { 
{ { NONE,0},    // first file
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, { NONE, 0}, 
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, 
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, 
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, 
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, 
  { NONE,0}, { NONE,0}, { NONE,0}, { NONE,0}, 
  { NONE,0}}
};
DirectoryType Directory;          // RAM copy of directory
int DirectoryIn;                  // 1 if Directory is loaded

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
//this file should set all files 
return 0;
}

// bring directory from flash into RAM
// Output: 1 if successful and 0 on failure (e.g., trouble reading from flash)
int FetchDirectory(void){
return 0;
}

// save RAM-copy of directory out to flash
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int BackupDirectory(void){
  return 0;

}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
return 0;

}

// unlink a free block, return a block pointer of a free block
// assumes directory is loaded into RAM
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int AllocateBlock(unsigned long *pt){   
  return 0;

}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
return 0;

}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
return 0;

}

//---------- eFile_Write-----------------
// Save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){unsigned long newBlock;
return 0;
}

//---------- eFile_WriteString-----------------
// Save at end of the open file
// Input: pointer to string to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WriteString(const char *pt){ int max=512; 
 
  return SUCCESS;
}

//-----------------------eFile_WriteUDec-----------------------
// Write a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Variable format 1-10 digits with space before and no space after
int eFile_WriteUDec(uint32_t n){
  
  return 0;
}

//-----------------------eFile_WriteSDec-----------------------
// Write a 32-bit number in signed decimal format
// Input: 32-bit number to be transferred
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// Variable format 1-10 digits with space before and no space after
int eFile_WriteSDec(int32_t num){
  
  return eFile_WriteString(0);
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
  
  return eFile_WriteString(0);
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
return 0;
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 

  return SUCCESS;     
}
 
//---------- eFile_ReadNext-----------------
// Retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 

  return FAIL; // end of file
}
//---------- eFileReadNextWord-----------------
// Retreive 32-bit little endian word from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
uint32_t eFileReadNextWord(uint32_t *pt){char data; int status; *pt=0;

  return SUCCESS;
}
//---------- eFile_RClose-----------------
// Close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
 
  return SUCCESS;
}


//---------- eFile_Delete-----------------
// Delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 


  return BackupDirectory();    // restore directory back to flash
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
  return SUCCESS;
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
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
  return FAIL;          // error, because not open
}
