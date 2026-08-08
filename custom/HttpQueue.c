#include "HttpQueue.h"

#ifdef HTTP_QUEUE

#include "Server.h"
#include "GPRS.h"

HttpQueue httpQueue;
static volatile uint8_t g_httpQueuePaused = 0;

void HttpQueue_Pause(void)    { g_httpQueuePaused = 1; }
void HttpQueue_Resume(void)   { g_httpQueuePaused = 0; }
uint8_t HttpQueue_IsSending(void) { return httpQueue.sending; }

/* Force-abort an in-progress queue send for SOS/tamper emergency.
 * Closes the QHTTP session, resets all send-state flags, and logs the abort. */
void HttpQueue_AbortSend(void)
{
    LOGData(TAG_SERVER, "HttpQueue: aborting in-progress send for emergency (IsSend=%d q.sending=%d)",
            IsSendProcess, httpQueue.sending);
    HTTP_Close(0);
    IsSendProcess = 0;
    httpQueue.sending = 0;
    httpQueue.busy = 0;
    httpQueue.queue_send_start_ms = 0;
}

static void http_queue_thread_init(u32 taskId)
{
    OSThread queueThread = {0};

    queueThread.taskId = taskId;
    Ql_strcpy(queueThread.taskName, "HTTP Queue");
    queueThread.taskEnable = 1;
    queueThread.taskState = TASK_STATE_NORMAL;
    queueThread.taskPriority = 1;
    InitializeThread(&queueThread);
}

static uint8_t HttpQueue_WaitLock(uint32_t timeout_ms)
{
    uint32_t start = Ql_GetMsSincePwrOn();

    while(httpQueue.busy) {
        if((Ql_GetMsSincePwrOn() - start) > timeout_ms) {
            LOGData(TAG_SERVER, "HttpQueue lock timeout");
            return 0;
        }
        ThreadSleep(5);
    }

    httpQueue.busy = 1;
    return 1;
}

static void HttpQueue_Unlock(void)
{
    httpQueue.busy = 0;
}

static void HttpQueue_RemoveHead(void)
{
    if(httpQueue.count == 0) {
        return;
    }

    httpQueue.items[httpQueue.head].valid = 0;
    httpQueue.head = (uint8_t)((httpQueue.head + 1) % HTTP_QUEUE_SIZE);
    httpQueue.count--;
}

static uint8_t HttpQueue_StoreOldest(void)
{
    char flashData[HTTP_QUEUE_PACKET_SIZE];
    uint8_t flashType;

    if(httpQueue.count == 0) {
        return 0;
    }

    Ql_strncpy(flashData, httpQueue.items[httpQueue.head].data, HTTP_QUEUE_PACKET_SIZE - 1);
    flashData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
    flashType = httpQueue.items[httpQueue.head].storage_type;
    HttpQueue_RemoveHead();

    HttpQueue_Unlock();
    StoreFileToFlash(flashData, flashType);
    return HttpQueue_WaitLock(100);
}

static uint8_t HttpQueue_ExpireOld(void)
{
    uint8_t expired = 0;
    uint32_t now = Ql_GetMsSincePwrOn();

    while(httpQueue.count > 0) {
        HttpQueueItem *item = &httpQueue.items[httpQueue.head];
        int32_t timeDiff = (int32_t)(item->expiry_time - now);

        if(timeDiff > 0) {
            break;
        }

        {
            char expiredData[HTTP_QUEUE_PACKET_SIZE];
            uint8_t expiredType = item->storage_type;

            Ql_strncpy(expiredData, item->data, HTTP_QUEUE_PACKET_SIZE - 1);
            expiredData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
            HttpQueue_RemoveHead();
            HttpQueue_Unlock();
            StoreFileToFlash(expiredData, expiredType);
            if(!HttpQueue_WaitLock(100)) {
                return (uint8_t)(expired + 1);
            }
        }

        expired++;
    }

    return expired;
}

void HttpQueue_Init(void)
{
    Ql_memset(&httpQueue, 0, sizeof(httpQueue));
}

