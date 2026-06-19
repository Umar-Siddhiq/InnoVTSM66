#include "MOTA.h"
#include "MCU.h"
#include "FTP.h"  // For LOGData, TAG_FTP, etc.
#include "File.h" // For file operations
#include <string.h>
#include <stdlib.h>

uint16_t mota_rcv_chunk_num;
uint8_t IsMotaProcessing = 0;

uint8_t mota_start(uint32_t size)
{
    uint32_t chunkcount = size / MOTA_CHUNK_SIZE;
    if (size % MOTA_CHUNK_SIZE)
        chunkcount++;
    // Prevent chunk count overflow for 2-byte chunk number
    if (chunkcount == 0 || chunkcount > 0xFFFF) {
        LOGData(TAG_FTP, "MCU OTA Error: Invalid chunk count %lu", chunkcount);
        return 0;
    }

    // MOTA Start packet here
    MCOMMRcvFlags[MCOMM_COM_FUNCTION_SUCCESS] = 0;
    uint8_t txBuffer[5];
    int idx = 0;
    txBuffer[idx++] = MCOMM_COM_HEADER;
    txBuffer[idx++] = MCOMM_COM_FUNCTION_FWSTART;
    txBuffer[idx++] = (uint8_t)((chunkcount >> 8) & 0xff);  // High byte
    txBuffer[idx++] = (uint8_t)(chunkcount & 0xff);         // Low byte
    txBuffer[idx++] = MCOMM_COM_FOOTER; 

    if(idx != MCOMM_COM_LEN_FWSTART){
        LOGData(TAG_FTP, "MCU OTA Error: FWSTART length mismatch");
        return 0;
    }

    if (!MCOMM_SendData(txBuffer, idx, 1, MCOMM_COM_FUNCTION_SUCCESS, 5000)) {
        LOGData(TAG_FTP, "MCU OTA Error: Failed to send FWSTART");
        return 0;
    }

    return 1;
}
// Remove the duplicate definitions - keep ONLY ONE implementation
uint8_t SendAllChunksDirect(uint32_t fileHandle, uint32_t fileSize)
{
    uint8_t chunkBuffer[MOTA_CHUNK_SIZE];
    uint8_t txBuffer[1 + 1 + MOTA_CHUCK_NUM_SIZE + MOTA_CHUNK_SIZE + 1]; // 261
    uint32_t totalSent = 0;
    uint16_t chunkNumber = 0;
    u32 bytesRead = 0;

    LOGData(TAG_FTP, "Starting direct chunk transfer: %lu bytes", fileSize);

    while (totalSent < fileSize)
    {
        uint32_t remaining = fileSize - totalSent;
        uint32_t currentChunkSize = (remaining < MOTA_CHUNK_SIZE) ? remaining : MOTA_CHUNK_SIZE;

        // Read chunk from file
        bytesRead = 0;
        if (Ql_FS_Read(fileHandle, chunkBuffer, currentChunkSize, &bytesRead) != QL_RET_OK)
        {
            LOGData(TAG_FTP, "File read error at offset %lu", totalSent);
            return 0;
        }
        if (bytesRead == 0)
        {
            LOGData(TAG_FTP, "Reached EOF unexpectedly at offset %lu", totalSent);
            return 0;
        }

        // Build the frame: [HEADER, FUNC, chunk_hi, chunk_lo, data(256 padded), FOOTER]
        int idx = 0;
        memset(txBuffer, 0, sizeof(txBuffer));
        txBuffer[idx++] = MCOMM_COM_HEADER;
        txBuffer[idx++] = MCOMM_COM_FUNCTION_FWUPDATE;
        txBuffer[idx++] = (uint8_t)((chunkNumber >> 8) & 0xFF); // chunk high
        txBuffer[idx++] = (uint8_t)(chunkNumber & 0xFF);        // chunk low

        // Copy data and pad with 0xFF if needed
        memcpy(&txBuffer[idx], chunkBuffer, bytesRead);
        if (bytesRead < MOTA_CHUNK_SIZE)
        {
            memset(&txBuffer[idx + bytesRead], 0xFF, MOTA_CHUNK_SIZE - bytesRead);
        }
        idx += MOTA_CHUNK_SIZE;

        txBuffer[idx++] = MCOMM_COM_FOOTER; // footer
        const int frameLen = idx; // should be 261 for full chunk

        // Try send with retries and verify ACK chunk number
        uint8_t sent_ok = 0;
        for (int attempt = 0; attempt < MOTA_MAX_RETRIES; ++attempt)
        {
            // clear ACK flag and recorded chunk number BEFORE sending
            MCOMMRcvFlags[MOTA_WAIT_FLAG] = 0;
            mota_rcv_chunk_num = 0; // ensure it's zeroed

            // Send and wait for ACK (isWait = 1)
            if (!MCOMM_SendData(txBuffer, frameLen, 1, MOTA_WAIT_FLAG, MOTA_CHUNK_TIMEOUT_MS))
            {
                LOGData(TAG_FTP, "Chunk %u: send/wait failed on attempt %d", chunkNumber, attempt + 1);
                ThreadSleep(MOTA_BACKOFF_MS);
                continue; // retry
            }

            // MCOMM_SendData returned success, which means wait-flag triggered.
            // Verify the ACK chunk number matches.
            if ((uint16_t)mota_rcv_chunk_num == chunkNumber)
            {
                sent_ok = 1;
                break; // next chunk
            }
            else
            {
                LOGData(TAG_FTP, "Chunk %u: ACK mismatch (got=%u) attempt %d", chunkNumber, mota_rcv_chunk_num, attempt + 1);
                ThreadSleep(MOTA_BACKOFF_MS);
                // continue to retry
            }
        } // attempts

        if (!sent_ok)
        {
            LOGData(TAG_FTP, "Chunk %u: failed after %d attempts. Aborting transfer.", chunkNumber, MOTA_MAX_RETRIES);
            return 0;
        }

        totalSent += bytesRead;
        chunkNumber++;

        // Periodic progress log
        if ((chunkNumber % 10) == 0 || totalSent >= fileSize)
        {
            uint8_t percent = (uint8_t)((totalSent * 100U) / fileSize);
            LOGData(TAG_FTP, "Progress: %lu/%lu bytes (%u%%) - Chunk %u", totalSent, fileSize, percent, chunkNumber);
        }

        // Small delay to avoid overwhelming MCU (tunable)
        ThreadSleep(50);
    }

    LOGData(TAG_FTP, "Direct chunk transfer completed: %u chunks, %lu bytes", chunkNumber, totalSent);
    return 1;
}

