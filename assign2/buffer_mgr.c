#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dberror.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"

// Buffer Pool Functions

// create new bufferpool, used to cache pages from the page file. All page frames should initially be empty. Page file should already exist.
// stratData can be used to pass parameters for the page replacement strategy. For example, for LRU-k this could be the parameter k

// can use the mgmtData to store any necessary information about a buffer pool that you need to implement the interface. For example, this could
// include a pointer to the area in memory that stores the page frames or data structures needed by the page replacement strategy to make replacement decisions.
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy,
		void *stratData){
    if (access(pageFileName, F_OK) == 0){ // file should exists already
        return RC_FILE_NOT_FOUND;
    }
    SM_FileHandle fh;
    if(openPageFile(pageFileName, &fh) != RC_OK){ // shouldnt need to, but just i ncase
        return RC_FILE_NOT_FOUND;
    }

    bm -> pageFile = pageFileName;
    bm -> numPages = numPages;
    bm -> strategy = strategy;
    bm -> numWriteIO = 0;
    bm -> numReadIO = 0;

    // make page table and frames
    BM_PageFrame *pageFrames = malloc(sizeof(BM_PageFrame) * numPages);
    for(int i = 0; i < numPages; i++){
        pageFrames[i].dirtyFlag = false; // or pageFrames[i] -> dirtyFlag ??
        pageFrames[i].pinCounter = 0;
        pageFrames[i].pageNum = NO_PAGE;
        pageFrames[i].data = NULL;
    }

    BM_PageTable *pageTable = malloc(sizeof(BM_PageTable));
    pageTable -> pageFrames = pageFrames;
    pageTable -> numFrames = 0;
    pageTable -> fh = fh;

    PageNumber *frameContents = malloc(sizeof(PageNumber) * numPages);
    bool *dirtyFlags = malloc(sizeof(bool) * numPages);
    int *fixCounts = malloc(sizeof(int) * numPages);
    for(int i = 0; i < numPages; i++){
        frameContents[i] = NO_PAGE;
        dirtyFlags[i] = false;
        fixCounts[i] = 0;
    }
    pageTable -> frameContents = frameContents;
    pageTable -> dirtyFlags = dirtyFlags;
    pageTable -> fixCounts = fixCounts;

    bm -> mgmtData = pageTable; // pointer to an area in memory that stores page frames or data structures . maybe ADD a section for random, like to keep stratData

    return RC_OK;
}

// all dirty pages are written to disk
RC forceFlushPool(BM_BufferPool *const bm){
    BM_PageTable *pageTable = bm -> mgmtData;
    BM_PageFrame *pageFrames = pageTable -> pageFrames;
    for(int i = 0; i < bm -> numPages; i ++){
        if(pageFrames[i].page != NO_PAGE && pageFrames[i].dirty == true){
        }
    }
}

RC shutdownBufferPool(BM_BufferPool *const bm){

}

// Page Management Functions




// Statistics Functions


PageNumber *getFrameContents (BM_BufferPool *const bm);
bool *getDirtyFlags (BM_BufferPool *const bm){
    return 0;
}

int *getFixCounts (BM_BufferPool *const bm){
    return 0;
}

int getNumReadIO (BM_BufferPool *const bm){
    return bm -> numReadIO;
}

int getNumWriteIO (BM_BufferPool *const bm){
    return bm -> numWriteIO;
}