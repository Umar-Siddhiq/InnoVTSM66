#include "Systic.h"
#include "MCU.h"
#include "Alert.h"
#include "Hardware.h"
#include "GPS.h"
#include "LEDManager.h"
#include "Sensors.h"
#ifdef PROTO_CDAC
#include "HTTP.h"
#endif

u32 Heartbeat_timer = 0x100;
ThreadSystem HyperThread= {0};
void InitSysticthread(void);

volatile TickTypeDef IntervalTick={0};
PacketReadyTypedef IsPacketReady={0};
volatile uint8_t oc=0, ServerThreadTimeout=0, HourlyResetCount=0;
volatile uint16_t GSMRegTimeout=0;
#ifdef PROTO_CDAC
VehicleTypeDef VehicleState = {0};
volatile uint32_t HaltCounter = 0;
volatile uint32_t SleepCounter = 0;
char VehicleMovingMode = 'H';
#endif

uint8_t InitializeThread(OSThread *Thread)
{
    if (HyperThread.ThreadCount >= MAX_THREADS)
        return 0; // Fail, max reached

    memcpy((void*)&HyperThread.Thread[HyperThread.ThreadCount],(void*)Thread,sizeof(OSThread));
    Thread->taskEnable = 1;
    LOGData(TAG_HYPER, "Thread %s initialized. TaskId: %d, taskIndex: %d", Thread->taskName, Thread->taskId,HyperThread.ThreadCount);
    HyperThread.ThreadCount++;
    return 1;
}

void ThreadSleep(uint32_t ms)// called by other Threads
{
    ST_MSG msg = {0};
    s32 currentTaskId = Ql_OS_GetActiveTaskId();
    if(ms < HEARTBEAT_DELAY_RES)
        ms = HEARTBEAT_DELAY_RES;
    OSThread *thread = NULL;

    for (int i = 0; i < HyperThread.ThreadCount; i++)
    {
        if (HyperThread.Thread[i].taskId == currentTaskId)
        {
            thread = &HyperThread.Thread[i];
            break;
        }
    }

    if (!thread)
    {
        LOGData(TAG_SYSTIC, "ThreadSleep: Active thread not registered! TaskId: %d", currentTaskId);
        return;
    }

    thread->taskState = TASK_STATE_WAITING;

    while (ms > 0)
    {
        Ql_OS_GetMessage(&msg);
        if (msg.message == HEARTBEAT_MSG_ID)
        {
            if (ms > HEARTBEAT_DELAY_RES)
                ms -= HEARTBEAT_DELAY_RES;
            else
                break;
        }
        else
            continue; // Ignore other messages
    }

    thread->taskState = TASK_STATE_NORMAL;
}

void HeartBeat(u32 timerId, void* param)
{
    static int mainCounter = 0;
    if(mainCounter++ >= 300)
    {
        mainCounter = 0;
        Ql_OS_SendMessage(main_task_id, HEARTBEAT_MSG_ID, 0, 0);
    }
    
    for (int i = 0; i < HyperThread.ThreadCount; i++)
    {
        OSThread *t = &HyperThread.Thread[i];
        if (t->taskEnable && t->taskState==TASK_STATE_WAITING)
        {
            Ql_OS_SendMessage(t->taskId, HEARTBEAT_MSG_ID, 0, 0);
        }
    }
}

void InitSystic(void)
{
    s32 ret;
    ret = Ql_Timer_Register(Heartbeat_timer, HeartBeat, NULL);
    if (ret != QL_RET_OK)
    {
        LOGData(TAG_SYSTIC, "Failed to register heartbeat timer. Error: %d", ret);
    }
    else
    {
        ret = Ql_Timer_Start(Heartbeat_timer, HEARTBEAT_DELAY_RES, TRUE);
        if (ret != QL_RET_OK)
        {
            LOGData(TAG_SYSTIC, "Failed to start heartbeat timer. Error: %d", ret);
        }
        else
        {
            LOGData(TAG_SYSTIC, "Heartbeat timer started successfully.");
        }
    }
}
volatile u64 stime=0;
volatile unsigned long msprev100ms=0 ,msprevSec=0 , msprevMin=0;


