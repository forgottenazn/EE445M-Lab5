// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk
// Jonathan W. Valvano 3/16/11
#include "Integer.h"

int StreamToFile=0; // 0=UART, 1=stream to file

#include "edisk.h"
#include "integer.h"

#define DIRECTORY_FILE_SIZE 3//size of individual files in the directory block
#define BLOCK_SIZE 512	//size of blocks in bytes
#define MAX_BLOCK_INDEX 256	//highest block index that can be addressed
#define DRIVE_NUM 0	//Drive number to access; always 0 for Lab 5
#define FREE_BLOCK_INDEX MAX_BLOCK_INDEX - 1	//Index of free block linked list in directory

BYTE blockBuffer[BLOCK_SIZE];
BYTE directoryBuffer[BLOCK_SIZE];
int directoryCacheStatus;	//1 = directory buffer is current, 0 = needs to be reloaded

struct directoryStruct{
	BYTE fileName;
	BYTE startIndex;
	BYTE size;
} typedef directoryEntryStruct;

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with
//    the disk periodic task operating
int eFile_Init(void){ // initialize file system
	BYTE status;

	status = eDisk_Init(DRIVE_NUM);
	if(status){
		//Error occured in initialization
		return 1;
	}
	else{
		//Initialization was successful
		return 0;
	}
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	int directoryPtr;
	int blockIndex;
	int status;
	BYTE writeByte;
	directoryEntryStruct directoryEntry;

	directoryEntry.fileName = 0;
	directoryEntry.startIndex = 0;
	directoryEntry.size = 0;

	//All files in directory are set to free
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr+=DIRECTORY_FILE_SIZE){
		directoryBuffer[directoryPtr] = directoryEntry.fileName;
		directoryBuffer[directoryPtr+1] = directoryEntry.startIndex;
		directoryBuffer[directoryPtr+2] = directoryEntry.size;
	}

	//Last byte of directory contains starting index of linked list of free blocks
	directoryBuffer[BLOCK_SIZE - 1] = 1;	//All data sectors are set to free

	//Write directory; directory is always in sector 0
	if(eDisk_WriteBlock(directoryBuffer, 0)){
		//Error occured
		return 1;
	}

	//Directory is in sector 0; Data Blocks start at sector 1
	for(blockIndex = 1; blockIndex < MAX_BLOCK_INDEX - 1; blockIndex++){
		//initialize all blocks to free
		//first byte in block is index of next block
		writeByte = blockIndex + 1;	//form free data blocks linked list
		if(eDisk_Write(DRIVE_NUM, &writeByte, blockIndex, 0)){
			//Error occured
			return 1;
		}

	}
	//last free block has a NULL pointer as its next block pointer
	blockIndex++;
	writeByte = 0;
	if(eDisk_Write(DRIVE_NUM, &writeByte, blockIndex, 0)){
		//Error occured
		return 1;
	}

	return 0; //Formatting operation was successful
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters; currently only up to 1
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[]){  // create new file, make it empty
	int directoryPtr;
	BYTE blockIndex;
	BYTE writeByte;
	directoryEntryStruct directoryEntry;

	if(directoryCacheStatus == 0){
		//directoryBuffer is not current, reload
		if(eDisk_ReadBlock(directoryBuffer, 0)){
			//Error occured
			return 1;
		}
	}

	//Build directory entry
	directoryEntry.fileName = name[0];
	directoryEntry.size = 1;

	//find free block to allocate to file
	blockIndex = directoryBuffer[BLOCK_SIZE - 1];
	if(blockIndex == 0){
		//block is available
		directoryEntry.startIndex = blockIndex;
	}


	//Search for empty directory file
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr += DIRECTORY_FILE_SIZE){
		//NULL file name means file is empty; file name is first byte
		if(directoryBuffer[directoryPtr] == 0){
			//File is empty
			directoryBuffer[directoryPtr] = directoryEntry.fileName;
			directoryBuffer[directoryPtr+1] = directoryEntry.startIndex;
			directoryBuffer[directoryPtr+2] = directoryEntry.size;

			//Update free block linked list
			if(eDisk_Read(DRIVE_NUM, &blockIndex, blockIndex, 0)){
				//Error occurred
				return 1;
			}
			directoryBuffer[FREE_BLOCK_INDEX] = blockIndex;

			//update allocated block
			writeByte = 0;
			if(eDisk_Write(DRIVE_NUM, &writeByte, directoryEntry.startIndex, 0)){
				//Error occured
				return 1;
			}
			writeByte = 0;
			if(eDisk_Write(DRIVE_NUM, &writeByte, directoryEntry.startIndex, 1)){
				//Error occured
				return 1;
			}
			writeByte = 0;
			if(eDisk_Write(DRIVE_NUM, &writeByte, directoryEntry.startIndex, 2)){
				//Error occured
				return 1;
			}
			return 0;
		}
	}

	return 1;	//File not created
}
//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[]){      // open a file for writing
	return 1;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data){
	return 1;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){
	return 1;
}


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	return 1;
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[]){      // open a file for reading
	return 1;
}

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte
	return 1;
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	return 1;
}

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(char)){
	int directoryPtr;
	if(eDisk_ReadBlock(directoryBuffer, 0)){
			//Error occured
			return 1;
		}
	
	//All files in directory are set to free
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr+=DIRECTORY_FILE_SIZE){
		if (directoryBuffer[directoryPtr]){ // if the file isn't null
			fp(directoryBuffer[directoryPtr]); // name
			fp(directoryBuffer[directoryPtr+2]); // size
		}
	}
	return 0;
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[]){  // remove this file
	int directoryPtr;
	BYTE blockIndex;
	BYTE writeByte;
	directoryEntryStruct directoryEntry;
	if(directoryCacheStatus == 0){
		//directoryBuffer is not current, reload
		if(eDisk_ReadBlock(directoryBuffer, 0)){
			//Error occured
			return 1;
		}
	}
	//Build directory entry
	directoryEntry.fileName = name[0];
//	directoryEntry.size = 1;

	//Search for the directory file
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr += DIRECTORY_FILE_SIZE){
		//NULL file name means file is empty; file name is first byte
		if(directoryBuffer[directoryPtr] == name[0]){ //found the file
			int i,blockPtr;
			blockPtr = directoryBuffer[directoryPtr+1];
			for (i=0; i< directoryBuffer[directoryPtr+2]; i++){ // for the size of the file
				blockPtr = eDisk_Read(DRIVE_NUM, &writeByte, blockPtr,0);
				//free the blocks of the file
			}
			BYTE* temp = &directoryBuffer[FREE_BLOCK_INDEX];
			eDisk_Write(DRIVE_NUM,&directoryBuffer[directoryPtr+1],directoryBuffer[FREE_BLOCK_INDEX],0);
			//blockPtr = temp;
			eDisk_Write(DRIVE_NUM,temp,blockPtr,0);
			// removing the file from directory
			directoryBuffer[directoryPtr] = 0;
			directoryBuffer[directoryPtr+1] = 0;
			directoryBuffer[directoryPtr+2] = 0;
			return 0;
		}
	}
	return 1;
}

//---------- eFile_RedirectToFile-----------------
// open a file for writing
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)

int eFile_RedirectToFile(char *name){
 eFile_Create(name); // ignore error if file already exists
 if(eFile_WOpen(name)) return 1; // cannot open file
 StreamToFile = 1;
 return 0;
}
//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)


int eFile_EndRedirectToFile(void){
 StreamToFile = 0;
 if(eFile_WClose()) return 1; // cannot close file
 return 0;
}


