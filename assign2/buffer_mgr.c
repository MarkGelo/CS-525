#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dberror.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"

int globalTime = 0; // keep time, used for keeping time when frames were used, LRU

// initialize page table with numPages frames, and also including framecontents, dirtyflags, fix counts arrays, as easability and so stats functions is already implemented
BM_PageTable *initPageTable(int numPages) {
    BM_PageTable *table = MAKE_PAGE_TABLE();
    table -> frames = malloc(sizeof(BM_PageFrame *) * numPages);
    PageNumber *frameContents = malloc(sizeof(PageNumber) * numPages);
	bool *dirtyFlags = malloc(sizeof(bool) * numPages);
	int *fixCounts = malloc(sizeof(int) * numPages);

    int i;
    for(i = 0; i < numPages; i++) {
        table -> frames[i] = NULL; // frame should be empty
        frameContents[i] = NO_PAGE; // since frame empty, shouldnt have anything in frame content
        dirtyFlags[i] = false;
        fixCounts[i] = 0;
    }
    // for stats and easability
    table -> frameContents = frameContents;
    table -> dirtyFlags = dirtyFlags;
    table -> fixCounts = fixCounts;

    table -> numFramesUsed = 0;

    return table;
}

// buffer manager interface pool handling

// initialize buffer pool with numPages page frames. This makes sure file exists already, and if so, readies the buffer pool and creates a page table for it
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy,
		void *stratData){
    
    if(access(pageFileName, F_OK) != 0){ // file should exists already
        return RC_FILE_NOT_FOUND;
    }

    bm -> pageFile = (char *) pageFileName;
    bm -> numPages = numPages;
    bm -> strategy = strategy;
    bm -> numWriteIO = 0;
    bm -> numReadIO = 0;
    
    // create page table , with numPages page frames, also with other arrays for stats, hash?
    bm -> mgmtData = initPageTable(numPages);

    return RC_OK;
}

void freeFrame(BM_PageFrame *curFrame){
    free(curFrame -> page -> data);
    //free(curFrame -> dirtyFlag);
    //free(curFrame -> fixCount);
    //free(curFrame -> age);
    //free(curFrame -> timeUsed);
    free(curFrame -> page);
    free(curFrame);

    return;
}

void freeTable(BM_PageTable *table){
    free(table -> frames);
    free(table -> frameContents);
    free(table -> dirtyFlags);
    free(table -> fixCounts);
    free(table);

    return;
}

