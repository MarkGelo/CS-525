#include "dberror.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>


/*
 * Create an empty frame container with numberOfFrames frames
 * The result need to be freed before the end of the program
 */
BM_PageTable *createFrames(int numberOfFrames) {
    BM_PageTable *frames = malloc(sizeof(BM_PageTable));
    frames->frames = malloc(sizeof(BM_PageFrame *) * numberOfFrames);
    for (int i = 0; i < numberOfFrames; i++) {
        frames->frames[i] = NULL;
    }
    frames->numFramesUsed = 0;
    frames->lastPinnedPos = -1;
    return frames;
}


/*
 * Loop over the frames in order to find which one contains the page number pageNum and returns it
 * If not found returns NULL
 * This could be improved by using a hash table.
 */
BM_PageFrame *findFrameNumberN(BM_BufferPool *const bm, const PageNumber pageNum) {
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    for (int i = 0; i < bm->numPages; i++) {
        if (framesHandle->frames[i] != NULL) {
            if (framesHandle->frames[i]->page->pageNum == pageNum) {
                return framesHandle->frames[i];
            }
        }
    }
    return (BM_PageFrame *) NULL;
}

/*
 * Taking a buffer pool, page handle already initialized (i.e pageNum and data are correct) finds a frame in the buffer
 * pool to store the information.
 * The strategy used to find a place is FIFO
 */
RC fifoReplacement(BM_BufferPool *const bm, BM_PageHandle *const page, SM_FileHandle fh) {
    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    for (int i = 1; i < bm->numPages; i++) {
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

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData) {
    // CHECK IF FILE EXISTS
    if (access(pageFileName, F_OK) == 0) {
        // file exists
        initStorageManager();
        bm->pageFile = (char *) pageFileName;
        bm->numPages = numPages;
        bm->mgmtData = createFrames(numPages);
        bm->strategy = strategy;
        bm->numReadIO = 0;
        bm->numWriteIO = 0;

        return RC_OK;
    }
    return RC_FILE_NOT_FOUND;
}

RC shutdownBufferPool(BM_BufferPool *const bm) {
    char *filename = (char *) bm->pageFile;
    SM_FileHandle fh;
    if (openPageFile(filename, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    BM_PageTable *frames = bm->mgmtData;
    for (int i = 0; i < bm->numPages; i++) {
        BM_PageFrame *frame = frames->frames[i];
        if (frame != NULL) {
            if (frame->fixCount != 0) {
                //CHANGE RETURN CODE
                return RC_WRITE_FAILED;
            }
            if (frame->dirtyFlag == TRUE) {
                writeBlock(frame->page->pageNum, &fh, frame->page->data);
            }
            free(frame->page->data);
            free(frame->page);
            free(frame);
        }
    }
    free(frames->frames);
    free(frames);
    closePageFile(&fh);
    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm) {
    char *filename = (char *) bm->pageFile;
    SM_FileHandle fh;
    if (openPageFile(filename, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    BM_PageTable *frames = bm->mgmtData;
    for (int i = 0; i < bm->numPages; i++) {
        BM_PageFrame *frame = frames->frames[i];
        if (frame != NULL) {
            if (frame->dirtyFlag == TRUE) {
                writeBlock(frame->page->pageNum, &fh, frame->page->data);
                frame->dirtyFlag = FALSE;
                bm->numWriteIO++;
            }
        }
    }
    closePageFile(&fh);
    return RC_OK;
}

// Buffer Manager Interface Access Pages
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_PageFrame *foundFrame = findFrameNumberN(bm, page->pageNum);
    if (foundFrame != NULL) {
        foundFrame->dirtyFlag = TRUE;
        return RC_OK;
    }

    // CHANGE RETURNED CODE
    return RC_WRITE_FAILED;
}

RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_PageFrame *foundFrame = findFrameNumberN(bm, page->pageNum);
    if (foundFrame != NULL) {
        foundFrame->fixCount--;
        return RC_OK;
    }

    // CHANGE RETURNED CODE
    return RC_WRITE_FAILED;
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_PageFrame *foundFrame = findFrameNumberN(bm, page->pageNum);
    if (foundFrame != NULL) {

        char *filename = (char *) bm->pageFile;
        SM_FileHandle fh;
        if (openPageFile(filename, &fh) != RC_OK) {
            return RC_FILE_NOT_FOUND;
        }

        if (writeBlock(page->pageNum, &fh, foundFrame->page->data) != RC_OK) {
            return RC_WRITE_FAILED;
        }
        bm->numWriteIO++;
        foundFrame->dirtyFlag = 0;
        closePageFile(&fh);
        return RC_OK;
    }

    // CHANGE RETURNED CODE
    return RC_WRITE_FAILED;
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {

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



    BM_PageTable *framesHandle = (BM_PageTable *) bm->mgmtData;
    return framesHandle -> fixCounts;
}

int getNumReadIO(BM_BufferPool *const bm) {
    return bm->numReadIO;
}

int getNumWriteIO(BM_BufferPool *const bm) {
    return bm->numWriteIO;
}