#include "storage_mgr.h"

#ifdef _WIN32
#  include <io.h>
#  define F_OK 0
#  define access _access
#else
#  include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#define HEAD_SIZE 32

#define HEAD_VERSION 8
#define HEAD_PAGE_SIZE 8
#define HEAD_PAGE_COUNT 4
#define HEAD_OFFSET 4

#define DEFAULT_MAJOR_VERSION 0
#define DEFAULT_MINOR_VERSION 1

#define VERIFY_HANDLER(HANDLER)          \
  if (!fHandle || !HANDLER->mgmtInfo)    \
  {                                      \
    LOG("[Warning] Invalid handler.\n"); \
    return RC_FILE_HANDLE_NOT_INIT;      \
  }

// HEAD 32 bytes
// +-------------+-----------+----------+--------+----------+
// | version     | page_size | page_num | offset | reserved |
// +-------------+-----------+----------+--------+----------+
// | 4 + 4 bytes | 4 bytes   | 4 bytes  | 4 bytes| 12 bytes |
// +-------------+-----------+----------+--------+----------+

char *zeroPage = NULL;

void initStorageManager(void) {}

RC createPageFile(char *fileName)
{
  if (access(fileName, F_OK) != -1)
  {
    LOG("[Warning] file already exists: ");
    LOG(fileName);
    LOG("\n");
  }

  FILE *file = fopen(fileName, "w+");
  if (!file)
  {
    LOG("[Error] failed to open file.\n");
    return RC_FILE_NOT_FOUND;
  }

  int res        = 1;
  char *zeroPage = (char *)calloc(1, sizeof(char) * (HEAD_SIZE + PAGE_SIZE));
  // clear and write head
  if (zeroPage)
  {
    ((int *)zeroPage)[0] = DEFAULT_MAJOR_VERSION;
    ((int *)zeroPage)[1] = DEFAULT_MINOR_VERSION;
    ((int *)zeroPage)[2] = PAGE_SIZE;
    ((int *)zeroPage)[3] = 1;
    ((int *)zeroPage)[4] = HEAD_SIZE;

    // write first page
    res = (fwrite(zeroPage, 1, (HEAD_SIZE + PAGE_SIZE), file) != (HEAD_SIZE + PAGE_SIZE));
  }

  if (fclose(file))
  {
    LOG("[Error] Failed to close file.\n");
  }
  free(zeroPage);
  zeroPage = NULL;
  return res ? RC_WRITE_FAILED : RC_OK;
}

RC openPageFile(char *fileName, SM_FileHandle *fHandle)
{
  if (!fHandle)
  {
    LOG("[Error] No handler.");
    return RC_FILE_HANDLE_NOT_INIT;
  }

  if (zeroPage)
  {
    free(zeroPage);
    zeroPage = NULL;
  }

  FILE *file = fopen(fileName, "r+");
  if (!file)
  {
    LOG("[Error] failed to open file.\n");
    return RC_FILE_NOT_FOUND;
  }

  //// read header
  //
  RC res     = RC_OK;
  char *head = NULL;
  do
  {
    head = (char *)calloc(1, sizeof(char) * HEAD_SIZE);
    if (!head)
    {
      LOG("[Error] Failed to open page file: Failed to allocate memory.\n");
      res = RC_WRITE_FAILED;
      break;
    }
    if (fread(head, HEAD_SIZE, 1, file) != 1)
    {
      LOG("[Error] Invalid head.\n");
      res = RC_WRITE_FAILED;
      break;
    }

    // file name
    fHandle->fileName = (char *)calloc(1, (strlen(fileName) + 1) * sizeof(char));
    memcpy(fHandle->fileName, fileName, strlen(fileName) + 1);

    // mgmtInfo
    fHandle->mgmtInfo = file;

    // page count
    fHandle->curPagePos = 0;
#ifdef USE_HEAD
    fHandle->totalNumPages = ((int *)head)[3];
#else
    int off = fseek(fHandle->mgmtInfo, 0, SEEK_END);
    if ((off != -1) && ((off = ftell(fHandle->mgmtInfo)) > 0))
    {
      fHandle->totalNumPages = (off - HEAD_SIZE) / PAGE_SIZE;
    }
    else
    {
      LOG("[Warning] Failed to get file size by fseek.\n");
      fHandle->totalNumPages = ((int *)head)[3];
    }
#endif

    // todo: version, page_size, offset
  } while (0);

  free(head);
  return res;
}

