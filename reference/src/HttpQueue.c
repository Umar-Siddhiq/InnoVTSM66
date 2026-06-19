/**
 * @file HttpQueue.c
 * @brief HTTP Queue System for reliable packet transmission
 * 
 * This implements a circular FIFO queue for HTTP packets that:
 * - Decouples packet generation from transmission
 * - Allows burst sending (multiple packets per connection)
 * - Handles expiry with flash storage fallback
 * - Maintains FIFO order (no priority - order of events matters)
 * 
 * Usage:
 * - Server thread calls HttpQueue_Add() to queue packets
 * - HTTP thread calls HttpQueue_Process() periodically
 * - Expired packets are stored to flash for batch sending later
 */

#include "project.h"   // Main project header - includes VTS.h which defines HTTP_QUEUE
#include "HttpQueue.h"
#include "Batch.h"

#ifdef HTTP_QUEUE

/* Queue instance */
HttpQueue httpQueue;

/* External references */
extern TCPSocketTypedef ServerSocket[];
extern uint8_t HTTPConnectFlag;
extern uint8_t IsSendProcess;

/* External function - uses existing proven send logic */
extern uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec);

/**
 * @brief Initialize the HTTP queue
 * Call this at system startup
 */
void HttpQueue_Init(void)
{
    memset(&httpQueue, 0, sizeof(HttpQueue));
    httpQueue.head = 0;
    httpQueue.tail = 0;
    httpQueue.count = 0;
    httpQueue.busy = 0;
    httpQueue.sending = 0;
    nwy_dbg_log("=== HttpQueue INIT ===");
    nwy_dbg_log("HttpQueue: size=%d, burst=%d, expiry_mult=%d", 
                HTTP_QUEUE_SIZE, HTTP_QUEUE_BURST, HTTP_QUEUE_EXPIRY_MULTIPLIER);
    nwy_dbg_log("HttpQueue: packet_size=%d bytes", HTTP_QUEUE_PACKET_SIZE);
}

/**
 * @brief Wait for busy flag to clear (with timeout)
 * @return 1 if acquired, 0 if timeout
 */
static uint8_t HttpQueue_WaitLock(uint32_t timeout_ms)
{
    uint32_t start = nwy_get_ms();
    while(httpQueue.busy) {
        if((nwy_get_ms() - start) > timeout_ms) {
            nwy_dbg_log("HttpQueue lock timeout!");
            return 0;
        }
        nwy_sleep(5);
    }
    httpQueue.busy = 1;
    return 1;
}

/**
 * @brief Release busy flag
 */
static void HttpQueue_Unlock(void)
{
    httpQueue.busy = 0;
}

/**
 * @brief Store oldest packet to flash and remove from queue
 * Called when queue is full and new packet needs to be added
 * NOTE: Caller must hold lock. Flash write done after queue update to minimize lock time.
 * @return 1 if packet was stored, 0 if queue was empty
 */
static uint8_t HttpQueue_StoreOldest(void)
{
    if(httpQueue.count == 0) return 0;
    
    HttpQueueItem* oldest = &httpQueue.items[httpQueue.head];
    
    // Copy data locally before modifying queue
    char flashData[HTTP_QUEUE_PACKET_SIZE];
    uint8_t flashType = oldest->storage_type;
    strncpy(flashData, oldest->data, HTTP_QUEUE_PACKET_SIZE - 1);
    flashData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
    
    nwy_dbg_log("HttpQueue overflow - storing oldest to flash (type=%d)", flashType);
    
    // Clear slot and advance head FIRST (fast operation)
    oldest->valid = 0;
    httpQueue.head = (httpQueue.head + 1) % HTTP_QUEUE_SIZE;
    httpQueue.count--;
    
    // Release lock BEFORE slow flash write
    HttpQueue_Unlock();
    
    // Now do slow flash write without holding lock
    StoreFileToFlash(flashData, flashType);
    
    // Re-acquire lock for caller
    HttpQueue_WaitLock(100);
    
    return 1;
}

/**
 * @brief Add packet to queue
 * 
 * @param data Packet data string
 * @param frequency_sec Current interval/frequency in seconds
 * @param storage_type ALERT(3) or NORMAL(0) for flash fallback
 * @return 1 on success, 0 on failure
 * 
 * Called by Server thread
 */
