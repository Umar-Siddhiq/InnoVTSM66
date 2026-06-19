/**
 * @file HttpQueue.h
 * @brief HTTP Queue System for reliable packet transmission
 * 
 * Enable with HTTP_QUEUE macro in build config
 */

#ifndef _HTTP_QUEUE_H
#define _HTTP_QUEUE_H

#include <stdint.h>
#include <string.h>

#ifdef HTTP_QUEUE

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/** Maximum packets in queue */
#define HTTP_QUEUE_SIZE         10

/** Maximum packets to send per keep-alive session (burst) */
#define HTTP_QUEUE_BURST        4

/** Maximum packet data size */
#define HTTP_QUEUE_PACKET_SIZE  512

/** HTTP receive buffer size */
#ifndef HTTP_RECV_BUF_SIZE
#define HTTP_RECV_BUF_SIZE      512
#endif

/** Expiry multiplier - packet expires after (frequency ¡Á this) seconds */
#define HTTP_QUEUE_EXPIRY_MULTIPLIER  5

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Single queue item
 */
typedef struct {
    char data[HTTP_QUEUE_PACKET_SIZE];  // Packet data
    uint32_t expiry_time;               // Absolute expiry time (ms)
    uint16_t frequency_sec;             // Original frequency in seconds (for timeout)
    uint8_t storage_type;               // ALERT(3) or NORMAL(0) for flash
    uint8_t valid;                      // Slot in use (1) or empty (0)
} HttpQueueItem;

/**
 * @brief Circular queue structure
 */
typedef struct {
    HttpQueueItem items[HTTP_QUEUE_SIZE];
    uint8_t head;                       // Read position (oldest)
    uint8_t tail;                       // Write position (next free)
    uint8_t count;                      // Current items in queue
    volatile uint8_t busy;              // Access lock flag
    volatile uint8_t sending;           // Currently sending flag
} HttpQueue;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize the HTTP queue
 * Call at system startup
 */
void HttpQueue_Init(void);

/**
 * @brief Add packet to queue
 * 
 * @param data Packet data string
 * @param frequency_sec Current interval/frequency in seconds
 * @param storage_type ALERT(3) or NORMAL(0) for flash fallback
 * @return 1 on success, 0 on failure
 * 
 * Expiry is calculated as: frequency_sec ¡Á HTTP_QUEUE_EXPIRY_MULTIPLIER
 * Timeout for SendDataToServer uses the original frequency_sec
 * If queue is full, oldest packet is stored to flash first.
 * Called by Server thread.
 */
uint8_t HttpQueue_Add(const char* data, uint16_t frequency_sec, uint8_t storage_type);

/**
 * @brief Process the HTTP queue
 * 
 * - Expires old packets (stores to flash)
 * - Connects if needed
 * - Sends burst of packets with keep-alive
 * 
 * Called by HTTP thread periodically.
 */
void HttpQueue_Process(void);

/**
 * @brief Get current queue count
 * @return Number of packets in queue
 */
uint8_t HttpQueue_Count(void);

/**
 * @brief Check if queue has pending packets
 * @return 1 if packets pending, 0 if empty
 */
uint8_t HttpQueue_HasPending(void);

/**
 * @brief Flush all packets to flash
 * Call before shutdown to preserve data
 */
void HttpQueue_Flush(void);

/**
 * @brief HTTP Queue Thread Entry Point
 * Call this from nwy_open_app_entry to start the queue thread
 * 
 * @param param Unused (pass NULL)
 */
void HttpQueueThreadEntry(void* param);

/*============================================================================
 * HELPER MACROS
 *============================================================================*/

/**
 * @brief Calculate expiry time in ms from frequency
 */
#define HTTP_QUEUE_CALC_EXPIRY_MS(freq_sec) \
    ((uint32_t)(freq_sec) * HTTP_QUEUE_EXPIRY_MULTIPLIER * 1000)

/**
 * @brief Storage types for flash fallback
 */
#define HTTP_QUEUE_TYPE_NORMAL  0
#define HTTP_QUEUE_TYPE_ALERT   3

#endif /* HTTP_QUEUE */

#endif /* _HTTP_QUEUE_H */