void UpdateTick(void)
{   
    #ifdef AUTO_SLEEP_ENABLE
    // Auto-sleep timer increment: Track ignition off duration (called every 1 second)
    if(!SleepConfig.IsEnabled && !PeriPheralVal.IGN)
    {
        SleepConfig.IgnOffTimer++;  // Increment timer when ignition is OFF
    }
    #endif
    
    #ifndef PROTO_CDAC
    if(SensorsConfig.isEnabled)
    {
        uint16_t sensorInterval = GetCurrentSensorInterval();
        if(sensorInterval > 0)
        {
            LOGData(TAG_SYSTIC, "Sensor Interval: %d, Current Tick: %d", sensorInterval, IntervalTick.SensTick);
            
            // Query CLS sensor 3 seconds before packet is due (gives time for response)
            // Only query if interval is greater than 5 seconds
            if(sensorInterval >= 5 && IntervalTick.SensTick == (sensorInterval - 3))
            {
                QueryCLSSensor();
            }
            
            if(++IntervalTick.SensTick >= sensorInterval)
            {
                IsPacketReady.IsSensPacket = 1;
                IntervalTick.SensTick = 0;
            }
        }
    }

	if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-1)
	{
		IsPacketReady.IsNormalPacket=1;
		IntervalTick.NormalTick=0;
	}
	else
		IntervalTick.NormalTick++;


    if(IntervalTick.HistoryTick >= 3)
    {
        IsPacketReady.IsHistoryPacket=1;
        IntervalTick.HistoryTick=0;
    }
    else    
        IntervalTick.HistoryTick++;
//	if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval)
//	{
//		IsPacketReady.IsFullPacket=1;
//		IntervalTick.FullTick=0;
//	}
//	else
//		IntervalTick.FullTick++;
	if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval)
	{
		IsPacketReady.IsHealthPacket=1;
		IntervalTick.HealthTick=0;
	}
	else
		IntervalTick.HealthTick++;
//	if(IntervalTick.CriticalTick >= VTSData.IntervalData.EnergencyInterval-2)
//	{
//		IsPacketReady.IsCriticalPacket=1;
//		IntervalTick.CriticalTick=0;
//	}
//	else
//		IntervalTick.CriticalTick++;
    #else
    if(SOS.IsSOS && (IntervalTick.CriticalTick >= VTSData.IntervalData.EnergencyInterval-CONNTECTION_PRETIME))
        HTTPConnectFlag=1;
    else
    {
        if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
        if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
        if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
    }
    if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-1)
	{
		IsPacketReady.IsNormalPacket=1;
		IntervalTick.NormalTick=0;
	}
	else
		IntervalTick.NormalTick++;
	if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval-1)
	{
		IsPacketReady.IsFullPacket=1;
		IntervalTick.FullTick=0;
	}
	else
		IntervalTick.FullTick++;

	if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval-1)
	{
		IsPacketReady.IsHealthPacket=1;
		IntervalTick.HealthTick=0;
	}
	else
		IntervalTick.HealthTick++;

	if(IntervalTick.CriticalTick >= VTSData.IntervalData.EnergencyInterval-1)
	{
		IsPacketReady.IsCriticalPacket=1;
		IntervalTick.CriticalTick=0;
	}
	else
		IntervalTick.CriticalTick++;
    #endif
    if(GSM.IsNeighbourCells)
    {
        if(IntervalTick.NeighbourTick >= NEIGHBOURCELL_TIME)
        {
            IsPacketReady.IsGSMNeighbour=1;
            IntervalTick.NeighbourTick=0;
        }
        else
            IntervalTick.NeighbourTick++;
    }
    else
    {
        if(IntervalTick.NeighbourTick >= 20)
        {
            IsPacketReady.IsGSMNeighbour=1;
            IntervalTick.NeighbourTick=0;
        }
        else
            IntervalTick.NeighbourTick++;   
    }
    
    #ifdef PRF_AUTOSWITCH 
    #ifndef PROTO_CDAC
    if(ServerSocket[0].SocketState != SOCKET_CONNECTED && !SleepConfig.IsEnabled && ServerSocket[1].SocketState!=SOCKET_CONNECTED 
        && ServerSocket[2].SocketState!=SOCKET_CONNECTED && FTPState != FTP_STATE_CONNECTED && GSM.GSMState>=SIM_DETECTED)
    #else
    if(HTTPState!=HTTP_STATE_SET && !SleepConfig.IsEnabled && ServerSocket[1].SocketState!=SOCKET_CONNECTED 
        && ServerSocket[2].SocketState!=SOCKET_CONNECTED && FTPState != FTP_STATE_CONNECTED && GSM.GSMState>=SIM_DETECTED)
    #endif
    {
        uint32_t current_timeout = PRF_TIMEOUT; // Default to 10 minutes (600s)
        #ifdef SIM_PROFILE_AIRTEL
        if (VTSState.CurrentProfile == SIM_PROFILE_AIRTEL)
        {
            current_timeout = 300; // Airtel gets 5 minutes
        }
        #endif
        #ifdef SIM_PROFILE_BSNL
        else if (VTSState.CurrentProfile == SIM_PROFILE_BSNL)
        {
            current_timeout = 120; // BSNL gets 2 minutes
        }
        #endif
        #ifdef SIM_PROFILE_VI
        else if (VTSState.CurrentProfile == SIM_PROFILE_VI)
        {
            current_timeout = 600; // VI gets 10 minutes
        }
        #endif

        if(++IntervalTick.ProfileChangeCount >= current_timeout)
        {
            IntervalTick.ProfileChangeCount = 0;
            if(ZigTestMode)
            {
                LOGData(TAG_SYSTIC,"ZigTestMode Active - Automatic profile switch prevented");
                
            }
            else
            {
                #ifdef AUTO_PROFILESWITCH_DISABLE
                LOGData(TAG_SYSTIC, "Auto Profile Switch Disabled - Skipping profile switch on connection timeout");
                #else
                // Get next valid profile intelligently
                prfReq = GetNextValidProfile(VTSState.CurrentProfile);
                UpdateStateInFlash();

                LOGData(TAG_SYSTIC, "GSM Connection Timeout, Switching to Profile %d", prfReq);
                SendRS232Response("****GSM Connection Timeout, Switching Profile ****\n");
                #endif
            }
        }
        else
            LOGData(TAG_SYSTIC,"Profile connection timeout in  %d/%d",IntervalTick.ProfileChangeCount,current_timeout);
    }
    else
    IntervalTick.ProfileChangeCount=0;
    #endif

    #define PRINTF
    #ifdef PRINTF

    #ifdef PROTO_CDAC
    LOGData(TAG_SYSTIC,"NTick: %d/%d  CTick: %d/%d  HTick: %d/%d  FTick: %d/%d  Vehicle Mode: %c",
                           IntervalTick.NormalTick, VTSData.IntervalData.CurrentInterval,
                           IntervalTick.CriticalTick, VTSData.IntervalData.EnergencyInterval, 
                           IntervalTick.HealthTick,VTSData.IntervalData.HealthInterval,
                           IntervalTick.FullTick,VTSData.IntervalData.FullDataPacketInterval,VehicleMovingMode);
    #else
    LOGData(TAG_SYSTIC,"Normal Tick: %d / %d\nHealth Tick: %d / %d\n",
                           IntervalTick.NormalTick, VTSData.IntervalData.CurrentInterval,
                           IntervalTick.HealthTick, VTSData.IntervalData.HealthInterval);
    #endif
    if(SOS.IsSOS)
    {
        LOGData(TAG_SYSTIC,"SOS Tick: %d / %d", SOS.SOSTimeLasped, SOS.SOSTimeOut);
    }
    #endif

}
#ifdef PROTO_CDAC
void UpdateVehicle(void)
{
	if(GPS.Speed > 3.0)
	{
		if((VehicleState.VehicleMode==HALT) || (VehicleState.VehicleMode==SLEEP))
		{
//			HaltCounter++;
//			if(HaltCounter >= VTSData.IntervalData.HaltTime)
//			{
				VehicleState.VehicleMode=MOTION;
				VehicleMovingMode='M';
				VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.MotionInterval;
				HaltCounter=0;
		//	}
		}
	}
	else
	{
		if(VehicleState.VehicleMode==MOTION)
		{
			HaltCounter++;
			if(HaltCounter >= VTSData.IntervalData.HaltTime)
			{
				VehicleState.VehicleMode=HALT;
				VehicleMovingMode='H';
				SleepCounter=0;
				VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.HaltInterval;
				HaltCounter=0;
			}
		}
		if(VehicleState.VehicleMode==HALT)
		{
			SleepCounter++;
			if(SleepCounter >= VTSData.IntervalData.SleepTime)
			{
				VehicleState.VehicleMode=SLEEP;
				VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.SleepInterval;
				VehicleMovingMode='S';
				SleepCounter=0;
			}
		}
	}
	
}
#endif

