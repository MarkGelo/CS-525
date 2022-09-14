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
    /*
        Calculation of # pages should be correct cuz should always be in PAGE_SIZE blocks anyways. If don't need whole block, just fill with \0 to reach PAGE_SIZE? right?
    */
    //printf("Size: %d\n", size);
    //printf("Total Num Pages: %d\n", fHandle -> totalNumPages); // checking if calculation is right
    rewind(file); // do i need to do this, since i saved the fp before doing the seek

    //fclose(file);

    return RC_OK;
}

/*
close page file -- close, should i remove fhandle?
*/
RC closePageFile (SM_FileHandle *fHandle){
    FILE *file = fHandle -> mgmtInfo;
    if (fclose(file) != 0){
        return RC_FILE_NOT_FOUND;
    }
    fHandle = NULL;

    return RC_OK;
}

RC destroyPageFile (char *fileName){
    if (remove(fileName) != 0){
        return RC_FILE_NOT_FOUND;
    }
    return RC_OK;
}