

/**
 * @file Batch.c
 * @brief Circular Batch Storage System
 * 
 * Implements a circular FIFO buffer for storing packets to flash when
 * network is unavailable. Packets are stored in order and retrieved
 * FIFO (oldest first) for batch sending.
 * 
 * Structure:
 * - FTable: Circular index with head/tail pointers
 * - BatchFile.bin: Data file with fixed-size slots
 * - Each slot is FILE_SIZE (256) bytes
 * - MAX_FILE (50) slots available
 */

#include "Batch.h"

FTableTypedef FTable;

/**
 * @brief Calculate file address from slot index
 */
static inline uint32_t SlotToAddress(uint16_t slot) {
    return (slot * FILE_SIZE) + FILE_START_ADDRESS;
}

/**
 * @brief Update file table to persistent storage
 */
void UpdateFileTable(void)
{
    nwy_dbg_log("Updating Batch Table (head=%d, tail=%d, count=%d)...", 
                FTable.head, FTable.tail, FTable.TotalFiles);
    int fd = nwy_sdk_fopen(BATCH_INDEX_FILE, NWY_WB_PLUS_MODE);
    if (fd < 0) {
        nwy_dbg_log("Unable to open Batch Table File for write!, err: %d", fd);
        return;
    }

    nwy_sdk_fwrite(fd, (const void*)&FTable, sizeof(FTable));
    nwy_sdk_fclose(fd);
}

/**
 * @brief Read file table from persistent storage
 * @return Number of files in storage
 */
uint16_t ReadFileTable(void)
{
    int ret = 0;
    nwy_dbg_log("Reading File Table...");
    
    if (!nwy_sdk_fexist(BATCH_INDEX_FILE)) {
        nwy_dbg_log("No Existing Batch Index File");
        goto INIT_NEW;
    }

    ret = nwy_sdk_fsize(BATCH_INDEX_FILE);
    if (ret != sizeof(FTable)) {
        nwy_dbg_log("Invalid Batch Index File size %d (expected %d)", ret, sizeof(FTable));
        goto INIT_NEW;
    }

    int fd = nwy_sdk_fopen(BATCH_INDEX_FILE, NWY_RB_MODE);
    if (fd < 0) {
        nwy_dbg_log("Unable to open Batch Index File, err: %d", fd);
        goto INIT_NEW;
    }

    if (nwy_sdk_fread(fd, (void*)&FTable, sizeof(FTable)) != sizeof(FTable)) {
        nwy_dbg_log("Error reading Batch Index File");
        nwy_sdk_fclose(fd);
        goto INIT_NEW;
    }
    nwy_sdk_fclose(fd);

    // Check format magic - if mismatch, old format detected
    if (FTable.magic != BATCH_TABLE_MAGIC) {
        nwy_dbg_log("Batch format magic mismatch (got 0x%04X, expected 0x%04X) - clearing storage",
                    FTable.magic, BATCH_TABLE_MAGIC);
        goto INIT_NEW;
    }

    // Validate structure
    if (FTable.TotalFiles > MAX_FILE || 
        FTable.head >= MAX_FILE || 
        FTable.tail >= MAX_FILE) {
        nwy_dbg_log("Invalid Batch Index (total=%d, head=%d, tail=%d)", 
                    FTable.TotalFiles, FTable.head, FTable.tail);
        goto INIT_NEW;
    }

    nwy_dbg_log("Batch: %d files (head=%d, tail=%d)", 
                FTable.TotalFiles, FTable.head, FTable.tail);
    return FTable.TotalFiles;

INIT_NEW:
    memset(&FTable, 0, sizeof(FTable));
    FTable.magic = BATCH_TABLE_MAGIC;
    FTable.TotalFiles = 0;
    FTable.head = 0;
    FTable.tail = 0;
    UpdateFileTable();
    return 0;
}

/**
 * @brief Write data to a specific slot in the data file
 */
static uint8_t WriteFileData(uint32_t addr, void* data, int len)
{
    int ret;
    nwy_dbg_log("Writing Batch Data, addr=%lu, len=%d", addr, len);
    
    int fd = nwy_sdk_fopen(BATCH_DATA_FILE, NWY_AB_MODE);
    if (fd < 0) {
        nwy_dbg_log("Unable to open Batch Data File for write!, err: %d", fd);
        return 0;
    }

    int currentSize = nwy_sdk_fsize_fd(fd);
    if ((addr + len) > currentSize) {
        nwy_sdk_ftrunc_fd(fd, (addr + len));
    }

    ret = nwy_sdk_fseek(fd, addr, NWY_SEEK_SET);
    if (ret < 0) {
        nwy_dbg_log("Batch data write seek error %d", ret);
        nwy_sdk_fclose(fd);
        return 0;
    }
    
    ret = nwy_sdk_fwrite(fd, data, len);
    if (ret != len) {
        nwy_dbg_log("Batch data write len error exp %d got %d", len, ret);
        nwy_sdk_fclose(fd);
        return 0;
    }
    
    nwy_sdk_fclose(fd);
    return 1;
}

