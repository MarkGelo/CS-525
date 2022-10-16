## To run

do `make` and then `./test_assign2_1` to run the tests

All the given tests are passed. I didn't add any extra tests.

## Info

The buffer pool contains various variables to store valuable information as well as a page table storing all the frames with all the frame information. I added numWriteIO and numReadIO vars to buffer pool structure, for statistic purposes. I then made a new structure called PageFrame which has the page, dirtyFlag, fixCount, timeAccessed, and age which are needed to perform the replacement strategy. The page table includes an array of the page frames, total frames used, and arrays for statistic purposes and easy lookup such as frameContents, dirtyFlags, and fixCounts. Using these structures, the buffer pool management is complete.

**initBufferPool** - 
**shutdownBufferPool** - 
**forceFlushPool** - 

**markDirty** - 
**unpinPage** - 
**forcePage** - 
**pinPage** - 

**getFrameContents** - 
**getDirtyFlags** - 
**getFixCounts** - 
**getNumReadIO** - 
**getNumWriteIO** - 