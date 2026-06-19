#include "HttpQueue.h"

#ifdef HTTP_QUEUE

#include "Server.h"

HttpQueue httpQueue;

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

    httpQueue.tail = (uint8_t)((httpQueue.tail + 1) % HTTP_QUEUE_SIZE);
    httpQueue.count++;
    LOGData(TAG_SERVER, "HTTP queue add: count=%d, freq=%d, type=%d", httpQueue.count, frequency_sec, storage_type);
    HttpQueue_Unlock();
    return 1;
}

// Track when queue entered sending state for stuck detection
static uint32_t queue_send_start_ms = 0;
static const uint32_t QUEUE_STUCK_TIMEOUT_MS = 60000;  // 60 seconds

void HttpQueue_Process(void)
{
    uint32_t now_ms = Ql_GetMsSincePwrOn();
    
    // EMERGENCY CLEAR: If sending or IsSendProcess stuck > 60 seconds, force reset
    if((httpQueue.sending || IsSendProcess) && queue_send_start_ms > 0) {
        uint32_t stuck_ms = now_ms - queue_send_start_ms;
        if(stuck_ms > QUEUE_STUCK_TIMEOUT_MS) {
            LOGData(TAG_SERVER, "[EMERGENCY] Queue STUCK for %ldms! sending=%d, IsSendProcess=%d, FORCING RESET",
                    stuck_ms, httpQueue.sending, IsSendProcess);
            httpQueue.sending = 0;
            IsSendProcess = 0;
            queue_send_start_ms = 0;
            return;  // Let retry happen next call
        }
    }
    
    // Safety check for IsSendProcess corruption
    if(IsSendProcess > 1) {
        LOGData(TAG_SERVER, "WARNING: IsSendProcess corrupted! Value=%d, resetting to 0", IsSendProcess);
        IsSendProcess = 0;
    }
    
    uint8_t sent = 0;
    uint8_t failed = 0;
    LOGData(TAG_SERVER, "HTTP queue process ENTRY: count=%d, sending=%d, IsSendProcess=%d", httpQueue.count, httpQueue.sending, IsSendProcess);
    if(httpQueue.count == 0 || httpQueue.sending || IsSendProcess) {
        LOGData(TAG_SERVER, "HTTP queue process skipped: count=%d, sending=%d, IsSendProcess=%d", httpQueue.count, httpQueue.sending, IsSendProcess);
        return;
    }
    
    // Mark start time for stuck detection
    queue_send_start_ms = now_ms;

    LOGData(TAG_SERVER, "HTTP queue process: pending=%d", httpQueue.count);

    httpQueue.sending = 1;
    if(!HttpQueue_WaitLock(50)) {
        httpQueue.sending = 0;
        IsSendProcess = 0;
        LOGData(TAG_SERVER, "[FIX_LEAK] HttpQueue initial lock timeout, IsSendProcess cleared");
        return;
    }

    HttpQueue_ExpireOld();
    if(httpQueue.count == 0) {
        httpQueue.sending = 0;
        HttpQueue_Unlock();
        return;
    }

    if(ServerSocket[0].SocketState != SOCKET_CONNECTED) {
        HTTPConnectFlag = 1;
        httpQueue.sending = 0;
        HttpQueue_Unlock();
        return;
    }

    IsSendProcess = 1;
    while(httpQueue.count > 0 && sent < HTTP_QUEUE_BURST && !failed) {
        HttpQueueItem *item = &httpQueue.items[httpQueue.head];
        uint8_t keepAlive = (httpQueue.count > 1 && sent < (HTTP_QUEUE_BURST - 1)) ? 1 : 0;
        char sendData[HTTP_QUEUE_PACKET_SIZE];
        uint16_t frequency = item->frequency_sec;

        Ql_strncpy(sendData, item->data, HTTP_QUEUE_PACKET_SIZE - 1);
        sendData[HTTP_QUEUE_PACKET_SIZE - 1] = '\0';

        HttpQueue_Unlock();
        if(!SendDataToServer(sendData, keepAlive, frequency)) {
            failed = 1;
        }
        if(!HttpQueue_WaitLock(200)) {
            failed = 1;
            IsSendProcess = 0;
            LOGData(TAG_SERVER, "[FIX_LEAK] HttpQueue loop lock timeout, IsSendProcess cleared");
            break;
        }

        if(!failed) {
            HttpQueue_RemoveHead();
            sent++;
        }
    }

    if(!failed && sent > 0) {
        HTTP_Close(0);
    }

    IsSendProcess = 0;
    httpQueue.sending = 0;
    queue_send_start_ms = 0;  // Clear stuck timer on success
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