#include "Batch.h"

FTableTypedef FTable = {0};

// CDAC (CD1) uses flash-file store-and-forward (BTH batching + HttpQueue eviction),
// so its Batch.c primitives must be built even though HISTORY_DISABLED is set
// globally to strip the legacy RAM-history path for the other protocols.
#if !defined(HISTORY_DISABLED) || defined(PROTO_CDAC)

static uint32_t SlotToAddress(uint16_t slot)
{
	return ((uint32_t)slot * FILE_SIZE) + FILE_START_ADDRESS;
}

void UpdateFileTable(void)
{
	int fd;
	u32 bytesWritten = 0;
	s32 ret;

	LOGData(TAG_BATCH, "Updating Batch Table (head=%d, tail=%d, count=%d)",
		FTable.head, FTable.tail, FTable.TotalFiles);

	fd = Ql_FS_Open(BATCH_INDEX_FILE, QL_FS_CREATE_ALWAYS);
	if(fd < 0)
	{
		LOGData(TAG_BATCH, "Unable to create Batch Table File, err: %d", fd);
		ret = Ql_FS_Format(Ql_FS_UFS);
		if(ret != QL_RET_OK)
		{
			LOGData(TAG_BATCH, "Format UFS failed, ret: %d", ret);
			return;
		}

		fd = Ql_FS_Open(BATCH_INDEX_FILE, QL_FS_CREATE_ALWAYS);
		if(fd < 0)
		{
			LOGData(TAG_BATCH, "Batch table create failed again, err: %d", fd);
			return;
		}
	}

	ret = Ql_FS_Write(fd, (void *)&FTable, sizeof(FTable), &bytesWritten);
	Ql_FS_Close(fd);
	if(ret != QL_RET_OK || bytesWritten != sizeof(FTable))
	{
		LOGData(TAG_BATCH, "Batch table write error, ret=%d written=%d", ret, bytesWritten);
	}
}

uint16_t ReadFileTable(void)
{
	int fd;
	u32 bytesRead = 0;
	s32 fileSize;
	s32 ret;

	LOGVerbose(TAG_BATCH, "Reading Batch Table...");
	if(Ql_FS_Check(BATCH_INDEX_FILE) != QL_RET_OK)
		goto INIT_NEW;

	fileSize = Ql_FS_GetSize(BATCH_INDEX_FILE);
	if(fileSize != sizeof(FTable))
	{
		LOGData(TAG_BATCH, "Invalid Batch Index size %d (expected %d)", fileSize, sizeof(FTable));
		goto INIT_NEW;
	}

	fd = Ql_FS_Open(BATCH_INDEX_FILE, QL_FS_READ_ONLY);
	if(fd < 0)
	{
		LOGData(TAG_BATCH, "Unable to open Batch Index File, err: %d", fd);
		goto INIT_NEW;
	}

	ret = Ql_FS_Read(fd, (void *)&FTable, sizeof(FTable), &bytesRead);
	Ql_FS_Close(fd);
	if(ret != QL_RET_OK || bytesRead != sizeof(FTable))
	{
		LOGData(TAG_BATCH, "Error reading Batch Index File, ret=%d read=%d", ret, bytesRead);
		goto INIT_NEW;
	}

	if(FTable.magic != BATCH_TABLE_MAGIC)
	{
		LOGData(TAG_BATCH, "Batch format mismatch (got 0x%04X expected 0x%04X)", FTable.magic, BATCH_TABLE_MAGIC);
		goto INIT_NEW;
	}

	if(FTable.TotalFiles > MAX_FILE || FTable.head >= MAX_FILE || FTable.tail >= MAX_FILE)
	{
		LOGData(TAG_BATCH, "Invalid Batch Index (count=%d head=%d tail=%d)",
			FTable.TotalFiles, FTable.head, FTable.tail);
		goto INIT_NEW;
	}

	LOGVerbose(TAG_BATCH, "Batch: %d files (head=%d, tail=%d)", FTable.TotalFiles, FTable.head, FTable.tail);
	return FTable.TotalFiles;

INIT_NEW:
	Ql_memset(&FTable, 0, sizeof(FTable));
	FTable.magic = BATCH_TABLE_MAGIC;
	FTable.TotalFiles = 0;
	FTable.head = 0;
	FTable.tail = 0;
	UpdateFileTable();
	return 0;
}

