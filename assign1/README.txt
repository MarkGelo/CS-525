To run, do "make"
Then "./test_assign1"
Can also do "make clean" to have a cleanslate

For this assignment, I mostly just followed the methods and what they're supposed to do, not really diverging from it. I added my own tests, that seems more exhaustive, making sure most, if not all, methods are correct and double checking the file handle information after the methods. My own test runs after the given tests.
An important thing, is that I didn't follow the hint of reserving some space in the beginning of a file to store info such as total number of pages. Since number of pages is already going to be stored in the file handle, i don't see the reason why it also needs to be in the file. It may be faster to just read it from file than calculating so, this may come bite me in the ass in later assignments. However, it should be pretty easy to implement having some space in the beggining of the file for misc info. 
I also checked for memory leaks and adjusted the storage manager as well as the tests to make sure there were none. One thing to note is that to have no memory leak, make sure to close all page files. Just destroying page files isn't enough, you also have to close all opened page files. Ex. calling closePageFile() and then destroyPageFile().
All the tests were passed when run on my VM and on fourier.

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

    writeBlock, writeCurrentBlock: Similar to readBlock methods, but instead, write. One thing to note was that, I made it so, it wouldn't increase the total number of pages. The description doesn't say anything about doing so, but when I think of writing a block, it first should already have the required amount of pages. So if writing on page greater than total number of pages, then it would throw an error. So outside of this function, if you want to writeblock greater than total number of pages, then do ensurecapacity first then write.

    appendEmptyBlock: Basically go to the end of file and just add an empty page, then update the file handle infos. 

    ensureCapacity: Calculate amount of pages needed, and just call appendEmptyBlock.

Not sure why "Hint: You should reserve some space in the beginning of a file to store information such as the total number of pages." when total number of pages is already in the file handle.

https://stackoverflow.com/questions/21327791/creating-a-file-with-with-size-one-page-and-fill-it-with-0-bytes
https://www.tutorialspoint.com/c_standard_library/c_function_fwrite.htm
https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
https://www.geeksforgeeks.org/c-program-delete-file/

use "xxd -b file" to view bin files