void Systic_Event_100ms(void)
{
    static uint8_t ledCounter = 0;
    
    //100 ms
    hw_led_process();
    
    // Update LED Manager every 200ms (every 2nd call)
    if (++ledCounter >= 2)
    {
        LEDManager_Process();
        ledCounter = 0;
    }
    
    msprev100ms = stime;
}

void Systic_Event_1s(void)
{
    msprevSec=stime;
    UpdateTick();
    CheckGPSAlerts();
}

void Systic_Event_1m(void)
{
     // 1 Min
    msprevMin = stime;

}

void SysticThreadEntry(s32 taskId)
{
    InitSysticthread();
    ThreadSleep(300);
    while (1)
    {
        stime = Ql_GetMsSincePwrOn();
        if((stime-msprev100ms > 96))
            Systic_Event_100ms();
        if((stime-msprevSec) > 996)
            Systic_Event_1s();
        if((stime-msprevMin) > (1000*60)-4)
            Systic_Event_1m();
        ThreadSleep(20);
    }
}

void InitSysticthread(void)
{
    s32 ret;
    OSThread Systic_Thread = {0};
    Systic_Thread.taskId = Ql_OS_GetActiveTaskId();
    strcpy(Systic_Thread.taskName, "Systic Thread");
    Systic_Thread.taskEnable = 1;
    Systic_Thread.taskState = TASK_STATE_NORMAL;
    Systic_Thread.taskPriority = 1;
    ret = InitializeThread(&Systic_Thread);
    if (ret != 1)
    {
        LOGData(TAG_SYSTIC, "Failed to initialize Systic thread");
        return;
    }
}













