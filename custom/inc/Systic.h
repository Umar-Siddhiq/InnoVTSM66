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
	s32 taskState;
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
#define MCU_HANG_TIME                   10
#define ALIVE_PKT_TIME                  3
#define PRF_TIMEOUT 				(10*60)


void SysticThreadEntry(s32 taskId);

#endif // __SYSTIC_H
