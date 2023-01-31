#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <io.h>
#  define F_OK 0
#  define access _access
#else
#  include <unistd.h>
#endif

#include "dberror.h"
#include "storage_mgr.h"
#include "test_helper.h"

// test name
char *testName;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testSinglePageContent(void);
static void testMultiplePageContent(void);

/* main function running all tests */
int main(void)
{
  testName = "";

  initStorageManager();

  testCreateOpenClose();
  testSinglePageContent();
  testMultiplePageContent();

  return 0;
}

/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void testCreateOpenClose(void)
{
  SM_FileHandle fh;

  testName = "test create open and close methods";

  TEST_CHECK(createPageFile(TESTPF));

  TEST_CHECK(openPageFile(TESTPF, &fh));
  ASSERT_TRUE(strcmp(fh.fileName, TESTPF) == 0, "filename correct");
  ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
  ASSERT_TRUE((fh.curPagePos == 0), "freshly opened file's page position should be 0");

  TEST_CHECK(closePageFile(&fh));
  TEST_CHECK(destroyPageFile(TESTPF));

  // after destruction trying to open the file should cause an error
  ASSERT_TRUE((openPageFile(TESTPF, &fh) != RC_OK),
              "opening non-existing file should return an error.");

  TEST_DONE();
}

/* Try to create, open, and close a page file */
void testSinglePageContent(void)
{
  SM_FileHandle fh;
  SM_PageHandle ph;
  int i;

  testName = "test single page content";

  // allocate memory for a page
  ph = (SM_PageHandle)malloc(PAGE_SIZE);

  // create a new page file
  TEST_CHECK(createPageFile(TESTPF));
  TEST_CHECK(openPageFile(TESTPF, &fh));
  printf("created and opened file\n");

  // read first page into handle
  TEST_CHECK(readFirstBlock(&fh, ph));
  // the page should be empty (zero bytes)
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  printf("first block was empty\n");

  // change ph to be a string and write that one to disk
  for (i = 0; i < PAGE_SIZE; i++)
    ph[i] = (i % 10) + '0';
  TEST_CHECK(writeBlock(0, &fh, ph));
  printf("writing first block\n");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readFirstBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'),
                "character in page read from disk is the one we expected.");
  printf("reading first block\n");

  // destroy new page file
  TEST_CHECK(closePageFile(&fh));
  TEST_CHECK(destroyPageFile(TESTPF));

  // free page memory
  free(ph);

  TEST_DONE();
}

void testMultiplePageContent(void)
{
  SM_FileHandle fh;
  SM_PageHandle ph;
  int i;

  testName = "test single page content";

  // allocate memory for a page
  ph = (SM_PageHandle)malloc(PAGE_SIZE);

  // create a new page file
  TEST_CHECK(createPageFile(TESTPF));
  TEST_CHECK(openPageFile(TESTPF, &fh));
  printf("created and opened file\n");

  // read first page into handle
  TEST_CHECK(readFirstBlock(&fh, ph));
  // the page should be empty (zero bytes)
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  printf("first block was empty\n");

  // first block: digits
  for (i = 0; i < PAGE_SIZE; i++)
    ph[i] = (i % 10) + '0';
  TEST_CHECK(writeBlock(0, &fh, ph));

  printf("writing first block\n");
  TEST_CHECK(readBlock(0, &fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'),
                "character in page read from disk is the one we expected.");
  printf("reading first block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 0), "right block position");

  // second block: digits
  TEST_CHECK(writeBlock(1, &fh, ph));

  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'),
                "character in page read from disk is the one we expected.");
  printf("reading second block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 1), "right block position");

  // second block: letters
  for (i = 0; i < PAGE_SIZE; i++)
    ph[i] = (i % 26) + 'a';
  TEST_CHECK(writeCurrentBlock(&fh, ph));
  printf("writing second block\n");

  TEST_CHECK(readLastBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 26) + 'a'),
                "character in page read from disk is the one we expected.");
  printf("reading second block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 1), "right block position");

  // extend to 4 blocks
  TEST_CHECK(ensureCapacity(4, &fh));

  // fourth block: capitals
  for (i = 0; i < PAGE_SIZE; i++)
    ph[i] = (i % 26) + 'A';
  TEST_CHECK(writeBlock(3, &fh, ph));
  printf("writing 4-th block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 3), "right block position");

  // read 3rd
  TEST_CHECK(readPreviousBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == '\0'), "character in page read from disk is the one we expected.");
  printf("reading 3rd block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 2), "right block position");

  // read 2nd
  TEST_CHECK(readPreviousBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 26) + 'a'),
                "character in page read from disk is the one we expected.");
  printf("reading 3rd block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 1), "right block position");

  // read last
  TEST_CHECK(readLastBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 26) + 'A'),
                "character in page read from disk is the one we expected.");
  printf("reading 4th block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 3), "right block position");

  // read first
  TEST_CHECK(readFirstBlock(&fh, ph));
  for (i = 0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'),
                "character in page read from disk is the one we expected.");
  printf("reading 1st block\n");
  ASSERT_TRUE((getBlockPos(&fh) == 0), "right block position");

  TEST_CHECK(ensureCapacity(4, &fh));

  // destroy new page file
  TEST_CHECK(closePageFile(&fh));

  TEST_CHECK(openPageFile(TESTPF, &fh));
  printf("created and opened file\n");
  TEST_CHECK(closePageFile(&fh));

  TEST_CHECK(destroyPageFile(TESTPF));

  // free page memory
  free(ph);

  TEST_DONE();
}