uint8_t HttpQueue_Add(const char* data, uint16_t frequency_sec, uint8_t storage_type)
{
    nwy_dbg_log(">>> HttpQueue_Add: freq=%ds, type=%d, len=%d", 
                frequency_sec, storage_type, data ? strlen(data) : 0);
    
    if(data == NULL || strlen(data) == 0) {
        nwy_dbg_log("HttpQueue_Add: FAIL - invalid data (null or empty)");
        return 0;
    }
    
    if(strlen(data) >= HTTP_QUEUE_PACKET_SIZE) {
        nwy_dbg_log("HttpQueue_Add: FAIL - packet too large (%d >= %d)", 
                    strlen(data), HTTP_QUEUE_PACKET_SIZE);
        return 0;
    }
    
    // Wait for lock (only waits for other queue modifications, not for sending)
    // Use short timeout - Server thread should not block for long
    // If we can't get lock quickly, caller should store to flash directly
    nwy_dbg_log("HttpQueue_Add: waiting for lock (busy=%d)", httpQueue.busy);
    if(!HttpQueue_WaitLock(50)) {  // Short 50ms timeout - don't block server thread
        nwy_dbg_log("HttpQueue_Add: FAIL - lock timeout (busy)");
        return 0;
    }
    
    // If queue is full, store oldest to flash first
    if(httpQueue.count >= HTTP_QUEUE_SIZE) {
        nwy_dbg_log("HttpQueue_Add: queue full (%d), storing oldest to flash", httpQueue.count);
        HttpQueue_StoreOldest();
    }
    
    // Calculate expiry: frequency ¡Á multiplier
    uint32_t expiry_ms = HTTP_QUEUE_CALC_EXPIRY_MS(frequency_sec);
    
    // Add new packet at tail
    HttpQueueItem* item = &httpQueue.items[httpQueue.tail];
    strncpy(item->data, data, HTTP_QUEUE_PACKET_SIZE - 1);
    item->data[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
    item->expiry_time = nwy_get_ms() + expiry_ms;
    item->frequency_sec = frequency_sec;
    item->storage_type = storage_type;
    item->valid = 1;
    
    uint8_t old_tail = httpQueue.tail;
    httpQueue.tail = (httpQueue.tail + 1) % HTTP_QUEUE_SIZE;
    httpQueue.count++;
    
    nwy_dbg_log("HttpQueue_Add: OK - slot=%d, head=%d, tail=%d, count=%d", 
                old_tail, httpQueue.head, httpQueue.tail, httpQueue.count);
    nwy_dbg_log("HttpQueue_Add: expiry=%lums from now (freq=%ds x %d)", 
                expiry_ms, frequency_sec, HTTP_QUEUE_EXPIRY_MULTIPLIER);
    
    HttpQueue_Unlock();
    return 1;
}

/**
 * @brief Remove packet from head of queue (after successful send)
 */
static void HttpQueue_RemoveHead(void)
{
    if(httpQueue.count == 0) return;
    
    httpQueue.items[httpQueue.head].valid = 0;
    httpQueue.head = (httpQueue.head + 1) % HTTP_QUEUE_SIZE;
    httpQueue.count--;
}

/**
 * @brief Check and expire old packets
 * Stores expired packets to flash (releases lock during flash writes)
 * @return Number of packets expired
 */
static uint8_t HttpQueue_ExpireOld(void)
{
    uint8_t expired = 0;
    uint32_t now = nwy_get_ms();
    
    // Temporary storage for expired packets (to write to flash after releasing lock)
    char expiredData[HTTP_QUEUE_PACKET_SIZE];
    uint8_t expiredType;
    
    while(httpQueue.count > 0) {
        HttpQueueItem* oldest = &httpQueue.items[httpQueue.head];
        
        // Handle time wraparound (after ~49 days)
        int32_t time_diff = (int32_t)(oldest->expiry_time - now);
        
        if(time_diff <= 0) {
            // Copy data before removing
            strncpy(expiredData, oldest->data, HTTP_QUEUE_PACKET_SIZE - 1);
            expiredData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
            expiredType = oldest->storage_type;
            
            nwy_dbg_log("HttpQueue: packet expired, storing to flash (type=%d)", expiredType);
            
            // Remove from queue first (fast)
            HttpQueue_RemoveHead();
            
            // Release lock for slow flash write
            HttpQueue_Unlock();
            StoreFileToFlash(expiredData, expiredType);
            
            // Re-acquire lock to continue
            if(!HttpQueue_WaitLock(100)) {
                // Couldn't get lock back, return what we have
                return expired + 1;
            }
            
            expired++;
        } else {
            // Oldest not expired, stop checking
            break;
        }
    }
    
    return expired;
}

/**
 * @brief Send single packet via HTTP POST
 * Uses existing SendDataToServer for proven reliability
 * 
 * @param data Packet data
 * @param frequency_sec Original frequency - used as timeout for SendDataToServer
 * @param keepAlive Whether to keep connection open
 * @return 1 on success, 0 on failure
 */
static uint8_t HttpQueue_SendPacket(const char* data, uint16_t frequency_sec, uint8_t keepAlive)
{
    // Use the original frequency as timeout for SendDataToServer
    return SendDataToServer((char*)data, keepAlive, frequency_sec);
}

/**
 * @brief Process the HTTP queue
 * 
 * - Expires old packets (stores to flash)
 * - Connects if needed
 * - Sends burst of packets with keep-alive
 * 
 * Called by HttpQueue thread periodically
 */
void HttpQueue_Process(void)
{
    // Quick check without lock
    if(httpQueue.count == 0) return;
    
    // Don't process if already sending (batch or other)
    if(httpQueue.sending) {
        nwy_dbg_log("HttpQueue_Process: skip - already sending");
        return;
    }
    if(IsSendProcess) {
        nwy_dbg_log("HttpQueue_Process: skip - IsSendProcess active");
        return;
    }
    
    // Mark as sending BEFORE acquiring lock (prevents re-entry)
    httpQueue.sending = 1;
    
    // Acquire lock for queue operations
    if(!HttpQueue_WaitLock(50)) {
        nwy_dbg_log("HttpQueue_Process: skip - lock timeout");
        httpQueue.sending = 0;
        return;
    }
    
    //nwy_dbg_log(">>> HttpQueue_Process: START (count=%d)", httpQueue.count);
    
    // 1. Expire old packets first
    uint8_t expired = HttpQueue_ExpireOld();
    if(expired > 0) {
        nwy_dbg_log("HttpQueue_Process: expired %d packets", expired);
    }
    
    // 2. Check if anything left to send
    if(httpQueue.count == 0) {
        nwy_dbg_log("HttpQueue_Process: nothing to send after expiry check");
        httpQueue.sending = 0;
        HttpQueue_Unlock();
        return;
    }
    
    // 3. Check connection state
    // nwy_dbg_log("HttpQueue_Process: socket state=%d (0=closed,1=connecting,2=connected)", 
    //             ServerSocket[0].SocketState);
    
    if(ServerSocket[0].SocketState != SOCKET_CONNECTED) {
        // Not connected - trigger connection
        if(ServerSocket[0].SocketState != SOCKET_CONNECTING) {
            //nwy_dbg_log("HttpQueue_Process: triggering HTTP connection...");
            HTTPConnectFlag = 1;
        } else {
            //nwy_dbg_log("HttpQueue_Process: connection already in progress, waiting...");
        }
        // Will process on next call when connected
        httpQueue.sending = 0;
        HttpQueue_Unlock();
        return;
    }
    
    // 4. Send burst of packets
    uint8_t sent = 0;
    uint8_t failed = 0;
    IsSendProcess = 1;
    
    nwy_dbg_log("HttpQueue_Process: connected! starting burst (pending=%d, burst_max=%d)", 
                httpQueue.count, HTTP_QUEUE_BURST);
    
    while(httpQueue.count > 0 && sent < HTTP_QUEUE_BURST && !failed) {
        HttpQueueItem* item = &httpQueue.items[httpQueue.head];
        
        nwy_dbg_log("HttpQueue_Process: processing slot %d (of %d in queue)", 
                    httpQueue.head, httpQueue.count);
        
        // Check if expired (might have expired during processing)
        uint32_t now = nwy_get_ms();
        int32_t time_diff = (int32_t)(item->expiry_time - now);
        
        if(time_diff <= 0) {
            nwy_dbg_log("HttpQueue_Process: packet expired during processing (diff=%ld)", time_diff);
            StoreFileToFlash(item->data, item->storage_type);
            HttpQueue_RemoveHead();
            continue;
        }
        
        // Determine keep-alive: 
        // - true if more packets AND not at burst limit
        // - false for last packet (close connection)
        uint8_t morePackets = (httpQueue.count > 1) && (sent < HTTP_QUEUE_BURST - 1);
        
        // Copy data and params locally so we can release lock during send
        char sendData[HTTP_QUEUE_PACKET_SIZE];
        strncpy(sendData, item->data, HTTP_QUEUE_PACKET_SIZE - 1);
        sendData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
        uint16_t freq = item->frequency_sec;
        (void)item->storage_type;  // Preserved if needed later
        
        // RELEASE LOCK during HTTP send (allows Add to work)
        HttpQueue_Unlock();
        
        nwy_dbg_log("HttpQueue_Process: sending packet (keepAlive=%d, freq=%ds)", 
                    morePackets, freq);
        
        // Send packet using original frequency as timeout
        uint8_t sendResult = HttpQueue_SendPacket(sendData, freq, morePackets);
        
        // RE-ACQUIRE LOCK to modify queue
        if(!HttpQueue_WaitLock(200)) {
            // Couldn't get lock back - something is wrong, abort
            nwy_dbg_log("HttpQueue_Process: ERROR - couldn't re-acquire lock!");
            failed = 1;
            break;
        }
        
        if(sendResult) {
            HttpQueue_RemoveHead();
            sent++;
            nwy_dbg_log("HttpQueue_Process: packet %d sent OK (remaining=%d)", 
                        sent, httpQueue.count);
        } else {
            // Send failed - stop burst, will retry next cycle
            nwy_dbg_log("HttpQueue_Process: SEND FAILED! stopping burst");
            failed = 1;
            
            // Close connection on failure using public API
            nwy_dbg_log("HttpQueue_Process: closing socket after failure");
            TCPSocket_Disconnect(&ServerSocket[0]);
        }
    }
    
    // 5. Close connection after burst (don't leave idle)
    if(!failed && sent > 0) {
        nwy_dbg_log("HttpQueue_Process: burst complete, sent %d packets", sent);
        // Connection should be closed by last packet with Connection: close
        // But verify it's closed
        if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
            nwy_dbg_log("HttpQueue_Process: closing connection after burst");
            TCPSocket_Disconnect(&ServerSocket[0]);
        }
    }
    
    nwy_dbg_log("<<< HttpQueue_Process: END (sent=%d, failed=%d, remaining=%d)", 
                sent, failed, httpQueue.count);
    
    IsSendProcess = 0;
    httpQueue.sending = 0;
    HttpQueue_Unlock();
}

