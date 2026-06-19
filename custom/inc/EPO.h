#ifndef EPO_H
#define EPO_H
#include "VTS.h"
#include "File.h"
#include "GPS.h"


//#define EOP_USE

// Binary protocol definitions
#define BINARY_PREAMBLE1    0x04
#define BINARY_PREAMBLE2    0x24
#define BINARY_ENDWORD1     0xAA
#define BINARY_ENDWORD2     0x44

#define BINARY_PREAMBLE_SIZE    2
#define BINARY_CHECKSUM_SIZE    1
#define BINARY_ENDWORD_SIZE     2
#define BINARY_MSG_ID_SIZE      2
#define BINARY_LENGTH_SIZE      2
#define BINARY_MAX_DATA_SIZE    512

#define BINARY_CONTROL_SIZE     (BINARY_PREAMBLE_SIZE + BINARY_CHECKSUM_SIZE + BINARY_ENDWORD_SIZE)
#define BINARY_HEADER_SIZE      (BINARY_MSG_ID_SIZE + BINARY_LENGTH_SIZE)
#define BINARY_MAX_PAYLOAD_SIZE (BINARY_MAX_DATA_SIZE - BINARY_CONTROL_SIZE - BINARY_HEADER_SIZE)

// EPO specific definitions
#define EPO_SET_SIZE        2304
#define SAT_SIZE           72
#define SATS_PER_PACKET    3
#define EPO_START_MSG_ID   1200
#define EPO_DATA_MSG_ID    1201
#define EPO_END_MSG_ID     1202


typedef struct {
    uint16_t message_id;
    uint16_t data_size;
    uint8_t data[BINARY_MAX_PAYLOAD_SIZE];
} binary_payload_t;


void SendEPOFile(const char* filename);

// Flash EPO (binary protocol) injection helper.
// Returns true on success; on failure fills `status` with a short reason.
bool EPO_FlashInjectFile(const char* filename, char* status, uint16_t statusLen);

// Best-effort aiding helpers (PAIR590/PAIR600) per Quectel AGNSS guide.
// Safe to call even if time/position isn't available; returns false in that case.
bool EPO_SendReferenceTimeNow(void);
bool EPO_SendReferencePositionNow(void);

#endif
