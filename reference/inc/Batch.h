

#ifndef						_BATCH_DATA_H
#define						_BATCH_DATA_H

#include "project.h"
#include "MCU.h"
#include "Vts.h"
#include <string.h>



#define						INDEX_ADDRESS				0x00000000
#define						MAX_FILE					50
#define						FILE_START_ADDRESS			1024
#define						FILE_SIZE					256
#define 					MAX_VALID_PACKET_TYPE 		4

#define 					BATCH_INDEX_FILE	"BatchIndex.bin"
#define	 					BATCH_DATA_FILE		"BatchFile.bin"

// Magic number to detect format changes - increment when structure changes
#define                     BATCH_TABLE_MAGIC           0xBA02

// Circular buffer file info - simplified
typedef struct
{
	uint8_t valid;   // 1 = has data, 0 = empty
	uint8_t packet;  // Packet type (NORMAL=0, ALERT=3, etc.)
}FInfoTypedef;

// Circular file table structure
typedef struct 
{
	uint16_t magic;        // Magic number to detect format (BATCH_TABLE_MAGIC)
	uint16_t TotalFiles;   // Count of valid files
	uint16_t head;         // Index of oldest file (read from here)
	uint16_t tail;         // Index of next write position
	FInfoTypedef fInfo[MAX_FILE];
}FTableTypedef;

extern FTableTypedef FTable;

void StoreFileToFlash(char* data ,uint8_t paket);
//void RemoveLastPacket(uint8_t type, uint8_t istypemasked);
uint16_t ReadDataBatch(char* data, uint8_t type, uint8_t istypemasked, uint8_t lifocount, uint8_t isDelete);
uint16_t ReadFileTable(void);
void ClearFileTable(void);
uint16_t GetTotalFiles(void);

#endif						// _BATCH_DATA_H