RC closePageFile(SM_FileHandle *fHandle)
{
  VERIFY_HANDLER(fHandle);

  free(fHandle->fileName);
  fHandle->fileName = NULL;

  // update header
  if (fseek(fHandle->mgmtInfo, 12, SEEK_SET) ||
      (fwrite(&fHandle->totalNumPages, sizeof(int), 1, fHandle->mgmtInfo) != 1))
  {
    LOG("[Error] Failed to update header.\n");
    return RC_WRITE_FAILED;
  }

  // close
  if (fclose(fHandle->mgmtInfo))
  {
    LOG("[Error] Failed to close file.\n");
  }

  fHandle->totalNumPages = -1;
  fHandle->curPagePos    = -1;

  return RC_OK;
}

RC destroyPageFile(char *fileName)
{
  if (remove(fileName))
  {
    LOG("[Warning] Failed to remove pagefile.\n");
    return RC_FILE_NOT_FOUND;
  }
  return RC_OK;
}

RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  if ((pageNum >= fHandle->totalNumPages) || (pageNum < 0))
  {
    LOG("[Error] Non existing page.\n");
    return RC_READ_NON_EXISTING_PAGE;
  }

  if (fseek(fHandle->mgmtInfo, HEAD_SIZE + PAGE_SIZE * pageNum, SEEK_SET) ||
      (fread(memPage, PAGE_SIZE, 1, fHandle->mgmtInfo) != 1))
  {
    LOG("[Error] Failed to read block.\n");
    return RC_READ_NON_EXISTING_PAGE;
  }
  fHandle->curPagePos = pageNum;

  return RC_OK;
}

int getBlockPos(SM_FileHandle *fHandle)
{
  if (!fHandle || !fHandle->mgmtInfo)
  {
    LOG("[Warning] Invalid handler.\n");
    return -1;
  }

  return fHandle->curPagePos;
}

RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return readBlock(0, fHandle, memPage);
}

RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return readBlock(fHandle->curPagePos - 1, fHandle, memPage);
}

RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return readBlock(fHandle->curPagePos, fHandle, memPage);
}

RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return readBlock(fHandle->curPagePos + 1, fHandle, memPage);
}

RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  if ((pageNum < 0) || (pageNum > fHandle->totalNumPages))
  {
    LOG("[Error] No such page.\n");
    return RC_READ_NON_EXISTING_PAGE;
  }

  if ((pageNum == fHandle->totalNumPages) && (appendEmptyBlock(fHandle) != RC_OK))
  {
    return RC_WRITE_FAILED;
  }

  fHandle->curPagePos = pageNum;

  if (fseek(fHandle->mgmtInfo, HEAD_SIZE + PAGE_SIZE * pageNum, SEEK_SET) ||
      (fwrite(memPage, PAGE_SIZE, 1, fHandle->mgmtInfo) != 1))
  {
    LOG("[Error] Failed to read block.\n");
    return RC_READ_NON_EXISTING_PAGE;
  }

  return RC_OK;
}

RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
  VERIFY_HANDLER(fHandle);

  return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

RC appendEmptyBlock(SM_FileHandle *fHandle)
{
  VERIFY_HANDLER(fHandle);
  if (fseek(fHandle->mgmtInfo, 0, SEEK_END))
  {
    LOG("[Error] Failed to reach end.");
    return RC_WRITE_FAILED;
  }

  int res        = 1;
  char *zeroPage = (char *)calloc(1, sizeof(char) * PAGE_SIZE);
  // clear and write first page
  if (zeroPage)
  {
    res = (fwrite(zeroPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE);
  }

  free(zeroPage);

  if (res)
  {  // succeed
    return RC_WRITE_FAILED;
  }
  ++fHandle->totalNumPages;
  return RC_OK;
}

RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
  VERIFY_HANDLER(fHandle);

  if (fHandle->totalNumPages >= numberOfPages)
  {
    return RC_OK;
  }

  RC res = RC_OK;
  for (int i = 0; i < numberOfPages - fHandle->totalNumPages; ++i)
  {
    if ((res = appendEmptyBlock(fHandle)) != RC_OK)
    {
      break;
    }
  }
  return res;
}