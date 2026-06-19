#ifndef _HTTP_QUEUE_H
#define _HTTP_QUEUE_H

#include <stdint.h>
#include <string.h>
#include "VTS.h"

#ifdef HTTP_QUEUE

#define HTTP_QUEUE_SIZE 10
#define HTTP_QUEUE_BURST 4
#define HTTP_QUEUE_PACKET_SIZE 512
#define HTTP_QUEUE_EXPIRY_MULTIPLIER 5

typedef struct {
    char data[HTTP_QUEUE_PACKET_SIZE];
    uint32_t expiry_time;
    uint16_t frequency_sec;
    uint8_t storage_type;
    uint8_t valid;
} HttpQueueItem;

typedef struct {
    HttpQueueItem items[HTTP_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    volatile uint8_t busy;
    volatile uint8_t sending;
} HttpQueue;

void HttpQueue_Init(void);
uint8_t HttpQueue_Add(const char *data, uint16_t frequency_sec, uint8_t storage_type);
void HttpQueue_Process(void);
uint8_t HttpQueue_Count(void);
uint8_t HttpQueue_HasPending(void);
void HttpQueue_Flush(void);
void HttpQueueThreadEntry(s32 taskId);

#define HTTP_QUEUE_CALC_EXPIRY_MS(freq_sec) ((uint32_t)(freq_sec) * HTTP_QUEUE_EXPIRY_MULTIPLIER * 1000UL)
#define HTTP_QUEUE_TYPE_NORMAL 0
#define HTTP_QUEUE_TYPE_ALERT 3

#endif

#endif