// shutdown buffer pool, freeing everything and making sure dirty pages are written back. if any pinned, errors out.
RC shutdownBufferPool(BM_BufferPool *const bm){
    // assumes buffer pool is init already
    SM_FileHandle fh;
    if(openPageFile(bm -> pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    BM_PageTable *table = bm -> mgmtData;

    // go through each frame and see if dirty, if so write back

    // todo, just check dirtyflags and fixcounts array? but should be same efficiency
    for(int i = 0; i < bm -> numPages; i++) {
        BM_PageFrame *curFrame = table -> frames[i];
        if(curFrame == NULL){
            continue;
        }
        if(curFrame -> fixCount != 0) {
            printf("ERROR - Attempting to shutdown buffer pool that has a pinned page");
            return -3;
        }
        if(curFrame -> dirtyFlag) {
            writeBlock(curFrame -> page -> pageNum, &fh, curFrame -> page -> data);
        }
        freeFrame(curFrame);

    }
    freeTable(table);

    closePageFile(&fh);
    return RC_OK;
}

// all dirty pages are written back, ONLY if fix count 0
RC forceFlushPool(BM_BufferPool *const bm){
    SM_FileHandle fh;
    if(openPageFile(bm -> pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    BM_PageTable *table = bm -> mgmtData;
    for(int i = 0; i < bm -> numPages; i++){
        BM_PageFrame *curFrame = table -> frames[i];
        if(curFrame == NULL){
            continue;
        }
        if(curFrame -> dirtyFlag && curFrame -> fixCount == 0){ // only if fix count 0 and dirty, then write
            writeBlock(curFrame -> page -> pageNum, &fh, curFrame -> page -> data);
            curFrame -> dirtyFlag = false;
            table -> dirtyFlags[i] = false;
            bm -> numWriteIO += 1;
        }
    }

    closePageFile(&fh);
    return RC_OK;
}

// page management functions

int getFrame(BM_BufferPool *const bm, const PageNumber pageNum){
    BM_PageTable *table = bm -> mgmtData;
    // todo use framecontents array to do this instead? faster?
    for(int i = 0; i < bm -> numPages; i++){
        BM_PageFrame *curFrame = table -> frames[i];
        if(curFrame == NULL){
            continue;
        }
        if(curFrame -> page -> pageNum == pageNum){
            return i;
        }
    }

    return -1; // couldnt find page in the table.
}

// first iterate over pagetable, checking if page is in it, if not error, if so, mark it as dirty
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page){
    int idx = getFrame(bm, page -> pageNum);
    if(idx == -1){ // could not find page - error
        return -3;
    }
    BM_PageTable *table = bm -> mgmtData;
    table -> frames[idx] -> dirtyFlag = true; // mark that page as dirty
    table -> dirtyFlags[idx] = true; // also update array
    return RC_OK;
}

// same thing as markDirty, but unpin it, added an error check. if fixcount already 0, dont unpin, aka -1 it, as may case errors later on
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page){
    int idx = getFrame(bm, page -> pageNum);
    if(idx == -1){ // could not find page - error
        return -3;
    }
    BM_PageTable *table = bm -> mgmtData;
    if(table -> frames[idx] -> fixCount == 0){ // if fixCount == 0, dont do -1, cuz may case errors later on
        // or maybe just ignore if this case dont, do -1
        printf("Attempting to unpin page, when it is not pinned at all, fixCount == 0");
        // just leave it at 0
    }else{
        table -> frames[idx] -> fixCount -= 1; // unpin, -1 to fixCount.
        table -> fixCounts[idx] -= 1; // also update array
    }
    return RC_OK;
}

// write back to disk, if found. mark as not dirty cuz written back
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page){
    int idx = getFrame(bm, page -> pageNum);
    if(idx == -1){ // could not find page - error
        return -3;
    }
    SM_FileHandle fh;
    if(openPageFile(bm -> pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    BM_PageTable *table = bm -> mgmtData;
    writeBlock(table -> frames[idx] -> page -> pageNum, &fh, table -> frames[idx] -> page -> data);
    bm -> numWriteIO += 1;
    table -> frames[idx] -> dirtyFlag = false;
    table -> dirtyFlags[idx] = false; // also update array

    closePageFile(&fh);
    return RC_OK;
}

RC FIFO(BM_BufferPool *const bm, BM_PageHandle *const page, SM_FileHandle fh){
    BM_PageTable *table = bm -> mgmtData;

    // check fixcounts array, if all non zero, cant do evict anyone
    int i;
    int avail = 0;
    for(i = 0; i < bm -> numPages; i++){
        if(table -> fixCounts[i] == 0){
            avail += 1;
        }
    }
    if(avail == 0){ // all frames are pinned, nothing can be evicted
        return -3;
    }

    int first = 0; // opposite of lru, use the one recently used, FIFO. cant evict fixcount != 0, so not that simple FIFO
    //printf("Mine: %d", table -> frames[first] -> timeUsed);
    for(i = 1; i < bm -> numPages; i++){
        //printf(" %d", table -> frames[i] -> timeUsed);
        if(table -> frames[i] -> fixCount != 0){ // cant evict if not fixcount 0 
            continue;
        }
        if(table -> frames[i] -> age < table -> frames[first] -> age){ // oldest, then evict, FIFO
            first = i;
        }
    }
    //printf("\n");

    // if is dirty, write
    if(table -> frames[first] -> dirtyFlag){
        writeBlock(table -> frames[first] -> page -> pageNum, &fh, table -> frames[first] -> page -> data);
        bm -> numWriteIO += 1;
    }

    // evict and write data
    free(table -> frames[first] -> page -> data);
    table -> frames[first] -> page -> data = page -> data;
    table -> frames[first] -> page -> pageNum = page -> pageNum;
    table -> frameContents[first] = page -> pageNum;
    table -> frames[first] -> fixCount = 1;
    table -> fixCounts[first] = 1;
    table -> frames[first] -> dirtyFlag = false;
    table -> dirtyFlags[first] = false;
    table -> frames[first] -> timeUsed = globalTime;
    table -> frames[first] -> age = globalTime;
    globalTime += 1;

    closePageFile(&fh);
    return RC_OK;
}

RC LRU(BM_BufferPool *const bm, BM_PageHandle *const page, SM_FileHandle fh){
    BM_PageTable *table = bm -> mgmtData;

    // check fixcounts array, if all non zero, cant do evict anyone
    int i;
    int avail = 0;
    for(i = 0; i < bm -> numPages; i++){
        if(table -> fixCounts[i] == 0){
            avail += 1;
        }
    }
    if(avail == 0){ // all frames are pinned, nothing can be evicted
        return -3;
    }


    int lru = 0;
    // iterate till find least recently used
    for(i = 1; i < bm -> numPages; i++){
        if(table -> frames[i] -> fixCount != 0){ // cant evict if not fixcount 0 
            continue;
        }
        if(table -> frames[i] -> timeUsed < table -> frames[lru] -> timeUsed){
            lru = i;
        }
    }

    // if lru is dirty, write
    if(table -> frames[lru] -> dirtyFlag){
        writeBlock(table -> frames[lru] -> page -> pageNum, &fh, table -> frames[lru] -> page -> data);
        bm -> numWriteIO += 1;
    }

    // evict and write data
    free(table -> frames[lru] -> page -> data);
    table -> frames[lru] -> page -> data = page -> data;
    table -> frames[lru] -> page -> pageNum = page -> pageNum;
    table -> frameContents[lru] = page -> pageNum;
    table -> frames[lru] -> fixCount = 1;
    table -> fixCounts[lru] = 1;
    table -> frames[lru] -> dirtyFlag = false;
    table -> dirtyFlags[lru] = false;
    table -> frames[lru] -> timeUsed = globalTime;
    table -> frames[lru] -> age = globalTime;
    globalTime += 1;

    closePageFile(&fh);
    return RC_OK;

}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum){

    BM_PageTable *table = bm -> mgmtData;
    int idx = getFrame(bm, pageNum);
    page -> pageNum = pageNum; // buffer mgr responsible to set pageNum field of the page handle passed to the method

    // dont care if not found page, if not found then add later on
    if(idx != -1){ // found alreadyin table
        table -> frames[idx] -> fixCount += 1;
        table -> fixCounts[idx] += 1;
        page -> data = table -> frames[idx] -> page -> data; // data field should point to the page frame
        table -> frameContents[idx] = pageNum;

        table -> frames[idx] -> timeUsed = globalTime;
        table -> frames[idx] -> age = globalTime;
        globalTime += 1;

        return RC_OK;
    }

    // not in buffer pool
    SM_FileHandle fh;
    if(openPageFile(bm -> pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    ensureCapacity(pageNum + 1, &fh);

    page -> data = malloc(PAGE_SIZE);
    if(readBlock(pageNum, &fh, page -> data) != RC_OK){
        free(page -> data);
        closePageFile(&fh);
        return -3;
    }
    bm -> numReadIO += 1;

    // if free space in table
    if (table -> numFramesUsed < bm -> numPages) {
        int i;
        for(i = 0; i < bm -> numPages; i++){
            if(table -> frames[i] == NULL){ // free space
                BM_PageFrame *frame = MAKE_PAGE_FRAME();
                frame -> page = MAKE_PAGE_HANDLE();
                frame -> page -> data = page -> data;
                frame -> fixCount = 1;
                table -> fixCounts[i] = 1;
                frame -> dirtyFlag = false;
                table -> dirtyFlags[i] = false;
                table -> frameContents[i] = pageNum;
                frame -> page -> pageNum = pageNum;

                frame -> timeUsed = globalTime;
                frame -> age = globalTime;
                globalTime += 1;

                table -> frames[i] = frame;
                table -> numFramesUsed += 1;

                closePageFile(&fh);
                return RC_OK;
            }
        }
    }


    // have to evict based on strategy
    if(bm -> strategy == RS_LRU){
        return LRU(bm, page, fh);
    }else if(bm -> strategy == RS_FIFO){
        return FIFO(bm, page, fh);
    }else{
        printf("Not implemented this strategy");
        return -3;
    }

    // somehow couldnt find a place so error
    closePageFile(&fh);
    return -3;
}

// statistics interface

PageNumber *getFrameContents (BM_BufferPool *const bm){
    BM_PageTable *table = bm->mgmtData;
    return table -> frameContents;
}

bool *getDirtyFlags (BM_BufferPool *const bm){
    BM_PageTable *table = bm->mgmtData;
    return table -> dirtyFlags;
}

int *getFixCounts (BM_BufferPool *const bm){
    BM_PageTable *table = bm->mgmtData;
    return table -> fixCounts;
}

int getNumReadIO (BM_BufferPool *const bm){
    return bm->numReadIO;
}

int getNumWriteIO (BM_BufferPool *const bm){
    return bm->numWriteIO;
}