/**
 * @brief Get current queue count
 * @return Number of packets in queue
 */
uint8_t HttpQueue_Count(void)
{
    return httpQueue.count;
}

/**
 * @brief Check if queue has pending packets
 * @return 1 if packets pending, 0 if empty
 */
uint8_t HttpQueue_HasPending(void)
{
    return (httpQueue.count > 0) ? 1 : 0;
}

/**
 * @brief Flush all packets to flash (for shutdown)
 */
void HttpQueue_Flush(void)
{
    if(!HttpQueue_WaitLock(500)) return;
    
    nwy_dbg_log("HttpQueue: flushing %d packets to flash", httpQueue.count);
    
    while(httpQueue.count > 0) {
        HttpQueueItem* item = &httpQueue.items[httpQueue.head];
        StoreFileToFlash(item->data, item->storage_type);
        HttpQueue_RemoveHead();
    }
    
    HttpQueue_Unlock();
}

/**
 * @brief HTTP Queue Thread Entry
 * Runs forever, processing the queue periodically
 * 
 * @param param Unused
 */
void HttpQueueThreadEntry(void* param)
{
    (void)param;
    
    nwy_dbg_log("========================================");
    nwy_dbg_log("HttpQueue thread starting...");
    nwy_dbg_log("  Queue Size: %d packets", HTTP_QUEUE_SIZE);
    nwy_dbg_log("  Burst Size: %d packets", HTTP_QUEUE_BURST);
    nwy_dbg_log("  Expiry Mult: %dx frequency", HTTP_QUEUE_EXPIRY_MULTIPLIER);
    nwy_dbg_log("  Packet Size: %d bytes max", HTTP_QUEUE_PACKET_SIZE);
    nwy_dbg_log("========================================");
    
    // Initialize queue
    HttpQueue_Init();
    
    // Wait for system to stabilize
    nwy_dbg_log("HttpQueue: waiting 5s for system stabilization...");
    nwy_sleep(5000);
    
    nwy_dbg_log("HttpQueue thread running - process loop starting");
    
    uint32_t loop_count = 0;
    while(1) {
        loop_count++;
        
        // Periodic status log every 60 seconds (120 loops @ 500ms)
        if(loop_count % 120 == 0) {
            nwy_dbg_log("HttpQueue: STATUS - count=%d, head=%d, tail=%d, sending=%d, busy=%d",
                        httpQueue.count, httpQueue.head, httpQueue.tail, 
                        httpQueue.sending, httpQueue.busy);
        }
        
        // Process queue - handles expiry, connection, sending
        HttpQueue_Process();
        
        // Sleep interval - balance between responsiveness and CPU usage
       
        nwy_sleep(50);
    }
}

#endif /* HTTP_QUEUE */
