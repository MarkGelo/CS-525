#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "dberror.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"

// initialize page table with numPages frames, and also including framecontents, dirtyflags, fix counts arrays, as easability and so stats functions is already implemented
BM_PageTable *initPageTable(int numPages) {
    BM_PageTable *table = malloc(sizeof(BM_PageTable));
    table -> frames = malloc(sizeof(BM_PageFrame *) * numPages); // todo, just do malloc(sizeof(BM_PageFrame) * numPages) ?? 
    PageNumber *frameContents = malloc(sizeof(PageNumber) * numPages);
	bool *dirtyFlags = malloc(sizeof(bool) * numPages);
	int *fixCounts = malloc(sizeof(int) * numPages);

    int i;
    for(i = 0; i < numPages; i++) {
        table -> frames[i] = NULL; // frame should be empty
        frameContents[i] = -1; // since frame empty, shouldnt have anything in frame content
        dirtyFlags[i] = false;
        fixCounts[i] = 0;
    }
    // for stats and easability
    table -> frameContents = frameContents;
    table -> dirtyFlags = dirtyFlags;
    table -> fixCounts = fixCounts;

    table -> numFramesUsed = 0;
    table -> lastPinnedPos = -1;

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
        free(curFrame -> page -> data);
        free(curFrame -> page);
        free(curFrame);

    }
    free(table -> frames);
    free(table -> frameContents);
    free(table -> dirtyFlags);
    free(table -> fixCounts);
    free(table);

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

/*
 * Taking a buffer pool, page handle already initialized (i.e pageNum and data are correct) finds a frame in the buffer
 * pool to store the information.
 * The strategy used to find a place is FIFO
 */
RC fifoReplacement(BM_BufferPool *const bm, BM_PageHandle *const page, SM_FileHandle fh) {
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    for (int i = 1; i < bm->numPages; i++) { // i = 1 cuz want lastpinnedpos + 1, at the very least. If cant evict that one, then go to next one, +2, +3 and so on
        int position = (framesHandle->lastPinnedPos + i) % bm->numPages;
        BM_PageFrame *frame = framesHandle->frames[position];
        /* The frame can be evicted */
        if (frame->fixCount == 0) {
            if (frame->dirtyFlag == TRUE) {
                if (writeBlock(frame->page->pageNum, &fh, frame->page->data) != RC_OK)
                    return RC_WRITE_FAILED;
                bm->numWriteIO++;
            }
            free(frame->page->data);
            frame->page->data = page->data;
            frame->page->pageNum = page->pageNum;
            frame->fixCount = 1;
            frame->dirtyFlag = 0;
            time(&frame->timeUsed);
            frame->framePos = position;
            framesHandle->lastPinnedPos = position;
            closePageFile(&fh);
            return RC_OK;
        }
    }
    // CHANGE RETURN CODE
    return RC_WRITE_FAILED;
}

/*
 * Taking a buffer pool, page handle already initialized (i.e pageNum and data are correct) finds a frame in the buffer
 * pool to store the information.
 * The strategy used to find a place is LRU
 */
