#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"

// File Related Methods

/*
Not sure what this function is supposed to do
*/
void initStorageManager (void){
    printf("Initialized Storage Manager\n");
}

/*
Create new page file, one page initial file size, filled with '\0' bytes
*/
RC createPageFile (char *fileName){
    if (access(fileName, F_OK) == 0){ // file exists already
        return 1; // dont want to del an already existing file
    }

    FILE *file = fopen(fileName, "w+"); // create file
    
    char *p = malloc(PAGE_SIZE);
    memset(p, '\0', PAGE_SIZE); // "\0" is wrong
    fwrite(p, 1, PAGE_SIZE, file); // ptr, size, nmemb, stream
    free(p);
    fclose(file);
    return RC_OK;
}

/*
Open existing page file, return RC_FILE_NOT_FOUND if doesn't exist
fill file handle with info
*/
RC openPageFile (char *fileName, SM_FileHandle *fHandle){
    FILE *file = fopen(fileName, "r");
    if (file == NULL){
        return RC_FILE_NOT_FOUND;
    }

    fHandle -> fileName = fileName;
    fHandle -> curPagePos = 0; // in beginning
    fHandle -> mgmtInfo = file; // save pointer to file

    fseek(file, 0L, SEEK_END); // seek to end of file, get pos
    int size = ftell(file); // long int
    fHandle -> totalNumPages = size / PAGE_SIZE; // calc num of pages
    //Calculation of # pages should be correct cuz should always be in PAGE_SIZE blocks anyways. If don't need whole block, just fill with \0 to reach PAGE_SIZE? right?
    //printf("Size: %d\n", size);
    //printf("Total Num Pages: %d\n", fHandle -> totalNumPages); // checking if calculation is right
    rewind(file); // do i need to do this, since i saved the fp before doing the seek

    //fclose(file);

    return RC_OK;
}

/*
close page file -- close, should i also remove fhandle?
*/
RC closePageFile (SM_FileHandle *fHandle){
    FILE *file = fHandle -> mgmtInfo;
    if (fclose(file) != 0){
        return RC_FILE_NOT_FOUND;
    }
    fHandle = NULL;

    return RC_OK;
}

/*
When doing destroyPageFile, do closePageFile first, so it closes and also removes fhandle
*/
RC destroyPageFile (char *fileName){
    if (remove(fileName) != 0){
        return RC_FILE_NOT_FOUND;
    }
    return RC_OK;
}

// Read and Write Methods

/*
read block at pos pageNum from the file using fhandle, and then store that page content to memPage
if file has less than the pageNum page, then return RC_READ_NON_EXISTING_PAGE
Page starts at 0
*/
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    // pageNum starts at 0
    if (fHandle -> totalNumPages - 1 < pageNum){ // need to be false to cont ... total 1, pageNum 0 -> 0 < 0 FALSE CORRECT, total 1 pageNum 1 -> 0 < 1 TRUE ERROR
        return RC_READ_NON_EXISTING_PAGE;
    }

    FILE *file = fHandle -> mgmtInfo;
    if (fHandle -> curPagePos == pageNum){ // already in correct pos, ex. curpage 0 and pageNum 0
        fread(memPage, 1, PAGE_SIZE, file);
    }else{
        // have to go to correct pos, start of pageNum
        fseek(file, pageNum * PAGE_SIZE, SEEK_SET);
        fread(memPage, 1, PAGE_SIZE, file);
    }
    fHandle -> curPagePos = pageNum + 1; // read block from pos 0, so 0 -> 1, so now pointer starting at page 1. +1

    return RC_OK;
}

/*
why is this needed when you can just do fHandle -> curPagePos
*/
int getBlockPos (SM_FileHandle *fHandle){
    return fHandle -> curPagePos;
}

/*
The following read blocks are all similar, and can just use the readBlock method
uses fHandle -> curPagePos or getBlockPos cuz they same thing
*/
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(0, fHandle, memPage);
}

RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle -> curPagePos - 1, fHandle, memPage);
}

RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(getBlockPos(fHandle), fHandle, memPage); // just using getBlockPos and fHandle -> curPagePos, interchangeably lmao. should work
}

RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(getBlockPos(fHandle) + 1, fHandle, memPage);
}

RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle -> totalNumPages - 1, fHandle, memPage); //pageNum starts at 0
}

/*
write block to page file according to pagenum, basically same as readblock but write
*/
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    // pageNum starts at 0
    if (fHandle -> totalNumPages - 1 < pageNum){ // need to be false to cont ... total 1, pageNum 0 -> 0 < 0 FALSE CORRECT, total 1 pageNum 1 -> 0 < 1 TRUE ERROR
        return RC_WRITE_FAILED;
    }

    // same thing as readblock but write
    FILE *file = fHandle -> mgmtInfo;
    if (fHandle -> curPagePos == pageNum){ // already in correct pos, ex. curpage 0 and pageNum 0
        //fwrite(memPage, 1, PAGE_SIZE, file);
        // have to go to correct pos, start of pageNum
        fseek(file, pageNum * PAGE_SIZE, SEEK_SET);
        fwrite(memPage, 1, PAGE_SIZE, file);
    }else{
        // have to go to correct pos, start of pageNum
        fseek(file, pageNum * PAGE_SIZE, SEEK_SET);
        fwrite(memPage, 1, PAGE_SIZE, file);
    }
    fHandle -> curPagePos = pageNum + 1; // read block from pos 0, so 0 -> 1, so now pointer starting at page 1. +1

    return RC_OK;
}

RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return writeBlock(getBlockPos(fHandle), fHandle, memPage);
}

/*
append an empty block at end of file - similar to createpage, just gotta make sure to do it at the end of file
*/
RC appendEmptyBlock (SM_FileHandle *fHandle){
    FILE *file = fHandle -> mgmtInfo; // create file
    int curBlock = getBlockPos(fHandle);
    
    if (curBlock == fHandle -> totalNumPages){ // already at end -- 1 total page, beginning is curBlock 0, curBlock 1 means at end, so
        char *p = malloc(PAGE_SIZE);
        memset(p, '\0', PAGE_SIZE);
        fwrite(p, 1, PAGE_SIZE, file);
        free(p);
        fHandle -> totalNumPages += 1;
        fHandle -> curPagePos = curBlock + 1; // writes 1, so pointer is +1 pos
    }else{ // seek to end
        fseek(file, 0L, SEEK_END);
        char *p = malloc(PAGE_SIZE);
        memset(p, '\0', PAGE_SIZE);
        fwrite(p, 1, PAGE_SIZE, file);
        free(p);
        fHandle -> totalNumPages += 1;
        fHandle -> curPagePos = fHandle -> totalNumPages; // this should be right
    }

    return RC_OK;
}

/*
if file has less pages than specified, increase
*/
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle){
    if (fHandle -> totalNumPages < numberOfPages){
        int i;
        for (i = 0; i < numberOfPages - fHandle -> totalNumPages; i++){
            appendEmptyBlock(fHandle);
        }
    }

    return RC_OK;
}