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

#define MAXFILES 60
#define MAXBLOCKS 249
#define NONAME {0,0,0,0,0,0,0,0} 

typedef struct Entry {
  char Name[8]; // name of the file, 8 bytes
  uint8_t First;  // index of the first location, 1 byte
} Entry_t;  // 12 bytes

typedef struct Directory {
  Entry_t File[MAXFILES]; // 12 bytes * 60 files = 720 bytes
  uint8_t Free_Index; // 1 byte
} Directory_t;  // 724 bytes = 2 blocks

// For formatting the SDC
const Directory_t BlankDirectory = {
  {
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0},
    {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}, {NONAME, 0}
  }, // 60 DATA FILES
  {NONAME, 0}
};

Directory_t Directory;  // RAM copy of directory = 2 blocks
uint8_t FAT[MAXBLOCKS]; // File Allocation Table = 1 block

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system

}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format

}

// bring directory from flash into RAM
// Output: 1 if successful and 0 on failure (e.g., trouble reading from flash)
int FetchDirectory(void){

}

// save RAM-copy of directory out to flash
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int BackupDirectory(void){

}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system

}

// unlink a free block, return a block pointer of a free block
// assumes directory is loaded into RAM
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int AllocateBlock(unsigned long *pt){   

}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 

}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 

}

//---------- eFile_Write-----------------
// Save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){unsigned long newBlock;

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
  if(ROpenFile!=255){
    return FAIL;   // already open
  }
  if(!DirectoryIn){ // load if not previously loaded
    if(!FetchDirectory()){
      return FAIL;   // problem fetching directory
    }
  }
  i = 0;          // search for matching filename
  while((i<31) && (strcmp(Directory.File[i].Name,name))){
    i++;
  }
  if((i==31)||(i==WOpenFile)){   // can't have the same file open for read and write
    return FAIL;   // file does not exist
  }
  ROpenFile = i;
  RBlockNum = Directory.File[i].First;
  if(eDisk_ReadBlock((BYTE *)&RCurrentBlock,RBlockNum)){  // fetch data block
    ROpenFile = 255;
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
  if(ROpenFile==255){
    return FAIL;   // not open
  }
  if(RByteCnt < RCurrentBlock.size){ // this block have data?
    *pt = RCurrentBlock.data[RByteCnt];
    RByteCnt++;
    return SUCCESS; 
  }
  if(RCurrentBlock.next==0){    // no more blocks
    return FAIL; // end of file
  }
  RBlockNum = RCurrentBlock.next;   // next block
  if(eDisk_ReadBlock((BYTE *)&RCurrentBlock,RBlockNum)){  // fetch data block
    ROpenFile = 255;
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
  if(ROpenFile==255){
    return FAIL;   // not open
  }
  ROpenFile = 255; // Now closed for reading
  return SUCCESS;
}


//---------- eFile_Delete-----------------
// Delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
int i; unsigned short blknum;

  if(!OpenFlag){
    return FAIL;   // not initialized
  }
  if(WOpenFile!=255){
    return FAIL;     // can't delete a file, if one open for writing
  }
  if(!DirectoryIn){ // load if not previously loaded
    if(FetchDirectory()){
      return FAIL;   // problem fetching directory
    }
  }
  i = 0;          // search for matching filename
  while((i<31) && (strcmp(Directory.File[i].Name , name))){
    i++;
  }
  if(i==31){
    return FAIL;   // file doesn't exist
  }
  Directory.File[i].Name[0] = 0;  // delete directory entry
  Directory.File[i].Size = 0;  // empty file
  
  blknum = Directory.File[i].First;
  if(blknum){
    if(eDisk_ReadBlock((BYTE *)TempBlock,blknum)){   // fetch data block
      return FAIL;   // trouble reading a data block
    }
    while(TempBlock[0]){    // keep reading until find the last block
      blknum = TempBlock[0];
      if(eDisk_ReadBlock((BYTE *)TempBlock,blknum)){     // fetch data block
        return FAIL;   // trouble reading a data block
      }
    }
    TempBlock[0] = Directory.Free.First;
    if(eDisk_WriteBlock((const BYTE *)TempBlock,blknum)){ // save full block to disk
      return FAIL;   //trouble writing a data block
    }
    Directory.Free.First =  Directory.File[i].First;
    Directory.File[i].First = 0;
  }
  return BackupDirectory();    // restore directory back to flash
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
  if(!DirectoryIn){ // load if not previously loaded
    if(FetchDirectory()){
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
  if(!DirectoryIn){ 
    return FAIL;       // not opened
  }  
  while(DCurrentEntry<31){
    if(Directory.File[DCurrentEntry].Name[0]){  // file exists, if name is nonzero
      *name = Directory.File[DCurrentEntry].Name;
      *size = Directory.File[DCurrentEntry].Size;
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
    WOpenFile = 255; // not open
    ROpenFile = 255; // not open
    DirectoryIn = 0; // directory not loaded
    return SUCCESS;  
  }
  return FAIL;          // error, because not open
}
