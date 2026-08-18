#ifndef __SYSTIC_H
#define __SYSTIC_H
#include "VTS.h"
#include "Utilities.h"
#include "GPRS.h"
#include "LOG.h"
#include "Hardware.h"


#define MAX_THREADS 10 // Maximum number of threads

typedef struct OSThread
{
	s32 taskEnable;
	s32 taskId;
	/* volatile: written by the owning thread in ThreadSleep() and read by the
	 * HeartBeat() timer callback, which only wakes threads it sees as WAITING.
	 * A stale read here means the thread gets no wake message and blocks in
	 * Ql_OS_GetMessage() indefinitely, producing no log output at all. */
	volatile s32 taskState;
	char taskName[32];
	s32 taskPriority;
} OSThread;

typedef struct ThreadSystem
{
	uint8_t ThreadCount;
	OSThread Thread[MAX_THREADS]; // Pointer to an array of pointers to OSThread objects

} ThreadSystem;

extern ThreadSystem HyperThread;
uint8_t InitializeThread(OSThread *Thread);
	

extern u32 Heartbeat_timer;

void InitSystic(void);
void ThreadSleep(uint32_t ms);



#define HEARTBEAT_DELAY_RES    10// 10
#define HEARTBEAT_MSG_ID 99

#define TASK_STATE_NORMAL 0
#define TASK_STATE_WAITING 1

#define	NEIGHBOURCELL_TIME				180
#define SERVER_HANG_TIME				15
/* Seconds the server thread may go without completing a loop iteration before
 * the device is reset. Counted at 1 Hz in Systic_Event_1s(). */
#define SERVER_THREAD_HANG_SECS			600
/* Longest the watchdog stays suppressed for an in-flight FTP/FOTA or MCU
 * firmware transfer. Bounded on purpose — see ProcessServerThreadTimeout(). */
#define SERVER_FTP_EXEMPT_SECS			2700
#define MCU_HANG_TIME                   10
#define ALIVE_PKT_TIME                  3
#define PRF_TIMEOUT 				(10*60)


void SysticThreadEntry(s32 taskId);

#endif // __SYSTIC_H