static uint8_t WriteFileData(uint32_t addr, void *data, int len)
{
	int fd;
	s32 currentSize;
	s32 seekPos;
	s32 ret;
	u32 bytesWritten = 0;

	LOGData(TAG_BATCH, "Writing Batch Data, addr=%lu len=%d", addr, len);

	fd = Ql_FS_Open(BATCH_DATA_FILE, QL_FS_READ_WRITE);
	if(fd < 0)
	{
		fd = Ql_FS_Open(BATCH_DATA_FILE, QL_FS_CREATE_ALWAYS);
		if(fd < 0)
		{
			LOGData(TAG_BATCH, "Unable to open Batch Data File, err: %d", fd);
			return 0;
		}
	}

	currentSize = Ql_FS_GetSize(BATCH_DATA_FILE);
	if(currentSize < 0)
		currentSize = 0;

	if((addr + len) > (uint32_t)currentSize)
	{
		Ql_FS_Seek(fd, 0, QL_FS_FILE_END);
		uint32_t padLen = (addr + len) - currentSize;
		u8 *padBuf = Ql_MEM_Alloc(padLen);
		if(padBuf)
		{
			Ql_memset(padBuf, 0xFF, padLen);
			u32 wrote = 0;
			Ql_FS_Write(fd, padBuf, padLen, &wrote);
			Ql_MEM_Free(padBuf);
		}
		Ql_FS_Flush(fd);
	}

	seekPos = Ql_FS_Seek(fd, addr, QL_FS_FILE_BEGIN);
	if(seekPos != QL_RET_OK)
	{
		LOGData(TAG_BATCH, "Batch data seek error exp=%lu got=%d", addr, seekPos);
		Ql_FS_Close(fd);
		return 0;
	}

	ret = Ql_FS_Write(fd, data, len, &bytesWritten);
	Ql_FS_Close(fd);
	if(ret != QL_RET_OK || bytesWritten != (u32)len)
	{
		LOGData(TAG_BATCH, "Batch data write len error exp=%d got=%d ret=%d", len, bytesWritten, ret);
		return 0;
	}

	return 1;
}

static uint8_t ReadFileData(uint32_t addr, void *data, int len)
{
	int fd;
	s32 fileSize;
	s32 seekPos;
	s32 ret;
	u32 bytesRead = 0;

	LOGData(TAG_BATCH, "Reading Batch Data, addr=%lu len=%d", addr, len);
	if(Ql_FS_Check(BATCH_DATA_FILE) != QL_RET_OK)
	{
		LOGData(TAG_BATCH, "No Existing Batch Data File");
		return 0;
	}

	fileSize = Ql_FS_GetSize(BATCH_DATA_FILE);
	if(fileSize < (s32)(addr + len))
	{
		LOGData(TAG_BATCH, "Batch Data size %d < %d", fileSize, addr + len);
		return 0;
	}

	fd = Ql_FS_Open(BATCH_DATA_FILE, QL_FS_READ_ONLY);
	if(fd < 0)
	{
		LOGData(TAG_BATCH, "Unable to open Batch Data File, err: %d", fd);
		return 0;
	}

	seekPos = Ql_FS_Seek(fd, addr, QL_FS_FILE_BEGIN);
	if(seekPos != QL_RET_OK)
	{
		LOGData(TAG_BATCH, "Batch read seek error exp=%lu got=%d", addr, seekPos);
		Ql_FS_Close(fd);
		return 0;
	}

	ret = Ql_FS_Read(fd, data, len, &bytesRead);
	Ql_FS_Close(fd);
	if(ret != QL_RET_OK || bytesRead != (u32)len)
	{
		LOGData(TAG_BATCH, "Batch read len error exp=%d got=%d ret=%d", len, bytesRead, ret);
		return 0;
	}

	return 1;
}

