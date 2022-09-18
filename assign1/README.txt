

File Related Methods:
    initStorageManager: Not really sure what this function is supposed to do. Just made it say "Initialized Storage Manager"

    createPageFile: Checks if file already exists, if it does returns -1. Don't want to overwrite an existing file. Initial file size is 1 page and filled with '\0' bytes.

    openPageFile: Opens existing page file. If couldn't open, throws RC_FILE_NOT_FOUND. Fills file handle with all the information. mgmtinfo has the file pointer. Number of pages is calculated using fseek and ftell.

    closePageFile: Close page file, and removes the fhandle. If couldn't close, throws RC_FILE_NOT_FOUND.

    destroyPageFile: Delete the file. Not given the fhandle, so can't delete fhandle associated with the file. Though, probably customary to do closePageFile then destroyPageFile. If couldn't remove, throws RC_FILE_NOT_FOUND.

Read and Write Methods:
    readBlock: First checks if the block at pageNum is possible to be read. If not, then throw RC_READ_NON_EXISTING_PAGE. Then checks if current page pos is same as pageNum if so, just start reading the block and store the content to memPage. If its not the same page, then seek to the start of pageNum block and then read. At the end, update the curPagePos, but incrementing 1.

    getBlockPos: Just return curPagePos from the file handle

    readFirstBlock, readLastBlock: Just uses the readBlock method, and pageNum is 0 or fHandle -> totalNumPages - 1. -1 cuz pageNum needs to start at 0

    readPreviousBlock, readCurrentBlock, readNextBlock: Just uses the readBlock method, but uses fHandle -> curPagePos or getBlockPos and then either add 0, 1, or minus 1

    writeBlock, writeCurrentBlock:

    appendEmptyBlock:

    ensureCapacity:

Not sure why "Hint: You should reserve some space in the beginning of a file to store information such as the total number of pages." when total number of pages is already in the file handle.

https://stackoverflow.com/questions/21327791/creating-a-file-with-with-size-one-page-and-fill-it-with-0-bytes
https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
https://www.geeksforgeeks.org/c-program-delete-file/

use "xxd -b file" to view bin files