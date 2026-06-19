#ifndef MOTA_H
#define MOTA_H

#include <stdint.h>
#include <MCU.h>

// Replace or use in addition to your existing SendAllChunksDirect
#define MOTA_WAIT_FLAG               MCOMM_COM_FUNCTION_FWUPDATE
#define MOTA_CHUNK_TIMEOUT_MS        6000   // tune as needed
#define MOTA_MAX_RETRIES             3
#define MOTA_BACKOFF_MS              100


extern uint16_t mota_rcv_chunk_num;
extern uint8_t IsMotaProcessing;

// Function declarations
uint8_t ProcessMCUOTA(char* binFilePath);
uint8_t SendAllChunksDirect(uint32_t fileHandle, uint32_t fileSize);

#endif

