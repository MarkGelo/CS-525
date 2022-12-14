#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "test_helper.h"

// test name
char *testName;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testSinglePageContent(void);
static void myOwnTest(void);

/* main function running all tests */
int
main (void)
{
  testName = "";
  
  initStorageManager();

  testCreateOpenClose();
  testSinglePageContent();
  myOwnTest();

  return 0;
}

void myOwnTest(void){
  printf("\n------------------my own test-------------------\n");

  SM_FileHandle fh;
  SM_FileHandle fh2;
  SM_PageHandle ph;
  ph = (SM_PageHandle) malloc(PAGE_SIZE);
  int i;
  for (i=0; i < PAGE_SIZE; i++)
    ph[i] = (i % 11) + '0';
  SM_PageHandle ph2;
  ph2 = (SM_PageHandle) malloc(PAGE_SIZE);
  for (i=0; i < PAGE_SIZE; i++)
    ph2[i] = (i % 22) + '0';

  TEST_CHECK(createPageFile ("myowntest.bin"));
  TEST_CHECK(createPageFile ("myowntest2.bin"));
  TEST_CHECK(openPageFile ("myowntest.bin", &fh));
  TEST_CHECK(openPageFile ("myowntest2.bin", &fh2));
  //printf("\nhere\n");

  ASSERT_TRUE((createPageFile ("myowntest.bin") != RC_OK), "fails due to trying to create page file when theres already a file with same name");
  ASSERT_TRUE((writeBlock (1, &fh, ph) != RC_OK), "fails due to trying to write block when theres not enough space in page");
  //printf("\nhere1\n");

  //printf("Total Num Pages: %d\n", fh.totalNumPages);
  TEST_CHECK(ensureCapacity(3, &fh));
  //printf("Total Num Pages: %d\n", fh.totalNumPages);
  //printf("Cur Page Pos: %d\n", fh.curPagePos);
  ASSERT_TRUE((fh.totalNumPages == 3), "expect 3 page in file after ensuring capacity");
  ASSERT_TRUE((fh.curPagePos == 3), "after ensure capacity position should be at end, so 3");
  TEST_CHECK(writeBlock (1, &fh, ph));
  TEST_CHECK(writeBlock (2, &fh, ph2));
  //printf("\nhere2\n");

  TEST_CHECK(ensureCapacity(4, &fh2));
  TEST_CHECK(writeBlock (0, &fh2, ph));
  TEST_CHECK(writeBlock (3, &fh2, ph2));
  ASSERT_TRUE((fh2.curPagePos == 4), "after writing at block 3, cur page should be at 4");
  ASSERT_TRUE((getBlockPos(&fh2) == 4), "block pos should be at 4");
  ASSERT_TRUE((writeCurrentBlock (&fh2, ph) != RC_OK), "writing at current block, 4, should be error, since only 4 pages");
  //printf("\nhere3\n");

  SM_PageHandle ph3;
  ph3 = (SM_PageHandle) malloc(PAGE_SIZE);

  TEST_CHECK(readPreviousBlock(&fh2, ph3)); // should be equal to ph2
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph3[i] == ph2[i]), "character in page read from disk is the one we expected.");
  ASSERT_TRUE((getBlockPos(&fh2) == 4), "block pos should be at 4");
  TEST_CHECK(readBlock(1, &fh, ph3)); // should be equal to ph
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph3[i] == ph[i]), "character in page read from disk is the one we expected.");
  //printf("\nhere4\n");

  TEST_CHECK(closePageFile(&fh));
  TEST_CHECK(closePageFile(&fh2));
  TEST_CHECK(destroyPageFile("myowntest.bin"));
  TEST_CHECK(destroyPageFile("myowntest2.bin"));
  //printf("\nhere5\n");

  free(ph);
  free(ph2);
  free(ph3);
  TEST_DONE();
}

/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void
testCreateOpenClose(void)
{
  SM_FileHandle fh;

  testName = "test create open and close methods";

  TEST_CHECK(createPageFile (TESTPF));
  
  TEST_CHECK(openPageFile (TESTPF, &fh));
  ASSERT_TRUE(strcmp(fh.fileName, TESTPF) == 0, "filename correct");
  ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
  ASSERT_TRUE((fh.curPagePos == 0), "freshly opened file's page position should be 0");

  TEST_CHECK(closePageFile (&fh));
  TEST_CHECK(destroyPageFile (TESTPF));

  // after destruction trying to open the file should cause an error
  ASSERT_TRUE((openPageFile(TESTPF, &fh) != RC_OK), "opening non-existing file should return an error.");

  TEST_DONE();
}

/* Try to create, open, and close a page file */
void
testSinglePageContent(void)
{
  SM_FileHandle fh;
  SM_PageHandle ph;
  int i;

  testName = "test single page content";

  ph = (SM_PageHandle) malloc(PAGE_SIZE);

  // create a new page file
  TEST_CHECK(createPageFile (TESTPF));
  TEST_CHECK(openPageFile (TESTPF, &fh));
  printf("created and opened file\n");
  
  // read first page into handle
  TEST_CHECK(readFirstBlock (&fh, ph));
  // the page should be empty (zero bytes)
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  printf("first block was empty\n");
    
  // change ph to be a string and write that one to disk
  for (i=0; i < PAGE_SIZE; i++)
    ph[i] = (i % 10) + '0';
  TEST_CHECK(writeBlock (0, &fh, ph));
  printf("writing first block\n");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readFirstBlock (&fh, ph));
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character in page read from disk is the one we expected.");
  printf("reading first block\n");

  TEST_CHECK(closePageFile (&fh)); // need this cuz non freed block?
  // destroy new page file
  TEST_CHECK(destroyPageFile (TESTPF));  
  
  free(ph);
  TEST_DONE();
}