void StoreFileToFlash(char *data, uint8_t paket)
{
	char dBuff[FILE_SIZE] = {0};
	uint32_t addr;
	uint16_t oldTail;

	LOGData(TAG_BATCH, "Storing packet to flash (type=%d)", paket);
	ReadFileTable();

	if(FTable.TotalFiles >= MAX_FILE)
	{
		LOGData(TAG_BATCH, "Batch FULL - overwriting oldest at slot %d", FTable.head);
		FTable.fInfo[FTable.head].valid = 0;
		FTable.head = (uint16_t)((FTable.head + 1) % MAX_FILE);
		FTable.TotalFiles--;
	}

	addr = SlotToAddress(FTable.tail);
	Ql_strncpy(dBuff, data, FILE_SIZE - 1);
	dBuff[FILE_SIZE - 1] = '\0';
	if(!WriteFileData(addr, (void *)dBuff, FILE_SIZE))
	{
		LOGData(TAG_BATCH, "Failed to write batch data");
		return;
	}

	FTable.fInfo[FTable.tail].valid = 1;
	FTable.fInfo[FTable.tail].packet = paket;
	oldTail = FTable.tail;
	FTable.tail = (uint16_t)((FTable.tail + 1) % MAX_FILE);
	FTable.TotalFiles++;
	UpdateFileTable();

	LOGData(TAG_BATCH, "Packet stored at slot %d (head=%d tail=%d count=%d)",
		oldTail, FTable.head, FTable.tail, FTable.TotalFiles);
}

uint16_t GetTotalFiles(void)
{
	return FTable.TotalFiles;
}

uint16_t ReadDataBatchExt(char *data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete, int16_t *outSlot)
{
	uint16_t slotsToCheck;
	uint16_t pos;
	uint8_t matchCount = 0;
	uint8_t validFound = 0;

	if (outSlot)
		*outSlot = -1;

	if(isDelete)
		LOGData(TAG_BATCH, "Deleting batch data [skip=%d] mode:%d/%d", lifocount, istypemasked, type);
	else
		LOGData(TAG_BATCH, "Reading batch data [skip=%d] mode:%d/%d", lifocount, istypemasked, type);

	if(FTable.TotalFiles == 0)
	{
		LOGData(TAG_BATCH, "No files in batch storage");
		return 0;
	}

	if(FTable.tail >= FTable.head)
		slotsToCheck = FTable.tail - FTable.head;
	else
		slotsToCheck = (MAX_FILE - FTable.head) + FTable.tail;

	if(slotsToCheck == 0 && FTable.TotalFiles > 0)
		slotsToCheck = MAX_FILE;

	pos = (FTable.tail == 0) ? (MAX_FILE - 1) : (uint16_t)(FTable.tail - 1);
	for(uint16_t i = 0; i < slotsToCheck; i++)
	{
		if(!FTable.fInfo[pos].valid)
		{
			pos = (pos == 0) ? (MAX_FILE - 1) : (uint16_t)(pos - 1);
			continue;
		}

		validFound++;
		if(istypemasked && FTable.fInfo[pos].packet != type)
		{
			LOGData(TAG_BATCH, "Slot %d skipped (type=%d want=%d)", pos, FTable.fInfo[pos].packet, type);
			pos = (pos == 0) ? (MAX_FILE - 1) : (uint16_t)(pos - 1);
			continue;
		}

		if(matchCount < lifocount)
		{
			matchCount++;
			pos = (pos == 0) ? (MAX_FILE - 1) : (uint16_t)(pos - 1);
			continue;
		}

		if(isDelete)
		{
			uint16_t expectedTailSlot = (FTable.tail == 0) ? (MAX_FILE - 1) : (uint16_t)(FTable.tail - 1);

			LOGData(TAG_BATCH, "Removing packet at slot %d", pos);
			FTable.fInfo[pos].valid = 0;
			FTable.fInfo[pos].packet = 0;
			if(pos == expectedTailSlot)
				FTable.tail = pos;
			if(pos == FTable.head)
			{
				while(FTable.TotalFiles > 1 && !FTable.fInfo[FTable.head].valid)
				{
					FTable.head = (uint16_t)((FTable.head + 1) % MAX_FILE);
					if(FTable.head == FTable.tail)
						break;
				}
			}

			FTable.TotalFiles--;
			UpdateFileTable();
			return 1;
		}

		if(outSlot)
		{
			*outSlot = (int16_t)pos;
		}

		if(ReadFileData(SlotToAddress(pos), (void *)data, FILE_SIZE))
		{
			print_long_string(data);
			return 1;
		}
		return 0;
	}

	if(validFound != FTable.TotalFiles)
	{
		LOGData(TAG_BATCH, "TotalFiles mismatch, repairing found=%d expected=%d", validFound, FTable.TotalFiles);
		FTable.TotalFiles = validFound;
		UpdateFileTable();
	}

	return 0;
}