uint8_t mota_end(void)
{
    // MOTA End packet here
    MCOMMRcvFlags[MCOMM_COM_FUNCTION_SUCCESS] = 0;
    uint8_t txBuffer[5];
    int idx = 0;
    txBuffer[idx++] = MCOMM_COM_HEADER;
    txBuffer[idx++] = MCOMM_COM_FUNCTION_FWEND;
    txBuffer[idx++] = MCOMM_COM_FOOTER; 

    if(idx != MCOMM_COM_LEN_FWEND){
        LOGData(TAG_FTP, "MCU OTA Error: FWEND length mismatch");
        return 0;
    }

    if (!MCOMM_SendData(txBuffer, idx, 1, MCOMM_COM_FUNCTION_SUCCESS, 2000)) {
        LOGData(TAG_FTP, "MCU OTA Error: Failed to send FWEND");
        return 0;
    }

    return 1;
}

uint8_t ProcessMCUOTA(char* binFilePath)
{
    LOGData(TAG_FTP, "Starting MCU OTA process for file: %s", binFilePath);
    
    // Step 1: Get file size
    uint32_t fileSize = Ql_FS_GetSize(binFilePath);
    if(fileSize <= 0) 
    {
        LOGData(TAG_FTP, "MCU OTA Error: File not found or empty");
        return 0;
    }
    
    LOGData(TAG_FTP, "MCU firmware file size: %lu bytes", fileSize);
    
    // Step 2: Open file
    uint32_t fileHandle = Ql_FS_Open(binFilePath, QL_FS_READ_ONLY);
    if(fileHandle < 0) {
        LOGData(TAG_FTP, "MCU OTA Error: Failed to open firmware file");
        return 0;
    }

    if(!mota_start(fileSize)) {
        Ql_FS_Close(fileHandle);
        return 0;
    }

    // Step 3: Send all chunks directly
    uint8_t result = SendAllChunksDirect(fileHandle, fileSize);

    // MOTA End packet here
    
    if(!mota_end()) {
        result = 0; // mark failure if end fails
    }
    // Step 4: Cleanup
    Ql_FS_Close(fileHandle);
    
    if(result) {
        LOGData(TAG_FTP, "MCU OTA: All chunks sent successfully");
        return 1;
    } else {
        LOGData(TAG_FTP, "MCU OTA: Failed to send chunks");
        return 0;
    }
}