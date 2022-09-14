

Functions:
    initStorageManager: Not really sure what this function is supposed to do. Just made it say "Initialized Storage Manager"

    createPageFile: Checks if file already exists, if it does returns -1. Don't want to overwrite an existing file. Initial file size is 1 page and filled with '\0' bytes.

    openPageFile: Opens existing page file. If couldn't open, throws RC_FILE_NOT_FOUND. Fills file handle with all the information. mgmtinfo has the file pointer. Number of pages is calculated using fseek and ftell.

    closePageFile: Close page file, and removes the fhandle. If couldn't close, throws RC_FILE_NOT_FOUND.

    destroyPageFile: Delete the file. Not given the fhandle, so can't delete fhandle associated with the file. Though, probably customary to do closePageFile then destroyPageFile. If couldn't remove, throws RC_FILE_NOT_FOUND.


Not sure why "Hint: You should reserve some space in the beginning of a file to store information such as the total number of pages." when total number of pages is already in the file handle.

https://stackoverflow.com/questions/21327791/creating-a-file-with-with-size-one-page-and-fill-it-with-0-bytes
https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