/**
 * @brief Read data from a specific slot in the data file
 */
static uint8_t ReadFileData(uint32_t addr, void* data, int len)
{
    nwy_dbg_log("Reading Batch Data, addr=%lu, len=%d", addr, len);
    
    if (!nwy_sdk_fexist(BATCH_DATA_FILE)) {
        nwy_dbg_log("No Existing Batch Data File");
        return 0;
    }

    int ret = nwy_sdk_fsize(BATCH_DATA_FILE);
    if (ret < (addr + len)) {
        nwy_dbg_log("Batch Data File size %d < addr+len %d", ret, addr + len);
        return 0;
    }

    int fd = nwy_sdk_fopen(BATCH_DATA_FILE, NWY_RB_MODE);
    if (fd < 0) {
        nwy_dbg_log("Unable to open Batch Data File, err: %d", fd);
        return 0;
    }

    ret = nwy_sdk_fseek(fd, addr, 0);
    if (ret != addr) {
        nwy_dbg_log("Batch read seek err, exp:%lu, got:%d", addr, ret);
        nwy_sdk_fclose(fd);
        return 0;
    }

    ret = nwy_sdk_fread(fd, data, len);
    if (ret != len) {
        nwy_dbg_log("Batch read data err, exp:%d, got:%d", len, ret);
        nwy_sdk_fclose(fd);
        return 0;
    }
    
    nwy_sdk_fclose(fd);
    return 1;
}

/**
 * @brief Store a packet to flash (circular FIFO)
 * 
 * If buffer is full, overwrites the oldest entry (head).
 * 
 * @param data Packet data string
 * @param paket Packet type (NORMAL=0, ALERT=3)
 */
void StoreFileToFlash(char* data, uint8_t paket)
{
    nwy_dbg_log("Storing packet to flash (type=%d)", paket);
    
    char dBuff[FILE_SIZE] = {0};
    uint32_t addr;
    
    // Read current table state
    ReadFileTable();
    
    // If full, we'll overwrite head (oldest) - but first advance head
    if (FTable.TotalFiles >= MAX_FILE) {
        nwy_dbg_log("Batch FULL - overwriting oldest at slot %d", FTable.head);
        // Mark old head as invalid and advance
        FTable.fInfo[FTable.head].valid = 0;
        FTable.head = (FTable.head + 1) % MAX_FILE;
        FTable.TotalFiles--;
    }
    
    // Write to tail position
    addr = SlotToAddress(FTable.tail);
    strncpy(dBuff, data, FILE_SIZE - 1);
    dBuff[FILE_SIZE - 1] = '\0';
    
    if (!WriteFileData(addr, (void*)dBuff, FILE_SIZE)) {
        nwy_dbg_log("Failed to write batch data!");
        return;
    }
    
    // Update table entry
    FTable.fInfo[FTable.tail].valid = 1;
    FTable.fInfo[FTable.tail].packet = paket;
    
    // Advance tail
    uint16_t oldTail = FTable.tail;
    FTable.tail = (FTable.tail + 1) % MAX_FILE;
    FTable.TotalFiles++;
    
    // Persist table
    UpdateFileTable();
    
    nwy_dbg_log("Packet stored at slot %d (head=%d, tail=%d, count=%d)", 
                oldTail, FTable.head, FTable.tail, FTable.TotalFiles);
}

/**
 * @brief Get total number of stored files
 */
uint16_t GetTotalFiles(void)
{
    return FTable.TotalFiles;
}

/**
 * @brief Read or delete batch data (FIFO order - oldest first)
 * 
 * @param data Buffer to store read data (ignored if isDelete)
 * @param type Packet type filter
 * @param istypemasked 1 = filter by type, 0 = any type
 * @param lifocount Skip this many matching packets from tail (0 = get newest)
 * @param isDelete 1 = delete instead of read
 * @return 1 on success, 0 on failure/empty
 */