RC lruReplacement(BM_BufferPool *const bm, BM_PageHandle *const page, SM_FileHandle fh) {
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    BM_PageFrame *leastRecentlyUsedFrame = framesHandle->frames[0];
    for (int i =0; i < bm->numPages; i++) {
        BM_PageFrame *frame = framesHandle->frames[i];
        /* Searching the least recently used frame from the one that can be evicted (i.e fixCount = 0) */
        if (frame->fixCount == 0) {
            if (difftime(leastRecentlyUsedFrame->timeUsed, frame->timeUsed) >= 0) {
                leastRecentlyUsedFrame = frame;
            }
        }
    }

    /* Every frames are pinned at least once */
    if (leastRecentlyUsedFrame->fixCount != 0){
        // CHANGE RETURN CODE
        return RC_WRITE_FAILED;
    }

    if (leastRecentlyUsedFrame->dirtyFlag == TRUE) {
        if (writeBlock(leastRecentlyUsedFrame->page->pageNum, &fh, leastRecentlyUsedFrame->page->data) != RC_OK)
            return RC_WRITE_FAILED;
        bm->numWriteIO++;
    }
    struct timeval tv;
    free(leastRecentlyUsedFrame->page->data);

    leastRecentlyUsedFrame->page->data = page->data;
    leastRecentlyUsedFrame->page->pageNum = page->pageNum;
    leastRecentlyUsedFrame->fixCount = 1;
    leastRecentlyUsedFrame->dirtyFlag = 0;
    gettimeofday(&tv, NULL);
    leastRecentlyUsedFrame->timeUsed = tv.tv_usec;

    framesHandle->lastPinnedPos = leastRecentlyUsedFrame->framePos;
    closePageFile(&fh);
    return RC_OK;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum){

    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    BM_PageFrame *foundFrame = findFrameNumberN(bm, pageNum);
    struct timeval tv;

    /* We found the page in the buffer */
    if (foundFrame != NULL) {
        page->data = foundFrame->page->data;
        page->pageNum = pageNum;
        foundFrame->fixCount++;
        gettimeofday(&tv, NULL);
        foundFrame->timeUsed = tv.tv_usec;
        framesHandle->lastPinnedPos = foundFrame->framePos;
        return RC_OK;
    }

    /* Page is not in buffer, we will need to storage manager to get it from the disk */
    char *filename = (char *) bm->pageFile;
    SM_FileHandle fh;
    if (openPageFile(filename, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    ensureCapacity(pageNum + 1, &fh); // +1 because pages are numbered started from 0

    page->data = malloc(PAGE_SIZE);


    RC read = readBlock(pageNum, &fh, page->data);
    if (read != RC_OK) {
        free(page->data);
        closePageFile(&fh);
        return read;
    }
    bm->numReadIO++;

    page->pageNum = pageNum;

    /* Looking if we still have place in the frames */
    if (framesHandle->numFramesUsed < bm->numPages) {
        int availablePosition = framesHandle->lastPinnedPos + 1;

        BM_PageFrame *frame = malloc(sizeof(BM_PageFrame));
        frame->page = malloc(sizeof(BM_PageHandle));

        frame->page->data = page->data;
        frame->fixCount = 1;
        frame->dirtyFlag = FALSE;
        frame->framePos = availablePosition;
        frame->page->pageNum = pageNum;
        gettimeofday(&tv, NULL);
        frame->timeUsed = tv.tv_usec;
        framesHandle->frames[availablePosition] = frame;
        framesHandle->numFramesUsed++;
        framesHandle->lastPinnedPos = availablePosition;
        closePageFile(&fh);
        return RC_OK;
    }

    /*
    struct timeval tv; // DEL?

    BM_PageTable *table = bm -> mgmtData;
    int idx = getFrame(bm, page -> pageNum);
    // dont care if not found page, if not found then add later on
    if(idx != -1){ // found alreadyin table
        table -> lastPinnedPos = table -> frames[idx] -> framePos;
        table -> frames[idx] -> fixCount += 1;
        table -> fixCounts[idx] += 1;
        page -> data = table -> frames[idx] -> page -> data; // data field should point to the page frame
        page -> pageNum = pageNum; // ?

        gettimeofday(&tv, NULL);
        table -> frames[idx] -> timeUsed = tv.tv_usec;

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
    page -> pageNum = pageNum;

    // if free space in table
    if(table -> numFramesUsed < bm -> numPages){
        // iterate until find free spot
        int i;
        for(i = 0; i < bm -> numPages; i++){
            if(table -> frames[i] == NULL){
                BM_PageFrame *frame = malloc(sizeof(BM_PageFrame));
                frame -> page = malloc(sizeof(BM_PageHandle));
                frame -> page -> data = page -> data;
                frame -> fixCount = 1;
                table -> fixCounts[idx] = 1;
                frame -> dirtyFlag = false;
                table -> dirtyFlags[idx] = false;
                frame -> framePos = i; // delet
                frame -> page -> pageNum = pageNum;

                gettimeofday(&tv, NULL);
                frame -> timeUsed = tv.tv_usec;

                table -> frames[i] = frame;
                table -> numFramesUsed += 1;
                table -> lastPinnedPos = i;

                closePageFile(&fh);
                return RC_OK;
            }
        }
    }
    */

    /* If we don't have any place */

    switch (bm->strategy) {
        case RS_FIFO:
            return fifoReplacement(bm, page, fh);
        case RS_CLOCK:
            break;
        case RS_LRU:
            return lruReplacement(bm, page, fh);
        case RS_LFU:
            break;
        case RS_LRU_K:
            break;
        default:
            // CHANGE RETURN CODE
            return RC_WRITE_FAILED;
    }


    /*We didn't find any evicable page */
    // CHANGE RETURN CODE
    closePageFile(&fh);
    return RC_WRITE_FAILED;


}

// Statistics Interface

/* Results need to be freed after use */
PageNumber *getFrameContents(BM_BufferPool *const bm) {
    BM_PageTable *frames = bm->mgmtData;
    PageNumber *arrayOfPageNumber = malloc(sizeof(PageNumber) * bm->numPages);
    for (int i = 0; i < bm->numPages; i++) {
        BM_PageFrame *frame = frames->frames[i];
        if (frame != NULL)
            arrayOfPageNumber[i] = frame->page->pageNum;
        else
            arrayOfPageNumber[i] = NO_PAGE;
    }
    return arrayOfPageNumber;


    // remove the above and just do the 2 liner below if you initialize it correctly, and update during stuff ... Similar to how mickeytheone does it
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    return framesHandle -> frameContents;
}
/* Results need to be freed after use */
bool *getDirtyFlags(BM_BufferPool *const bm) {
    bool *array = malloc(sizeof(bool) * bm->numPages);
    BM_PageTable *frames = bm->mgmtData;
    for (int i = 0; i < bm->numPages; i++) {
        BM_PageFrame *frame = frames->frames[i];
        if (frame != NULL) {
            array[i] = frame->dirtyFlag;
        } else
            array[i] = FALSE;
    }
    return array;


    // remove the above and just do the 2 liner below if you initialize it correctly, and update during stuff ... Similar to how mickeytheone does it
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    return framesHandle -> dirtyFlags;
}
/* Results need to be freed after use */
int *getFixCounts(BM_BufferPool *const bm) {
    int *array = malloc(sizeof(int) * bm->numPages);
    BM_PageTable *frames = bm->mgmtData;
    for (int i = 0; i < bm->numPages; i++) {
        BM_PageFrame *frame = frames->frames[i];
        if (frame != NULL) {
            array[i] = frame->fixCount;
        } else
            array[i] = 0;
    }
    return array;


    // remove the above and just do the 2 liner below if you initialize it correctly, and update during stuff ... Similar to how mickeytheone does it
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    return framesHandle -> fixCounts;
}

int getNumReadIO(BM_BufferPool *const bm) {
    return bm->numReadIO;
}

int getNumWriteIO(BM_BufferPool *const bm) {
    return bm->numWriteIO;
}