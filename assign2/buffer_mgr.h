#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

// Include return codes and methods for logging errors
#include "dberror.h"
#include "storage_mgr.h"

// Include bool DT
#include "dt.h"

// Replacement Strategies
typedef enum ReplacementStrategy {
	RS_FIFO = 0,
	RS_LRU = 1,
	RS_CLOCK = 2,
	RS_LFU = 3,
	RS_LRU_K = 4
} ReplacementStrategy;

// Data Types and Structures
typedef int PageNumber;
#define NO_PAGE -1

typedef struct BM_BufferPool {
	//SM_FileHandle *fh; // added this, so dont have to open a lot during functions? more efficient? or not
	char *pageFile;
	int numPages;
	ReplacementStrategy strategy;
	void *mgmtData; // use this one to store the bookkeeping info your buffer
	// for stats functions
    int numWriteIO;
    int numReadIO;
	// manager needs for a buffer pool
} BM_BufferPool;

typedef struct BM_PageHandle {
	PageNumber pageNum;
	char *data;
} BM_PageHandle;

typedef struct BM_PageFrame {
    BM_PageHandle *page;
    bool dirtyFlag;
    int fixCount;
	int age; // FIFO
    int timeAccessed; // LRU
} BM_PageFrame;

typedef struct BM_PageTable {
    BM_PageFrame **frames;
	// arrays, hashtable for easy lookups and for stats functions
	PageNumber *frameContents; //array of PageNumbers
	bool *dirtyFlags; // array for stats
	int *fixCounts; // array
	int numFramesUsed;
} BM_PageTable;

// convenience macros
#define MAKE_POOL()						\
		((BM_BufferPool *) malloc (sizeof(BM_BufferPool)))

#define MAKE_PAGE_HANDLE()				\
		((BM_PageHandle *) malloc (sizeof(BM_PageHandle)))

#define MAKE_PAGE_FRAME()				\
		((BM_PageFrame *) malloc (sizeof(BM_PageFrame)))

#define MAKE_PAGE_TABLE()				\
		((BM_PageTable *) malloc (sizeof(BM_PageTable)))

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy,
		void *stratData);
RC shutdownBufferPool(BM_BufferPool *const bm);
RC forceFlushPool(BM_BufferPool *const bm);

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page);
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page);
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page);
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum);

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm);
bool *getDirtyFlags (BM_BufferPool *const bm);
int *getFixCounts (BM_BufferPool *const bm);
int getNumReadIO (BM_BufferPool *const bm);
int getNumWriteIO (BM_BufferPool *const bm);

#endif