uint16_t ReadDataBatch(char *data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete)
{
	return ReadDataBatchExt(data, type, istypemasked, lifocount, isDelete, NULL);
}

uint8_t DeleteDataBatchSlot(uint16_t pos)
{
	if(pos >= MAX_FILE)
	{
		return 0;
	}

	ReadFileTable();

	if(!FTable.fInfo[pos].valid)
	{
		LOGData(TAG_BATCH, "DeleteDataBatchSlot: slot %d already invalid", pos);
		return 0;
	}

	uint16_t expectedTailSlot = (FTable.tail == 0) ? (MAX_FILE - 1) : (uint16_t)(FTable.tail - 1);

	LOGData(TAG_BATCH, "DeleteDataBatchSlot: Removing packet at slot %d", pos);
	FTable.fInfo[pos].valid = 0;
	FTable.fInfo[pos].packet = 0;
	if(pos == expectedTailSlot)
		FTable.tail = pos;
	if(pos == FTable.head)
	{
		while(FTable.TotalFiles > 1 && !FTable.fInfo[FTable.head].valid)
		{
			FTable.head = (uint16_t)((FTable.head + 1) % MAX_FILE);
			if(FTable.head == FTable.tail)
				break;
		}
	}

	FTable.TotalFiles--;
	UpdateFileTable();
	return 1;
}

void ClearFileTable(void)
{
	LOGData(TAG_BATCH, "Clearing Batch Storage...");
	Ql_memset(&FTable, 0, sizeof(FTable));
	FTable.magic = BATCH_TABLE_MAGIC;
	FTable.TotalFiles = 0;
	FTable.head = 0;
	FTable.tail = 0;
	UpdateFileTable();

	if(Ql_FS_Check(BATCH_DATA_FILE) == QL_RET_OK)
	{
		Ql_FS_Delete(BATCH_DATA_FILE);
	}
}

uint8_t ClearBatchStorage(uint16_t *deletedCount, uint16_t *failedCount)
{
    const char *files[] = { BATCH_DATA_FILE, BATCH_INDEX_FILE };
    uint8_t success = 1;
    uint8_t i;

    if (deletedCount) *deletedCount = 0;
    if (failedCount) *failedCount = 0;

    // Delete both files rather than recreating an index file, so CLR DISK
    // actually returns their space. ReadFileTable() recreates a clean index on demand.
    for (i = 0; i < (sizeof(files) / sizeof(files[0])); i++)
    {
        if (Ql_FS_Check((char *)files[i]) == QL_RET_OK)
        {
            if (Ql_FS_Delete((char *)files[i]) == QL_RET_OK)
            {
                if (deletedCount) (*deletedCount)++;
            }
            else
            {
                success = 0;
                if (failedCount) (*failedCount)++;
                LOGData(TAG_BATCH, "Unable to delete batch storage file %s", files[i]);
            }
        }
    }

    Ql_memset(&FTable, 0, sizeof(FTable));
    FTable.magic = BATCH_TABLE_MAGIC;
    return success;
}

#endif
