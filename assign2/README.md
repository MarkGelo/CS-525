## To run

do `make` and then `./test_assign2_1` to run the tests

All the given tests are passed. I didn't add any extra tests.

## Info

The buffer pool contains various variables to store valuable information as well as a page table storing all the frames with all the frame information. I added numWriteIO and numReadIO vars to buffer pool structure, for statistic purposes. I then made a new structure called PageFrame which has the page, dirtyFlag, fixCount, timeAccessed, and age which are needed to perform the replacement strategy. The page table includes an array of the page frames, total frames used, and arrays for statistic purposes and easy lookup such as frameContents, dirtyFlags, and fixCounts. Using these structures, the buffer pool management is complete. In terms of replacement strategies, I have only implemented FIFO and LRU.

**initBufferPool** - Just Initializes the buffer pool with numPages page frames. Makes sure file exists and if so, readies the buffer pool by creating a page table with all the required frames, specified by the numPages \
**shutdownBufferPool** - Shut downs the buffer pool but makes sure all the dirty pages are written back first. If there are any pinned pages, then it errors out, as it shouldn't shutdown since its pinned. Currently, I have it just using the frames itself, but I could also use the dirtyFlags and fixCounts array instead, which may be more efficient in terms of memory usage? \
**forceFlushPool** - All the dirty pages are written back. Iterates over all the frames in the page table and checks if its dirty. If it is dirty then it is written back and the dirty variable for that frame is now false. \
\
**markDirty** - Marks a page dirty. I use a separate function to find the frame with the specified pageNumber using frameContents array, which maybe more efficient than just iterating over all the frames. Also updates the dirtyFlags array. \
**unpinPage** - Same thing as markDirty but unpins a page. I did doublecheck for a unique case. If a user is trying to unpin a page with a fixCount 0, i just decided to print a statement saying so, but ultimately it does nothing, since no need to decrement the count. If I didnt check for this case, then fixCount could have been negative, since unpinning is usually decrementing the fixCount. \
**forcePage** - Finds the frame using the pageNumber and then opens the file and write back. Updates numWriteIO and the dirtyFlag/s. \
**pinPage** - First I check if the page is already on the frame, if so just pin it, incrementing fixCount, frameContents, timeAccessed, age, and also the data field is now pointed to the page frame. If it is not in the buffer pool, then I open the file, and read the data and then check the page table, also incrementing numReadIO. I check if there is space in the page table using frameContents, and if there is then just place the page in the free space. If there is no space in the page table, then I have to choose which frame to evict and for this part, I have only implemented FIFO and LRU strategies. FIFO is done using the age variable for each frame, in which case the oldest page is prioritized for eviction. LRU, is least recently used, and I use the timeAccessed variable, to determine which was least recently used and evict that. In these cases, it first ignores any frames that doesnt have a fixCount of 0, and if the frame to be evicted is dirty, then writes back first and then evict and replace. \
\
**getFrameContents** - Returns an array of PageNumber which correspond to the pageNumber of the page stored in the page table. Dont have to iterate over the page table for this as I keep a separate array in the structure for this, for easy lookup. \
**getDirtyFlags** - Returns an array of bool which correspond to the dirtyFlag of the page stored in the page table. Dont have to iterate over the page table for this as I keep a separate array in the structure for this, for easy lookup. \
**getFixCounts** - Returns an array of ints which correspond to the fixcount of the page stored in the page table. Dont have to iterate over the page table for this as I keep a separate array in the structure for this, for easy lookup. \
**getNumReadIO** - Just returns the number of pages read from disk to the page file since initialization. Easily done because I added a variable to the buffer pool structure to keep track of this. \
**getNumWriteIO** - Just returns the number of pages written to the page file since initialization. Easily done because I added a variable to the buffer pool structure to keep track of this. \