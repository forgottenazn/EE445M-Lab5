// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk
// Jonathan W. Valvano 3/16/11

int StreamToFile=0; // 0=UART, 1=stream to file

#include "edisk.h"

#define DIRECTORY_FILE_SIZE 3//size of individual files in the directory block
#define BLOCK_SIZE 512	//size of blocks in bytes
#define MAX_BLOCK_INDEX 256	//highest block index that can be addressed
#define DRIVE_NUM 0	//Drive number to access; always 0 for Lab 5
#define FREE_BLOCK_INDEX BLOCK_SIZE - 1	//Index of free block linked list in directory
#define NULL 0

BYTE blockBuffer[BLOCK_SIZE];
BYTE directoryBuffer[BLOCK_SIZE];
BYTE fileBuffer[BLOCK_SIZE];
int fileDirectoryIndex;		//directory index of file currently open
int directoryCacheStatus;	//1 = directory buffer is current, 0 = needs to be reloaded
int fileBlockIndex;
int fileStatus;		//0 = a file is not open, 1 = a file is open
int writeStatus;	//0 = read mode; 1 = write mode;
int readIndex;

struct directoryStruct{
	BYTE fileName;
	BYTE startIndex;
	BYTE size;
} typedef directoryEntryStruct;

//---------- getFreeBlock --------------
//Returns 0 if no free block available or index of a free block
//block returned is not initialized
int getFreeBlock(void){
	int blockIndex;

	//read directory
	if(eDisk_ReadBlock(directoryBuffer, 0)){
		//Error occurred
		return 1;
	}

	blockIndex = directoryBuffer[FREE_BLOCK_INDEX];

	if(blockIndex == 0){
		//no free blocks
		return 0;
	}

	//Update free block linked list
	if(eDisk_ReadBlock(blockBuffer, blockIndex)){
		//Error occurred
		return 1;
	}

	//next free block is now first element of linked list
	directoryBuffer[FREE_BLOCK_INDEX] = blockBuffer[0];

	if(eDisk_WriteBlock(directoryBuffer, 0)){
		//Error occurred
		return 1;
	}

	return blockIndex;
}


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
	directoryBuffer[FREE_BLOCK_INDEX] = 1;	//All data sectors are set to free

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
		blockBuffer[0] = writeByte;
		if(eDisk_WriteBlock(blockBuffer, blockIndex)){
			//Error occured
			return 1;
		}

	}
	//last free block has a NULL pointer as its next block pointer
	blockIndex++;
	writeByte = 0;
	blockBuffer[0] = writeByte;
	if(eDisk_WriteBlock(blockBuffer, blockIndex)){
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
	blockIndex = directoryBuffer[FREE_BLOCK_INDEX];
	if(!(blockIndex == NULL)){
		//A block is available; use it
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

			//Free block will be used; Update free block linked list
			if(eDisk_ReadBlock(blockBuffer, blockIndex)){
				//Error occurred
				return 1;
			}
			//first available free block is now next free block in linked list
			directoryBuffer[FREE_BLOCK_INDEX] = blockBuffer[0];

			//update allocated block
			blockBuffer[0] = NULL;	//only 1 block for new file
			blockBuffer[1] = blockBuffer[2] = NULL; //file size is 0

			//Write blocks to disk
			if(eDisk_WriteBlock(directoryBuffer, 0) | eDisk_WriteBlock(blockBuffer, blockIndex)){
				return 1;
			}
			return 0;	//File created
		}
	}

	return 1;	//File not created
}
//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[]){      // open a file for writing
	int directoryPtr;
	int nextIndex;
	directoryEntryStruct directoryEntry;

	if(fileStatus){
		//Error: A file is already open
		return 1;
	}

	//Update file status
	fileStatus = 1;	//File open
	writeStatus = 1;	//Write mode

	//Load directory into RAM
	if(eDisk_ReadBlock(directoryBuffer, 0)){
		//Error occured
		return 1;
	}

	//Search for file
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr += DIRECTORY_FILE_SIZE){
		if(directoryBuffer[directoryPtr] == name[0]){
			//names are the same; file found
			directoryEntry.fileName = directoryBuffer[directoryPtr];
			directoryEntry.startIndex = directoryBuffer[directoryPtr+1];
			directoryEntry.size = directoryBuffer[directoryPtr+2];
			fileDirectoryIndex = directoryPtr;
		}
	}

	if(directoryEntry.fileName == NULL){
		//file was never found
		return 1;
	}

	nextIndex = directoryEntry.startIndex;
	//find last block of file
	while(nextIndex != NULL){
		//First byte of file is index to next block
		if(eDisk_ReadBlock(blockBuffer, nextIndex)){
			//Error occurred
			return 1;
		}
		//Byte will be null if it's the last block of linked list
		if(blockBuffer[0] == NULL){
			break;
		}
		nextIndex = blockBuffer[0];	//continue searching
	}

	fileBlockIndex = nextIndex;

	//Load last block of file into RAM
	if(eDisk_ReadBlock(fileBuffer, fileBlockIndex)){
		//error occurred
		return 1;
	}

	//file was opened successfully
	return 0;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data){
	BYTE writeByte;
	int freeBlockIndex;
	int blockSize;
	int dataIndex;

	//check that a file is open and in write mode
	if(!(fileStatus & writeStatus)){
		//Error: No file open or file open in read mode
		return 1;
	}

	if(eDisk_ReadBlock(fileBuffer, fileBlockIndex)){
		//Error occurred
		return 1;
	}

	writeByte = data;

	//Get size of block
	//Size of block is in 2nd (high byte) and 3rd (low byte) of data block
	blockSize = (((int) fileBuffer[1]) << 8) + (fileBuffer[2]);

	//set write index
	dataIndex = blockSize + 3;

	if(dataIndex == (BLOCK_SIZE)){
		freeBlockIndex = getFreeBlock();
		if(freeBlockIndex == 0){
			//file system full, cannot write
			return 1;
		}

		fileBuffer[0] = freeBlockIndex;

		//Write modified block
		if(eDisk_WriteBlock(fileBuffer, fileBlockIndex)){
			//Error occurred
			return 1;
		}

		//Load free block for use
		fileBlockIndex = freeBlockIndex;
		if(eDisk_ReadBlock(fileBuffer, fileBlockIndex)){
			//Error occurred
			return 1;
		}
		//Initialize new block
		fileBuffer[0] = NULL;
		fileBuffer[1] = fileBuffer[2] = 0;
		dataIndex = 3;
		blockSize = 0;

		//Increase size of file in directory
		if(eDisk_ReadBlock(directoryBuffer, 0)){
			//error occurred
			return 1;
		}
		directoryBuffer[fileDirectoryIndex + 2] += 1;
		if(eDisk_WriteBlock(directoryBuffer, 0)){
			//Error occurred
			return 1;
		}
	}

	fileBuffer[dataIndex] = writeByte;	//write data
	blockSize++;

	//Update blockSize
	fileBuffer[1] = (blockSize & 0xFF00) >> 8;
	fileBuffer[2] = blockSize & 0x00FF;

	//write blockBuffer to disk
	if(eDisk_WriteBlock(fileBuffer, fileBlockIndex)){
		//Error occurred
		return 1;
	}

	//Data successfully written to file
	return 0;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){
	if(fileStatus){
	//A file is open; close it
	fileStatus = 0;
	}
	else{
		//Error: no file open
		return 1;
	}

	return 0;
}


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	//check that a file is open and in write mode
	if(!(fileStatus & writeStatus)){
		//Error: No file open or file open in read mode
		return 1;
	}

	fileStatus = 0;
	writeStatus = 0;

	return 0; //File successfuly closed
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[]){      // open a file for reading
	int directoryPtr;
	directoryEntryStruct directoryEntry;

	if(fileStatus){
		//Error: A file is already open
		return 1;
	}

	//Update file status
	fileStatus = 1;	//File open
	writeStatus = 0;	//read mode

	//Load directory into RAM
	if(eDisk_ReadBlock(directoryBuffer, 0)){
		//Error occured
		return 1;
	}

	//Search for file
	for(directoryPtr = 0; directoryPtr < BLOCK_SIZE - 1; directoryPtr += DIRECTORY_FILE_SIZE){
		if(directoryBuffer[directoryPtr] == name[0]){
			//names are the same; file found
			directoryEntry.fileName = directoryBuffer[directoryPtr];
			directoryEntry.startIndex = directoryBuffer[directoryPtr+1];
			directoryEntry.size = directoryBuffer[directoryPtr+2];
			fileDirectoryIndex = directoryPtr;
		}
	}

	if(directoryEntry.fileName == NULL){
		//file was never found
		return 1;
	}

	fileBlockIndex = directoryEntry.startIndex;

	//Load first block of file into RAM
	if(eDisk_ReadBlock(fileBuffer, fileBlockIndex)){
		//error occurred
		return 1;
	}

	//set readIndex to 0
	readIndex = 0;

	//file was opened successfully
	return 0;
}

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte
	int blockSize;

	//Get block size
	blockSize = (((int)fileBuffer[1]) << 8) + fileBuffer[2];



	if(readIndex < blockSize){
		*pt = fileBuffer[readIndex + 3];	//first 3 bytes of block are block parameters
		readIndex++;
	}
	else{
		if(fileBuffer[0]){
			//Go to next block
			fileBlockIndex = fileBuffer[0];
			if(eDisk_ReadBlock(fileBuffer, fileBlockIndex)){
				//Error occurred
				return 1;
			}
			//Read next byte
			if(fileBuffer[2]){
				//Block not empty
				readIndex = 1;	//Set read index for next read call
				*pt = fileBuffer[readIndex + 2];	//read first data byte of block
			}
			else{
				//End of file
				return 1;
			}
		}
		else{
			//End of file
			return 1;
		}
	}

	return 0; //data successfully read
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	//check that a file is open and in write mode
	if(!(fileStatus & !writeStatus)){
		//Error: No file open or file open in read mode
		return 1;
	}

	fileStatus = 0;
	writeStatus = 0;

	return 0; //File successfuly closed
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
			for (i=0; i< directoryBuffer[directoryPtr+2] - 1; i++){ // for the size of the file
				//free the blocks of the file
				if(eDisk_ReadBlock(blockBuffer, blockPtr)){
					//Error occurred
					return 1;
				}
				blockPtr = blockBuffer[0];
			}
			BYTE temp = directoryBuffer[FREE_BLOCK_INDEX];
			blockBuffer[0] = directoryBuffer[directoryPtr+1];
			if(eDisk_WriteBlock(blockBuffer, directoryBuffer[FREE_BLOCK_INDEX])){
				//Error occurred
				return 1;
			}
			blockBuffer[0] = temp;
			if(eDisk_WriteBlock(blockBuffer, blockPtr)){
				//Error occurred
				return 1;
			}
			// removing the file from directory
			directoryBuffer[directoryPtr] = 0;
			directoryBuffer[directoryPtr+1] = 0;
			directoryBuffer[directoryPtr+2] = 0;
			if(eDisk_WriteBlock(directoryBuffer, 0)){
				//Error occurred
				return 1;
			}
			return 0;	//File deleted successfully
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
