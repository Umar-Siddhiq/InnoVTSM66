#ifndef _HTTP_QUEUE_H
#define _HTTP_QUEUE_H

#include <stdint.h>
#include <string.h>
#include "VTS.h"

#ifdef HTTP_QUEUE

#define HTTP_QUEUE_SIZE 10
#define HTTP_QUEUE_BURST 4
#define HTTP_QUEUE_PACKET_SIZE 512
/* A queued packet not sent within (interval x MULTIPLIER) is evicted to flash so
 * it goes out as a BTH batch packet (history, status 'H') instead of being
 * replayed LIVE with a stale timestamp. 1 = flash after ~1 interval of delay. */
#define HTTP_QUEUE_EXPIRY_MULTIPLIER 1
#define QUEUE_STUCK_TIMEOUT_MS 25000
/* After this many consecutive send failures the item is evicted to flash
 * and removed from the queue so the next item gets a turn. Without this,
 * a single unreachable server permanently blocks the queue head. */
#define HTTP_QUEUE_MAX_RETRIES 2

typedef struct {
    char data[HTTP_QUEUE_PACKET_SIZE];
    uint32_t expiry_time;
    uint16_t frequency_sec;
    uint8_t storage_type;
    uint8_t valid;
    uint8_t retry_count;
} HttpQueueItem;

typedef struct {
    HttpQueueItem items[HTTP_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    volatile uint8_t busy;
    volatile uint8_t sending;
    uint32_t queue_send_start_ms;
} HttpQueue;

void HttpQueue_Init(void);
uint8_t HttpQueue_Add(const char *data, uint16_t frequency_sec, uint8_t storage_type);
void HttpQueue_Process(void);
void HttpQueue_Pause(void);
void HttpQueue_Resume(void);
uint8_t HttpQueue_IsSending(void);
void HttpQueue_AbortSend(void);
uint8_t HttpQueue_Count(void);
uint8_t HttpQueue_HasPending(void);
uint8_t HttpQueue_HasNormalPending(void);
void HttpQueue_ReplaceLatestNormal(const char *data, uint16_t frequency_sec);
void HttpQueue_Flush(void);
void HttpQueueThreadEntry(s32 taskId);

#define HTTP_QUEUE_CALC_EXPIRY_MS(freq_sec) ((uint32_t)(freq_sec) * HTTP_QUEUE_EXPIRY_MULTIPLIER * 1000UL)
#define HTTP_QUEUE_TYPE_NORMAL 0
#define HTTP_QUEUE_TYPE_ALERT 3

#endif

#endif