uint16_t ReadDataBatch(char* data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete) 
{
    if (isDelete) {
        nwy_dbg_log("Deleting batch data [skip=%d] mode:%d/%d", lifocount, istypemasked, type);
    } else {
        nwy_dbg_log("Reading batch data [skip=%d] mode:%d/%d", lifocount, istypemasked, type);
    }
    
    if (FTable.TotalFiles == 0) {
        nwy_dbg_log("No files in batch storage!");
        return 0;
    }
    
    // LIFO: Walk from tail towards head (newest first)
    // Calculate how many slots to check
    uint16_t slotsToCheck;
    if (FTable.tail >= FTable.head) {
        slotsToCheck = FTable.tail - FTable.head;
    } else {
        slotsToCheck = MAX_FILE - FTable.head + FTable.tail;
    }
    
    // If slotsToCheck is 0 but we have files, check all slots
    if (slotsToCheck == 0 && FTable.TotalFiles > 0) {
        slotsToCheck = MAX_FILE;
    }
    
    nwy_dbg_log("ReadDataBatch LIFO: checking %d slots from tail=%d", slotsToCheck, FTable.tail);
    
    // Start from tail-1 (newest valid slot) and go backwards towards head
    uint16_t pos = (FTable.tail == 0) ? (MAX_FILE - 1) : (FTable.tail - 1);
    uint8_t matchCount = 0;
    uint8_t validFound = 0;
    
    for (uint16_t i = 0; i < slotsToCheck; i++) {
        // Check if this slot is valid
        if (!FTable.fInfo[pos].valid) {
            pos = (pos == 0) ? (MAX_FILE - 1) : (pos - 1);
            continue;
        }
        
        validFound++;
        
        // Check type filter
        if (istypemasked && FTable.fInfo[pos].packet != type) {
            nwy_dbg_log("Slot %d skipped (type=%d, want=%d)", pos, FTable.fInfo[pos].packet, type);
            pos = (pos == 0) ? (MAX_FILE - 1) : (pos - 1);
            continue;
        }
        
        // Check if we need to skip this one
        if (matchCount < lifocount) {
            nwy_dbg_log("Slot %d skipped (match %d < skip %d)", pos, matchCount, lifocount);
            matchCount++;
            pos = (pos == 0) ? (MAX_FILE - 1) : (pos - 1);
            continue;
        }
        
        // This is the one we want
        uint32_t addr = SlotToAddress(pos);
        
        if (isDelete) {
            nwy_dbg_log("Removing packet at slot %d (LIFO)", pos);
            FTable.fInfo[pos].valid = 0;
            FTable.fInfo[pos].packet = 0;
            
            // LIFO delete: if this was at tail-1, move tail back
            uint16_t expectedTailSlot = (FTable.tail == 0) ? (MAX_FILE - 1) : (FTable.tail - 1);
            if (pos == expectedTailSlot) {
                // Move tail back to this position (will be overwritten next write)
                FTable.tail = pos;
            }
            // Also handle head if needed
            if (pos == FTable.head) {
                while (FTable.TotalFiles > 1 && !FTable.fInfo[FTable.head].valid) {
                    FTable.head = (FTable.head + 1) % MAX_FILE;
                    if (FTable.head == FTable.tail) break;
                }
            }
            
            FTable.TotalFiles--;
            UpdateFileTable();
            nwy_dbg_log("Packet removed LIFO (head=%d, tail=%d, count=%d)", 
                        FTable.head, FTable.tail, FTable.TotalFiles);
            return 1;
        } else {
            // Read the data
            if (ReadFileData(addr, (void*)data, FILE_SIZE)) {
                nwy_dbg_log("Batch data read from slot %d", pos);
                print_long_string(data);
                return 1;
            }
            return 0;
        }
    }
    
    nwy_dbg_log("No matching batch data found (valid=%d of %d files)", validFound, FTable.TotalFiles);
    
    // Integrity check: if validFound != TotalFiles, repair the table
    if (validFound != FTable.TotalFiles) {
        nwy_dbg_log("WARNING: TotalFiles mismatch! Repairing... (found=%d, expected=%d)", 
                    validFound, FTable.TotalFiles);
        FTable.TotalFiles = validFound;
        UpdateFileTable();
    }
    
    return 0;
}

/**
 * @brief Clear all batch storage
 */
void ClearFileTable(void)
{
    nwy_dbg_log("Clearing Batch Storage...");
    
    // Clear the table structure
    memset(&FTable, 0, sizeof(FTable));
    FTable.TotalFiles = 0;
    FTable.head = 0;
    FTable.tail = 0;
    
    // Persist empty table
    UpdateFileTable();

    // Remove the data file
    if (nwy_sdk_fexist(BATCH_DATA_FILE)) {
        nwy_sdk_file_unlink(BATCH_DATA_FILE);
        nwy_dbg_log("Batch Data File removed.");
    }
    
    nwy_dbg_log("Batch storage cleared.");
}