uint8_t HttpQueue_Add(const char *data, uint16_t frequency_sec, uint8_t storage_type)
{
    HttpQueueItem *item;

    if(data == NULL || Ql_strlen(data) == 0 || Ql_strlen(data) >= HTTP_QUEUE_PACKET_SIZE) {
        return 0;
    }
    if(!HttpQueue_WaitLock(50)) {
        return 0;
    }

    if(httpQueue.count >= HTTP_QUEUE_SIZE) {
        if(!HttpQueue_StoreOldest()) {
            HttpQueue_Unlock();
            return 0;
        }
    }

    item = &httpQueue.items[httpQueue.tail];
    Ql_strncpy(item->data, data, HTTP_QUEUE_PACKET_SIZE - 1);
    item->data[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
    item->expiry_time = Ql_GetMsSincePwrOn() + HTTP_QUEUE_CALC_EXPIRY_MS(frequency_sec);
    item->frequency_sec = frequency_sec;
    item->storage_type = storage_type;
    item->valid = 1;
    item->retry_count = 0;

    httpQueue.tail = (uint8_t)((httpQueue.tail + 1) % HTTP_QUEUE_SIZE);
    httpQueue.count++;
    LOGData(TAG_SERVER, "HTTP queue add: count=%d, freq=%d, type=%d", httpQueue.count, frequency_sec, storage_type);
    HttpQueue_Unlock();
    return 1;
}

void HttpQueue_Process(void)
{
    uint8_t sent = 0;
    uint8_t failed = 0;
    static uint32_t last_log_time = 0;
    static uint32_t last_gprs_wait_log = 0;
    uint32_t now = Ql_GetMsSincePwrOn();
    
    // Stuck Thread Watchdog
    if (httpQueue.sending || IsSendProcess) {
        if (httpQueue.queue_send_start_ms == 0) {
            httpQueue.queue_send_start_ms = now;
        } else if ((now - httpQueue.queue_send_start_ms) > QUEUE_STUCK_TIMEOUT_MS) {
            LOGData(TAG_SERVER, "HTTP Queue Watchdog: stuck detected (sending=%d, IsSendProcess=%d) for %dms! Resetting state.", 
                    httpQueue.sending, IsSendProcess, (int)(now - httpQueue.queue_send_start_ms));
            httpQueue.sending = 0;
            IsSendProcess = 0;
            httpQueue.busy = 0; // Release queue lock
            httpQueue.queue_send_start_ms = 0;
        }
    } else {
        httpQueue.queue_send_start_ms = 0;
    }

    if (now - last_log_time > 10000) {
        LOGVerbose(TAG_SERVER, "HttpQueue Status: count=%d, sending=%d, IsSendProcess=%d",
                httpQueue.count, httpQueue.sending, IsSendProcess);
        last_log_time = now;
    }

    if(httpQueue.count == 0 || httpQueue.sending || IsSendProcess || g_httpQueuePaused) {
        return;
    }

    httpQueue.sending = 1;
    if(!HttpQueue_WaitLock(50)) {
        httpQueue.sending = 0;
        httpQueue.queue_send_start_ms = 0;
        return;
    }

    HttpQueue_ExpireOld();
    if(httpQueue.count == 0) {
        httpQueue.sending = 0;
        httpQueue.queue_send_start_ms = 0;
        HttpQueue_Unlock();
        return;
    }

    if(GSM.GSMState != GPRS_ACTIVE) {
        if(now - last_gprs_wait_log > 10000) {
            LOGData(TAG_SERVER, "HTTP queue waiting for GPRS before send (state=%d, pending=%d)",
                    GSM.GSMState, httpQueue.count);
            last_gprs_wait_log = now;
        }
        httpQueue.sending = 0;
        httpQueue.queue_send_start_ms = 0;
        HttpQueue_Unlock();
        return;
    }

    IsSendProcess = 1;
    LOGData(TAG_SERVER, "HTTP queue process: pending=%d", httpQueue.count);
    while(httpQueue.count > 0 && sent < HTTP_QUEUE_BURST && !failed) {
        HttpQueueItem *item = &httpQueue.items[httpQueue.head];
        uint8_t keepAlive = (httpQueue.count > 1 && sent < (HTTP_QUEUE_BURST - 1)) ? 1 : 0;
        char sendData[HTTP_QUEUE_PACKET_SIZE];
        uint16_t frequency = item->frequency_sec;
        uint8_t storage_type = item->storage_type;

        Ql_strncpy(sendData, item->data, HTTP_QUEUE_PACKET_SIZE - 1);
        sendData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';

        HttpQueue_Unlock();
        if(!SendDataToServer(sendData, keepAlive, frequency)) {
            if(!HttpQueue_WaitLock(200)) {
                failed = 1;
                break;
            }
            httpQueue.items[httpQueue.head].retry_count++;
            if(httpQueue.items[httpQueue.head].retry_count >= HTTP_QUEUE_MAX_RETRIES) {
                LOGData(TAG_SERVER, "HTTP queue: item retries exhausted (%d), evicting to flash",
                        HTTP_QUEUE_MAX_RETRIES);
                HttpQueue_RemoveHead();
                HttpQueue_Unlock();
                StoreFileToFlash(sendData, storage_type);
                if(!HttpQueue_WaitLock(200)) {
                    failed = 1;
                    break;
                }
            }
            failed = 1;
        } else {
            if(!HttpQueue_WaitLock(200)) {
                failed = 1;
                break;
            }
            HttpQueue_RemoveHead();
            sent++;
        }
    }

    if(!failed && sent > 0) {
        HTTP_Close(0);
    }

    IsSendProcess = 0;
    httpQueue.sending = 0;
    httpQueue.queue_send_start_ms = 0;
    HttpQueue_Unlock();
}

uint8_t HttpQueue_Count(void)
{
    return httpQueue.count;
}

uint8_t HttpQueue_HasPending(void)
{
    return (httpQueue.count > 0) ? 1 : 0;
}

uint8_t HttpQueue_HasNormalPending(void)
{
    uint8_t i, idx;
    for(i = 0; i < httpQueue.count; i++) {
        idx = (uint8_t)((httpQueue.head + i) % HTTP_QUEUE_SIZE);
        if(httpQueue.items[idx].valid &&
           httpQueue.items[idx].storage_type == HTTP_QUEUE_TYPE_NORMAL) {
            return 1;
        }
    }
    return 0;
}

/* Replace the most-recently-added NORMAL item in the queue with fresh data. */
void HttpQueue_ReplaceLatestNormal(const char *data, uint16_t frequency_sec)
{
    int8_t i;
    if(data == NULL || Ql_strlen(data) == 0) return;
    if(!HttpQueue_WaitLock(50)) return;
    for(i = (int8_t)(httpQueue.count - 1); i >= 0; i--) {
        uint8_t idx = (uint8_t)((httpQueue.head + (uint8_t)i) % HTTP_QUEUE_SIZE);
        if(httpQueue.items[idx].valid &&
           httpQueue.items[idx].storage_type == HTTP_QUEUE_TYPE_NORMAL) {
            Ql_strncpy(httpQueue.items[idx].data, data, HTTP_QUEUE_PACKET_SIZE - 1);
            httpQueue.items[idx].data[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';
            httpQueue.items[idx].expiry_time =
                Ql_GetMsSincePwrOn() + HTTP_QUEUE_CALC_EXPIRY_MS(frequency_sec);
            httpQueue.items[idx].frequency_sec = frequency_sec;
            httpQueue.items[idx].retry_count = 0;
            LOGData(TAG_SERVER, "HTTP queue NRM replaced at idx=%d", idx);
            HttpQueue_Unlock();
            return;
        }
    }
    HttpQueue_Unlock();
}

void HttpQueue_Flush(void)
{
    if(!HttpQueue_WaitLock(500)) {
        return;
    }

    while(httpQueue.count > 0) {
        StoreFileToFlash(httpQueue.items[httpQueue.head].data, httpQueue.items[httpQueue.head].storage_type);
        HttpQueue_RemoveHead();
    }

    HttpQueue_Unlock();
}

void HttpQueueThreadEntry(s32 taskId)
{
    http_queue_thread_init(taskId);
    HttpQueue_Init();
    ThreadSleep(5000);

    while(1) {
        HttpQueue_Process();
        ThreadSleep(50);
    }
}

#endif