#ifndef _BATCH_DATA_H
#define _BATCH_DATA_H

#include "MCU.h"
#include "VTS.h"
#include <string.h>

#define INDEX_ADDRESS 0x00000000
#define MAX_FILE 50
#define FILE_START_ADDRESS 1024
#define FILE_SIZE 256
#define MAX_VALID_PACKET_TYPE 4

#define BATCH_INDEX_FILE "BatchIndex.bin"
#define BATCH_DATA_FILE "BatchFile.bin"

#define BATCH_TABLE_MAGIC 0xBA02

typedef struct
{
	uint8_t valid;
	uint8_t packet;
} FInfoTypedef;

typedef struct
{
	uint16_t magic;
	uint16_t TotalFiles;
	uint16_t head;
	uint16_t tail;
	FInfoTypedef fInfo[MAX_FILE];
} FTableTypedef;

extern FTableTypedef FTable;

void StoreFileToFlash(char *data, uint8_t paket);
uint16_t ReadDataBatch(char *data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete);
uint16_t ReadDataBatchExt(char *data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete, int16_t *outSlot);
uint8_t DeleteDataBatchSlot(uint16_t pos);
uint16_t ReadFileTable(void);
void ClearFileTable(void);
// Clears the batch queue files without formatting UFS.
uint8_t ClearBatchStorage(uint16_t *deletedCount, uint16_t *failedCount);
uint16_t GetTotalFiles(void);

#endif
