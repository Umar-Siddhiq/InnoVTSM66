#ifndef MOTA_H
#define MOTA_H

#include "NuMicro.h"
#include "W25Qxx.h"
#include "MComm.h"




#define FW_CHUNK_NUMBER_SHIFT    16
#define FW_CHUNK_NUMBER_MASK     0xFFFF0000

/*--------------------------------------------------------------------------------------------------------*/
// Single 32-bit variable dedicated ONLY to firmware update flags
extern volatile uint32_t FWUpdateFlags;
/**
 * FIRMWARE UPDATE FLAGS BIT ASSIGNMENT:
 * Each bit represents a specific state in the firmware update process
 */

// === FWUPDATE PROCESS STATES ===
#define FW_FLAG_UPDATE_AVAILABLE     0   // New firmware update available to start
#define FW_FLAG_CHUNK_AVAILABLE      1   // 256-byte chunk received and ready
#define FW_FLAG_CHUNK_VERIFIED       2   // Chunk data verified (CRC/checksum OK)
#define FW_FLAG_CHUNK_READY          3   // Chunk ready for SPI flash write
#define FW_FLAG_CHUNK_WRITING        4   // Currently writing chunk to SPI flash
#define FW_FLAG_CHUNK_WRITTEN        5   // Chunk successfully written to flash
#define FW_FLAG_CHUNK_VERIFIED_WRITE 6   // Flash write verified (read-back OK)
#define FW_FLAG_WAITING_NEXT_CHUNK   7   // Ready to receive next chunk
#define FW_FLAG_UPDATE_IN_PROGRESS   8   // Firmware update session active
#define FW_FLAG_UPDATE_COMPLETED     9   // All chunks received & written
#define FW_FLAG_UPDATE_ABORTED       10  // Firmware update aborted due to error
#define FW_FLAG_UPDATE_SUCCESS       11  // Firmware update completed successfully
#define FW_FLAG_UART_IRQ_DISABLED    12  // UART IRQ disabled during flash write
#define FW_FLAG_CHUNK_MISSING_ERROR   13  // Chunk received out of sequence

// ... reserve remaining bits for future FWUPDATE features

// Bit manipulation macros
#define FW_SET(bit)       (FWUpdateFlags |= (1UL << (bit)))
#define FW_CLEAR(bit)     (FWUpdateFlags &= ~(1UL << (bit)))
#define FW_CHECK(bit)     (FWUpdateFlags & (1UL << (bit)))
#define FW_TOGGLE(bit)    (FWUpdateFlags ^= (1UL << (bit)))

#define FW_SET_CHUNK_NUMBER(num)    (FWUpdateFlags = (FWUpdateFlags & 0x0000FFFF) | ((num) << FW_CHUNK_NUMBER_SHIFT))
#define FW_GET_CHUNK_NUMBER()       ((FWUpdateFlags & FW_CHUNK_NUMBER_MASK) >> FW_CHUNK_NUMBER_SHIFT)
#define FW_CLEAR_CHUNK_NUMBER()     (FWUpdateFlags &= 0x0000FFFF
/*--------------------------------------------------------------------------------------------------------*/

#define FW_CHUNK_SIZE					256
#define FW_FLASH_ADDR_BASE		0x1F0000
#define FW_FLASH_SIZE					0x10000

extern uint16_t expected_chunk_number;
extern uint16_t total_chunks;

void ProcessFirmwareUpdateSM(void);
void FW_EraseFirmwareSectors(void);
void FW_ReadAllSPIFlash(void);

#endif