#include "Hardware.h"
#include "Systic.h"
#include "Alert.h"
#include "SOS.h"
#include "File.h"
#include "ril_custom.h"
#include "MCU.h"
#include "ql_power.h"
#include "GPRS.h"
#include "SMS.h"
#include "Geofence.h"
#include "LEDManager.h"
#ifdef PROTO_CDAC
#include "HTTP.h"
#endif

// External variables for RS232 functions
extern STKDatatypedef STKdata;

static uint32_t LED_GlobalTick = 0;   
LEDSystemTypedef LEDSystem = {0};
PepheralTypedef PeriPheralVal={0};
SleepConfigTypedef SleepConfig = {0};


uint8_t RFIDData[RFID_MAX_DATALEN]={0};
uint16_t RFIDDataCount = 0;
int GSMLED = 0,GPSLED = 0,BATTERYLED =0, SOSLED = 0;
uint8_t PrevIgn = 0, PrevBLow = 0, PrevMain = 0, PrevTamp = 0;

// RS232 Response Buffer
#define RS232_RESPONSE_BUFFER_SIZE 350
static char RS232ResponseBuffer[RS232_RESPONSE_BUFFER_SIZE] = {0};
static volatile uint8_t IsRS232ResponsePending = 0;


void hw_led_struct_init(void)
{
    LEDSystem.LEDCount = 0;
    for (int i = 0; i < MAX_LED_COUNT; i++)
    {
        LEDSystem.LEDState[i].IsEnabled = 0;
        LEDSystem.LEDState[i].ONTime = 0;
        LEDSystem.LEDState[i].TotalTime = 0;
        LEDSystem.LEDState[i].PinName = 0;
        LEDSystem.LEDState[i].PhaseOffset = 0;
    }
    LED_GlobalTick = 0;
}

void hw_led_process(void)
{
    LED_GlobalTick++; // advance global tick (should be called at fixed interval, e.g. 10ms)

    for (int i = 0; i < LEDSystem.LEDCount; i++)
    {
        if (LEDSystem.LEDState[i].IsEnabled)
        {
            uint16_t ONTime     = LEDSystem.LEDState[i].ONTime;
            uint16_t TotalTime  = LEDSystem.LEDState[i].TotalTime;
            uint16_t PhaseOffset= LEDSystem.LEDState[i].PhaseOffset;

            if (TotalTime == 0)   // avoid division by zero
                continue;

            // Calculate phase using global tick
            uint16_t phase = (LED_GlobalTick + PhaseOffset) % TotalTime;

            if (phase < ONTime)
                Ql_GPIO_SetLevel(LEDSystem.LEDState[i].PinName, PINLEVEL_HIGH); // ON
            else
                Ql_GPIO_SetLevel(LEDSystem.LEDState[i].PinName, PINLEVEL_LOW);  // OFF
        }
    }
}

int hw_led_add(int PinName, uint16_t ONTime, uint16_t TotalTime)
{
    if (LEDSystem.LEDCount >= MAX_LED_COUNT)
        return -1; // Fail, max reached

    LEDSystem.LEDState[LEDSystem.LEDCount].PinName = PinName;
    s32 ret = Ql_GPIO_Init(PinName, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_DISABLE);
    if(QL_RET_OK != ret)
    {
        LOGData(TAG_HARDWARE,"Failed to initialize GPIO %d for LED %d, ret=%d", PinName, LEDSystem.LEDCount, ret);
        return -1;
    }
    else
    {
        LOGData(TAG_HARDWARE,"GPIO %d initialized for LED %d", PinName, LEDSystem.LEDCount);
    }

    LEDSystem.LEDState[LEDSystem.LEDCount].IsEnabled = 1;
    LEDSystem.LEDState[LEDSystem.LEDCount].ONTime    = ONTime;
    LEDSystem.LEDState[LEDSystem.LEDCount].TotalTime = TotalTime;
    LEDSystem.LEDState[LEDSystem.LEDCount].PhaseOffset = 0; // default sync with global
    return LEDSystem.LEDCount++;
}

void hw_led_init(void)
{
    int ret=-1;
    hw_led_struct_init();

    ret = hw_led_add(LED_GSM_GPIO, 0, 10);
    if (ret < 0) LOGData(TAG_HARDWARE,"Failed to add GSM LED\n");
    else GSMLED = ret;
    
    ret = hw_led_add(LED_GPS_GPIO, 0, 10);
    if (ret < 0) LOGData(TAG_HARDWARE,"Failed to add GPS LED\n");
    else GPSLED = ret;
    
    ret = hw_led_add(LED_BATTERY_GPIO, 0, 10);
    if (ret < 0) LOGData(TAG_HARDWARE,"Failed to add Battery LED\n");
    else BATTERYLED = ret;

    ret = hw_led_add(LED_SOS_GPIO, 0, 10);
    if (ret < 0) LOGData(TAG_HARDWARE,"Failed to add SOS LED\n");
    else SOSLED = ret;
}

uint8_t hw_led_state_set(int LED, uint8_t State, uint16_t ONTime, uint16_t TotalTime)
{
    if (LED < 0 || LED >= LEDSystem.LEDCount)
    {
        LOGData(TAG_HARDWARE,"Invalid LED index: %d", LED);
        return 0; // Invalid index
    }

    if(ONTime > TotalTime)
    {
        LOGData(TAG_HARDWARE,"Invalid LED Time : ONT : %d, TT: %d", ONTime, TotalTime);
        return 0; 
    }

    if (State == 1) // Enable LED
    {
        LEDSystem.LEDState[LED].IsEnabled   = 1;
        LEDSystem.LEDState[LED].ONTime      = ONTime;
        LEDSystem.LEDState[LED].TotalTime   = TotalTime;
        LEDSystem.LEDState[LED].PhaseOffset = 0; // reset phase when re-enabled
    }
    else // Disable LED
    {
        LEDSystem.LEDState[LED].IsEnabled = 0;
        Ql_GPIO_SetLevel(LEDSystem.LEDState[LED].PinName, PINLEVEL_LOW);
    }
    return 1;
}

// Battery voltage to percentage lookup table for 3.7V Li-Ion battery
// Based on typical discharge curve for single-cell Li-Ion (3.0V-4.2V range)
typedef struct {
    float voltage;
    uint8_t percentage;
} BatteryLookupEntry;

static const BatteryLookupEntry batteryLookupTable[] = {
    {4.20, 100},
    {4.15, 95},
    {4.11, 90},
    {4.08, 85},
    {4.02, 80},
    {3.98, 75},
    {3.95, 70},
    {3.91, 65},
    {3.87, 60},
    {3.85, 55},
    {3.84, 50},
    {3.82, 45},
    {3.80, 40},
    {3.79, 35},
    {3.77, 30},
    {3.75, 25},
    {3.73, 20},
    {3.71, 15},
    {3.69, 10},
    {3.61, 5},
    {3.27, 0}
};

#define BATTERY_TABLE_SIZE (sizeof(batteryLookupTable) / sizeof(BatteryLookupEntry))

float batteryPercentageToVoltage(int percentage)
{
    if (percentage <= 0)
    {
        return batteryLookupTable[BATTERY_TABLE_SIZE - 1].voltage;
    }

    if (percentage >= 100)
    {
        return batteryLookupTable[0].voltage;
    }

    for (int i = 0; i < BATTERY_TABLE_SIZE - 1; i++)
    {
        uint8_t highPercent = batteryLookupTable[i].percentage;
        uint8_t lowPercent = batteryLookupTable[i + 1].percentage;

        if (percentage <= highPercent && percentage >= lowPercent)
        {
            float percentSpan = (float)(highPercent - lowPercent);
            float voltageSpan = batteryLookupTable[i].voltage - batteryLookupTable[i + 1].voltage;
            float offset = (float)(percentage - lowPercent);

            if (percentSpan <= 0.0f)
            {
                return batteryLookupTable[i].voltage;
            }

            return batteryLookupTable[i + 1].voltage + ((offset / percentSpan) * voltageSpan);
        }
    }

    return LOW_BAT_THRS_VOLT;
}

int batteryVoltageToPercentage(float voltage)
{
    return (int)GetBatteryPercentage(voltage);
}

uint8_t GetBatteryPercentage(float voltage)
{
    // Handle out of range voltages
    if (voltage >= batteryLookupTable[0].voltage)
    {
        return 100; // Fully charged or overcharged
    }
    
    if (voltage <= batteryLookupTable[BATTERY_TABLE_SIZE - 1].voltage)
    {
        return 0; // Depleted
    }
    
    // Linear interpolation between lookup table entries
    for (int i = 0; i < BATTERY_TABLE_SIZE - 1; i++)
    {
        if (voltage >= batteryLookupTable[i + 1].voltage && 
            voltage <= batteryLookupTable[i].voltage)
        {
            // Found the range, do linear interpolation
            float voltage_diff = batteryLookupTable[i].voltage - batteryLookupTable[i + 1].voltage;
            float percent_diff = batteryLookupTable[i].percentage - batteryLookupTable[i + 1].percentage;
            float voltage_from_lower = voltage - batteryLookupTable[i + 1].voltage;
            
            float interpolated = batteryLookupTable[i + 1].percentage + 
                                (voltage_from_lower / voltage_diff) * percent_diff;
            
            return (uint8_t)(interpolated + 0.5); // Round to nearest integer
        }
    }
    
    // Fallback (should not reach here)
    return 50;
}


static void Callback_OnADCSampling(Enum_ADCPin adcPin, u32 adcValue, void *customParam)
{
    PeriPheralVal.BattVolt = (double)adcValue / 1000.0; // Convert to volts
    PeriPheralVal.BattVolt = PeriPheralVal.BattVolt * 2; // Adjust for voltage divider
    LOGData(TAG_HARDWARE,"ADC Sampling: %d mV, BattVolt: %.2f V", adcValue, PeriPheralVal.BattVolt);
    PeriPheralVal.BattPerc = GetBatteryPercentage(PeriPheralVal.BattVolt);
}

void hw_charger_init(void)
{
    CONTROL_CHARGER_INIT;
    Ql_ADC_Register(ADC_VBAT_GPIO, Callback_OnADCSampling, NULL);
    Ql_ADC_Init(ADC_VBAT_GPIO, 10, 200);
    Ql_ADC_Sampling(ADC_VBAT_GPIO, TRUE); // Start ADC sampling
}

  


void hw_io_init(void)
{
    OUTPUT_1_INIT;
    OUTPUT_2_INIT;
    OP2_Set(0); // Ensure OP2 is off by default
    OP1_Set(0); // Ensure OP1 is off by default
}

void hw_init(void)
{
    hw_io_init();
    hw_led_init();
    hw_charger_init();
    LEDManager_Init();  // Initialize LED Manager
}

void CheckGPSAlerts(void)
{
    static uint8_t PrevFix=0;
    static float PrevSpeed=0.0f, PrevHeading=0.0f;
    if(!GPS.GPSFix)
    {
        PrevFix=0;
        return;
    }
    if(!PrevFix){
        PrevFix=1;
        PrevSpeed = GPS.Speed;
        PrevHeading = GPS.Heading;
        return;
    }

    float deltaSpeed = GPS.Speed-PrevSpeed;
    float deltaHeading = GPS.Heading - PrevHeading;

    if (deltaHeading > 180.0f) {
        deltaHeading -= 360.0f;
    } else if (deltaHeading < -180.0f) {
        deltaHeading += 360.0f;
    }

    if(deltaHeading < 0)
        deltaHeading *= -1;

    
    if(deltaSpeed > VTSData.VehicleData.HarshAcc/100){
        VAlert[HARSH_ACC_ALERT].Enable=1;
        AddAlert(HARSH_ACC_ALERT);
        LOGData(TAG_SYSTIC,"********* GPS HARSH ACCEL ALERT (dS=%.2f > %.2f)********\n", deltaSpeed, (float)VTSData.VehicleData.HarshAcc);
        //SendRS232String("\r\n ********* GPS HARSH ACCEL ALERT********\n");
    }
    

    if(deltaSpeed < -1 * VTSData.VehicleData.HarshBreak/50){
        VAlert[HARSH_BRK_ALERT].Enable=1;
        AddAlert(HARSH_BRK_ALERT);
        LOGData(TAG_SYSTIC,"********* GPS HARSH BRAKE ALERT (dS=%.2f < %.2f)********\n", deltaSpeed, -1.0f * VTSData.VehicleData.HarshBreak);
        //SendRS232String("\r\n ********* GPS HARSH BRAKE ALERT********\n");
    }

    if((deltaHeading > VTSData.VehicleData.RashTurn) && GPS.Speed > 20){
        VAlert[RASH_TURN_ALERT].Enable=1;
        AddAlert(RASH_TURN_ALERT);
        LOGData(TAG_SYSTIC,"********* GPS RASH TURN ALERT********\n");
        //SendRS232String("\r\n ********* GPS RASH TURN ALERT********\n");
    }
    
    if(deltaHeading > 30 && GPS.Speed > 10){
        IntervalTick.NormalTick=1; // Force send normal data once
    }

    PrevSpeed = GPS.Speed;
    PrevHeading = GPS.Heading;
}
void SystemStateSend(void)
{
    char ss[250], str[20];
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif

    Ql_sprintf(ss,"%s, FV_%s_%s, State: ",NetWork.IMEI,PROTO_TAG,FIRMWAREVERSION);

    #ifndef HTTP_SIMULATE
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
    {
        Ql_strcat(ss,"Server OK\n");
    }
    #else
    if(HTTPState == HTTP_STATE_SET)
    {
        Ql_strcat(ss,"Server OK\n"); 
    }
    #endif
    else
    {
        switch (GSM.GSMState)
        {
        case SIM_NOT_DETECTED:
            Ql_strcat(ss,"NO SIM\n");
            break;
        case SIM_DETECTED:
            Ql_strcat(ss,"Sim OK\n");
            break;
        case GPRS_INIT:
            Ql_strcat(ss,"Sim REG\n");
            break;
        default:
            Ql_strcat(ss,"GPRS OK\n");
            break;
        }
    }
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        Ql_strcat(ss,"Prof: ");
        
        Ql_sprintf(str,"%d",VTSState.CurrentProfile);
        Ql_strcat(ss,str);
        Ql_strcat(ss,", ccid: ");
        Ql_strcat(ss,NetWork.SIMNo);
        Ql_strcat(ss,", csq: ");
        Ql_sprintf(str,"%d",GSM.SignalStrength);
        Ql_strcat(ss,str);
    }
    if(GSM.GSMState>SIM_DETECTED)
    {
        Ql_strcat(ss,", SPN: ");
        Ql_strcat(ss,NetWork.Network);
    }
    if(GPS.State == 0)
    {
        Ql_strcat(ss,", GPS: FLT\n");
    }
    else
    {
        Ql_strcat(ss,", GPS: OK");
        Ql_sprintf(str,", fix: %d",GPS.GPSFix);
        Ql_strcat(ss,str);
    }
    if(SOS.IsSOS)
    {
        Ql_strcat(ss,",SOS: ON");
    }
    else
    {
        Ql_strcat(ss,",SOS: OFF");
    }
    Ql_strcat(ss,"\r\n");
    MCOMM_SendSerial(0,(uint8_t*)ss, Ql_strlen(ss));

}

uint8_t SendCFUNAT(uint8_t mode, uint8_t reset)
{
    char strAT[200];
    char responseBuffer[200];
    LOGData(TAG_HARDWARE,"Sending CFUN AT Command: mode=%d, reset=%d", mode, reset);
    Ql_sprintf(strAT,"AT+CFUN=%d,%d\r\n",mode, reset);
    int ret = SendATCommandSimple(strAT, responseBuffer, sizeof(responseBuffer), 5000);
    if(ret == RIL_AT_SUCCESS)
    {
        LOGData(TAG_HARDWARE,"Sent CFUN AT Command: %s", strAT);
        return 1;
    }
    else
    {
        LOGData(TAG_HARDWARE,"Failed to send CFUN AT Command: %s", strAT);
        return 2;
    }
    ThreadSleep(500);
}

uint8_t SleepModeON(uint32_t SleepTime)
{
    if(SleepConfig.IsEnabled)
    {
        LOGData(TAG_HARDWARE,"SleepMode Already ON");
        return 0;
    }
    if(SleepTime < 60)
    {
        LOGData(TAG_HARDWARE,"SleepMode Time too Low");
        return 0;
    }
    SleepConfig.IsEnabled=1;
    SleepConfig.TimeRemaining=SleepTime;
    //GPS_RESET_ON;
    Ql_ADC_Sampling(ADC_VBAT_GPIO, FALSE);
    GPS_UartSendString("$PAIR003*39\r\n");
    ThreadSleep(100);
    // GPS_UartSendString("$PAIR382,1*2E\r\n");
    // ThreadSleep(100);
    OUTPUT_1_OFF;
    OUTPUT_2_OFF;
    TCP_CloseALLSockets();
    Ql_GPRS_Deactivate(0);
    CONTROL_CHARGER_OFF;
    GSM.GSMState = GPRS_INIT;
    Ql_UART_Close(GPS_UART_PORT);
    MCOMM_SendSleep(SleepTime);
    Ql_UART_Close(MCOMM_UART_PORT);
    SendCFUNAT(4,0); // Airplane Mode
    ThreadSleep(1000);
    if(Ql_SleepEnable()==QL_RET_OK)
    {
        LOGData(TAG_HARDWARE,"SleepMode Enabled");

        return 1;
    }
    //Test
    //Ql_PowerDown(1);
    LOGData(TAG_HARDWARE,"SleepMode Enable Fail");
    return 0;
}

uint8_t SleepModeOFF(void)
{
    if(!SleepConfig.IsEnabled)
    {
        LOGData(TAG_HARDWARE,"SleepMode Already OFF");
        return 0;
    }
    SleepConfig.IsEnabled=0;
    //GPS_RESET_OFF;
    
   
    SendCFUNAT(1,0); // Full Functionality without Reset
    ThreadSleep(1000);
    Ql_UART_Open(MCOMM_UART_PORT, MCOMM_UART_BAUDRATE, FC_NONE);
    Ql_UART_Open(GPS_UART_PORT, GPS_UART_BAUDRATE, FC_NONE);
    GPS_UartSendString("$PAIR004*3E\r\n");
    ThreadSleep(100);
    MCOMM_SendSleep(0); // once to wake up MCU
    ThreadSleep(500);
    MCOMM_SendSleep(0); // again to reset sleep timer
    ThreadSleep(1000);
    // GPS_UartSendString("$PAIR382,0*2F\r\n");
    // ThreadSleep(100);
    if(Ql_SleepDisable()==QL_RET_OK)
    {
        LOGData(TAG_HARDWARE,"SleepMode Disabled");
        Ql_Reset(0);
        return 1;
    }
    LOGData(TAG_HARDWARE,"SleepMode Disable Error");
    return 0;
}

void Periphereal_init(void)
{
    PeriPheralVal.IGN = INPUT_IGNITION_VAL;
    PrevIgn = PeriPheralVal.IGN;
    PrevMain = VTSState.IsPrevMain;
}




void HardwareThreadEntry(s32 taskId)
{
    static uint8_t mcufetchcount = 0, SOSProcessCount = 0, SleepModeCount = 0;
    static uint8_t rs232_count = 0;  // Counter for RS232 outputs
    static uint8_t rs232_message_index = 0;  // Which message to send (0, 1, or 2)
   
    hardware_thread_init(taskId);
    ThreadSleep(3000);
    
    Periphereal_init();
    MCOMM_FetchPerihperal();
	while(1)
    {
        ProcessBLE();
        UpdateGeoFence();
        while(prfReq!=NONE)
		{
			LOGData(TAG_SERVER,"Hardware Thread Paused For Profile Update");
			ThreadSleep(500);
		}
        PeriPheralVal.IGN = INPUT_IGNITION_VAL;
        PeriPheralVal.OP1 = OUTPUT_1_VAL;
        PeriPheralVal.OP2 = OUTPUT_2_VAL;

        // Note: LED Manager now runs in Systic thread (100ms interval)
        // This ensures LEDs update even when GPRS thread blocks during profile switching

        #ifdef AUTO_SLEEP_ENABLE
        // Auto-sleep logic: Check if timer reached threshold (timer is incremented in Systic)
        if(!SleepConfig.IsEnabled)  // Only check if not already in sleep mode
        {
            if(!PeriPheralVal.IGN)  // Ignition is OFF
            {
                // Check if ignition has been off for 2 minutes (120 seconds)
                if(SleepConfig.IgnOffTimer >= 500)
                {
                    LOGData(TAG_HARDWARE, "Auto-sleep triggered: Ignition off for 2 minutes");
                    SleepModeON(60*60);  // Enter sleep mode for 30 minutes (in seconds)
                    SleepConfig.IgnOffTimer = 0;  // Reset timer
                }
            }
            else  // Ignition is ON
            {
                SleepConfig.IgnOffTimer = 0;  // Reset timer when ignition is on
            }
        }
        #endif

        if(SleepConfig.IsEnabled)
        {
            if(SleepModeCount++ >= 5)
            {
                if(--SleepConfig.TimeRemaining <= 0 || PeriPheralVal.IGN)
                {
                    SleepModeOFF();
                    #ifdef AUTO_SLEEP_ENABLE
                    SleepConfig.IgnOffTimer = 0;  // Reset auto-sleep timer when waking up
                    #endif
                }
                SleepModeCount=0;
            }
        }
        if(++SOSProcessCount >= 5)
        {
            SOSProcessCount = 0;
            ProcessSOS();
        }
        if(mcufetchcount++ >= 10 && !IsMotaProcessing && !SleepConfig.IsEnabled)
        {
            mcufetchcount = 0;
            MCOMM_FetchPerihperal();
        }
        if(GSM.GSMState>SIM_DETECTED && SleepConfig.IsEnabled==0)
        {
            if(IsPacketReady.IsGSMNeighbour==1)
            {
                RIL_GetQENGInfo(&GSM);
                IsPacketReady.IsGSMNeighbour=0;
            }
        }

        #ifdef IsBODYTAMP
        if(PeriPheralVal.AN2 < BodyThresh)
        {
            if(!PrevTamp)
            {
                LOGData(TAG_HARDWARE,"********* BODY TAMPER********\n");
                PrevTamp=1;
                VAlert[TAMPER_ALERT].Enable=1;
                AddAlert(TAMPER_ALERT);
            }
        }
        else
            PrevTamp=0;
        #endif

        //LOGData(TAG_HARDWARE,"BattVolt: %.2f, Adc : %d",PeriPheralVal.BattVolt, ADCVal);
        #ifndef PROTO_CDAC
        if(PeriPheralVal.IGN)
        {
            #warning sleep mode not implemented
            // if(IsSleepMode)
            // {
            //     SleepModeON();
                
            //     ThreadSleep(5000);
            // }

            if(!PrevIgn)
            {
                LOGData(TAG_HARDWARE,"********* IGNITION ON********\n");
                PrevIgn=1;
                VAlert[IGN_ON_ALERT].Enable = 1;
            }
        }
        else
        {
            if(PrevIgn)
            {
                LOGData(TAG_HARDWARE,"********* IGNITION OFF********\n");
                PrevIgn=0;
                VAlert[IGN_OFF_ALERT].Enable=1;
            }
        }
        #endif

        if(PeriPheralVal.BattVolt < VTSData.BattThrs)
        {
            if(!PrevBLow)
            {
                PrevBLow=1;
                // LED Manager will automatically handle battery LED
                VAlert[BATT_LOW_ALERT].Enable=1;
                AddAlert(BATT_LOW_ALERT);
                LOGData(TAG_HARDWARE,"********* BATTERY LOW ALERT********\n");
            }
        }
        else
        {
            if(PrevBLow)
            {
                PrevBLow=0;
                // LED Manager will automatically handle battery LED
                VAlert[BATT_LOW_ALERT].Enable=0;
                RemoveAlert(BATT_LOW_ALERT);
                LOGData(TAG_HARDWARE,"********* BATTERY LOW RESTORE ********\n");
                VAlert[BATT_LOW_RES_ALERT].Enable=1;
                AddAlert(BATT_LOW_RES_ALERT);
            }   
        }
        
        
            if(PeriPheralVal.MainsVolt > 6.0)
            {
                PeriPheralVal.IsMain = 1;
                if(!PrevMain)
                {
                    PrevMain=1;
                    VTSState.IsPrevMain=1;
                    UpdateStateInFlash();
                    VAlert[MAINS_FAIL_ALERT].Enable = 0;
                    RemoveAlert(MAINS_FAIL_ALERT);
                    VAlert[MAINS_RES_ALERT].Enable = 1;
                    AddAlert(MAINS_RES_ALERT);
                    LOGData(TAG_HARDWARE,"********* MAINS RESTORE ALERT********\n");
                }
            }
            else
            {
                PeriPheralVal.IsMain = 0;
                if(PrevMain)
                {
                    PrevMain=0;
                    VTSState.IsPrevMain=0;
                    UpdateStateInFlash();
                    VAlert[MAINS_FAIL_ALERT].Enable = 1;
                    AddAlert(MAINS_FAIL_ALERT);
                    VAlert[MAINS_RES_ALERT].Enable = 0;
                    RemoveAlert(MAINS_RES_ALERT);
                    LOGData(TAG_HARDWARE,"********* MAINS FAULT ALERT********\n");
                    #ifdef PROTO_CDAC
                    SMSAlert(3);
                    #endif
                }
            }
            if(SOS.IsSOS){
                OP1_Set(1);}
            else{
                OP1_Set(0);}

        // Send RS232 status messages every ~1 second (5 iterations * 200ms)
        #ifdef ENABLE_RS232_PRINT
        if(rs232_count++ >= 5  && !IsMotaProcessing && !SleepConfig.IsEnabled)
        {
            rs232_count = 0;
            
            // Prioritize pending response over status messages
            if(IsRS232ResponsePending)
            {
                SendBufferedRS232Response();
            }
            else
            {
                // Send rotating status message (no blocking wait for MCU response)
                // Now includes 4 messages in rotation
                switch(rs232_message_index % 4)
                {
                    case 0:
                        SystemInfoSend();
                        break;
                    case 1:
                        CellTowerInfoSend();
                        break;
                    case 2:
                        PeripheralInfoSend();
                        break;
                    case 3:
                        GPSDataSend();
                        break;
                }
                
                // Optionally send NMEA data (currently disabled)
                // SendNMEAToRS232();
                
                rs232_message_index++;
            }
        }
        #endif

        // if(SystemStateSendCount++ >= 15)
        // {
        //     SystemStateSend();
        //     SystemStateSendCount = 0;
        // }
        ThreadSleep(200);
    }
}

void hardware_thread_init(u32 taskId)
{
    s32 ret;
    OSThread Hardware_Thread = {0};
    Hardware_Thread.taskId = taskId;
    Ql_strcpy(Hardware_Thread.taskName, "Hardware Thread");
    Hardware_Thread.taskEnable = 1;
    Hardware_Thread.taskState = TASK_STATE_NORMAL;
    Hardware_Thread.taskPriority = 1;
    ret = InitializeThread(&Hardware_Thread);
    if (ret != 1)
    {
        LOGData(TAG_HARDWARE, "Failed to initialize Hardware thread");
        return;
    }
    LOGData(TAG_HARDWARE, "Hardware thread initialized successfully");
}

// RS232 Output Function Implementation
// Direct RS232 transmission for system status messages (without $RES formatting)
void SendRS232String(const char* message)
{
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    if (!message) return;
    
    // Direct transmission for status messages - no $RES formatting
    if (MCOMM_SendSerial(0, (uint8_t*)message, Ql_strlen(message)) != 1)
    {
        LOGData(TAG_HARDWARE, "Failed to send RS232 status message");
    }
}

/**
 * Queue RS232 Response - Store response to be sent in next rotation
 * @param response The response string to queue
 */
void QueueRS232Response(const char* response)
{
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    if(response == NULL || Ql_strlen(response) == 0)
        return;
    
    // Clear buffer and copy new response
    memset(RS232ResponseBuffer, 0, sizeof(RS232ResponseBuffer));
    Ql_strncpy(RS232ResponseBuffer, response, RS232_RESPONSE_BUFFER_SIZE - 1);
    IsRS232ResponsePending = 1;
    
    LOGData(TAG_HARDWARE, "RS232 Response Queued: %s", RS232ResponseBuffer);
}

/**
 * Send Buffered RS232 Response - Send queued response if available
 */
void SendBufferedRS232Response(void)
{
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    if(IsRS232ResponsePending && Ql_strlen(RS232ResponseBuffer) > 0)
    {
        SendRS232String(RS232ResponseBuffer);
        LOGData(TAG_HARDWARE, "RS232 Response Sent: %s", RS232ResponseBuffer);
        
        // Clear buffer after sending
        memset(RS232ResponseBuffer, 0, sizeof(RS232ResponseBuffer));
        IsRS232ResponsePending = 0;
    }
}

//$INF,imei,Firmare,GSMState(0=NoSIM,1=SimOK,2=SimREG,3=GPRSOK,4=SrvOK),Signal,SIMAKE,Currentprofile,Defaultprofile,Supportedprofile,SPN,CCID,IMSI,IP1:Port1,IP2:Port2,IP3:Port3,Url2\r\n
void SystemInfoSend(void)
{
    char ss[330], str[64];
    int gsmState = 0;
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif

    // Initialize buffers
    memset(ss, 0, sizeof(ss));
    memset(str, 0, sizeof(str));

    // Start with $INF header and IMEI
    Ql_sprintf(ss, "$INF,%s,", NetWork.IMEI);
    
    // Firmware version
    Ql_sprintf(str, "FQ_%s_%s,", PROTO_TAG, FIRMWAREVERSION);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    
    // GSM State - Convert to numeric code: 0=NoSIM, 1=SimOK, 2=SimREG, 3=GPRSOK, 4=ServerOK
    #ifndef HTTP_SIMULATE
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
    {
        gsmState = 4;
    }
    #else
    if(HTTPState == HTTP_STATE_SET)
    {
        gsmState = 4;
    }
    #endif
    else
    {
        switch (GSM.GSMState)
        {
        case SIM_NOT_DETECTED:
            gsmState = 0;
            break;
        case SIM_DETECTED:
            gsmState = 1;
            break;
        case GPRS_INIT:
            gsmState = 2;
            break;
        default:
            gsmState = 3;
            break;
        }
    }
    
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", gsmState);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    
    // Signal strength
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        memset(str, 0, sizeof(str));
        Ql_sprintf(str, "%d,", GSM.SignalStrength);
        Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
        
        // SIM Make
        Ql_strncat(ss, SIM_MAKE_STR, sizeof(ss) - Ql_strlen(ss) - 1);
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
        
        // Current Profile
        memset(str, 0, sizeof(str));
        Ql_sprintf(str, "%d,", VTSState.CurrentProfile);
        Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
        
        // Default Profile
        memset(str, 0, sizeof(str));
        Ql_sprintf(str, "%d,", VTSData.DefProfile);
        Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
        
        // Supported Profile
        memset(str, 0, sizeof(str));
        Ql_sprintf(str, "%d,", STKdata.profiles_supported);
        Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    }
    else
    {
        Ql_strncat(ss, ",,,,,", sizeof(ss) - Ql_strlen(ss) - 1); // Empty fields for signal, sim make, profiles
    }
    
    // SPN (Service Provider Name) - Truncate to 20 chars max
    if(GSM.GSMState > SIM_DETECTED)
    {
        char tempNet[21];
        memset(tempNet, 0, sizeof(tempNet));
        Ql_strncpy(tempNet, NetWork.Network, 20);
        Ql_strncat(ss, tempNet, sizeof(ss) - Ql_strlen(ss) - 1);
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    else
    {
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    
    // CCID - Truncate to 20 chars max
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        char tempSIM[21];
        memset(tempSIM, 0, sizeof(tempSIM));
        Ql_strncpy(tempSIM, NetWork.SIMNo, 20);
        Ql_strncat(ss, tempSIM, sizeof(ss) - Ql_strlen(ss) - 1);
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    else
    {
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    
    // IMSI - Truncate to 15 chars max
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        char tempIMSI[16];
        memset(tempIMSI, 0, sizeof(tempIMSI));
        Ql_strncpy(tempIMSI, NetWork.IMSI, 15);
        Ql_strncat(ss, tempIMSI, sizeof(ss) - Ql_strlen(ss) - 1);
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    else
    {
        Ql_strncat(ss, ",", sizeof(ss) - Ql_strlen(ss) - 1);
    }
    
    // Server IPs and Ports
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%s:%s,", VTSData.ServerData.IP1, VTSData.ServerData.Port1);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%s:%s,", VTSData.ServerData.IP2, VTSData.ServerData.Port2);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%s:%s,", VTSData.ServerData.IP3, VTSData.ServerData.Port3);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);

   #ifdef EXTENDED_IPS
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%s:%s", VTSData.ServerData.IP4, VTSData.ServerData.Port4);
    Ql_strncat(ss, str, sizeof(ss) - Ql_strlen(ss) - 1);
    #endif
    
    Ql_strncat(ss, "\n", sizeof(ss) - Ql_strlen(ss) - 1);
    
    LOGData(TAG_HARDWARE, "%s", ss);
    SendRS232String(ss);
}

// $CEL,imei,MCC,MNC,LAC,CELLID,Signal,NeighborCell1,MCC1,MNC1,LAC1,CELLID1,Signal1,NeighborCell2,MCC2,MNC2,LAC2,CELLID2,Signal2,NeighborCell3,MCC3,MNC3,LAC3,CELLID3,Signal3\r\n
void CellTowerInfoSend(void)
{
    char cel[300], str[80];
    int i;
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Check if GSM is initialized
    if(GSM.GSMState <= SIM_NOT_DETECTED)
    {
        return;
    }
    
    // Initialize buffer
    memset(cel, 0, sizeof(cel));
    memset(str, 0, sizeof(str));
    
    // Start with $CEL header and IMEI
    Ql_sprintf(cel, "$CEL,%s,", NetWork.IMEI);
    
    // Add serving cell information: MCC, MNC, LAC, CELLID, Signal (CSQ)
    // SignalStrength is already in CSQ format (0-31)
    Ql_sprintf(str, "%d,%d,%s,%s,%d,", GSM.MCC, GSM.MNC, GSM.LAC, GSM.CellID, GSM.SignalStrength);
    Ql_strncat(cel, str, sizeof(cel) - Ql_strlen(cel) - 1);
    
    // Add neighbor cells (up to 4)
    for(i = 0; i < 4; i++)
    {
        memset(str, 0, sizeof(str));
        if(GSM.NeigbourCell[i].mcc > 0 && GSM.NeigbourCell[i].mnc >= 0)
        {
            // CellDB is already in CSQ format (0-31) due to DBM_IN_CSQ define in GPRS.c
            // Add neighbor cell data: NeighborCell#, MCC#, MNC#, LAC#, CELLID#, Signal# (CSQ)
            Ql_sprintf(str, "N%d,%d,%d,%s,%s,%s,", 
                    i+1, 
                    GSM.NeigbourCell[i].mcc, 
                    GSM.NeigbourCell[i].mnc, 
                    GSM.NeigbourCell[i].LAC, 
                    GSM.NeigbourCell[i].CellID, 
                    GSM.NeigbourCell[i].CellDB);  // Already CSQ
        }
        else
        {
            // Add empty neighbor cell data
            Ql_sprintf(str, "N%d,,,,,,", i+1);
        }
        Ql_strncat(cel, str, sizeof(cel) - Ql_strlen(cel) - 1);
    }
    
    // Remove the trailing comma and add line ending
    int len = Ql_strlen(cel);
    if(len > 0 && cel[len-1] == ',')
    {
        cel[len-1] = '\0';
    }
    Ql_strncat(cel, "\n", sizeof(cel) - Ql_strlen(cel) - 1);
    
    SendRS232String(cel);
    LOGData(TAG_HARDWARE, "%s", cel);
}

//$PER,imei,GPSModemState,GPSFix,HHMMSS,DDMMYY,IsMEMs,IsFlash,IsSOS,Ignition,out1,out2,in2,MainsVolt,BattVolt\r\n
void PeripheralInfoSend(void)
{
    char per[200], str[32];
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Initialize buffers
    memset(per, 0, sizeof(per));
    memset(str, 0, sizeof(str));
    
    // Start with $PER header and IMEI
    Ql_sprintf(per, "$PER,%s,", NetWork.IMEI);
    
    // GPS Modem State (1 = OK, 0 = Fault) - Using GPS.State as indicator
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", (GPS.State > 0) ? 1 : 0);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // GPS Fix (1 = Fixed, 0 = Not Fixed)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", GPS.GPSFix);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Time (HHMMSS)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%02d%02d%02d,", 
             CurrentDateTime.Hour, 
             CurrentDateTime.Min, 
             CurrentDateTime.Sec);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Date (DDMMYY)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%02d%02d%02d,", 
             CurrentDateTime.Date, 
             CurrentDateTime.Month, 
             CurrentDateTime.Year % 100); // Last 2 digits of year
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // MEMs Status (1 = OK, 0 = Not OK) - Using IsMCU as indicator for external MCU
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", IsMCU ? 1 : 0);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Flash Status (1 = OK, 0 = Not OK) - Assume 1 for now
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", 1);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // SOS Status (1 = Active, 0 = Not Active)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", SOS.IsSOS);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Ignition Status (1 = ON, 0 = OFF)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", PeriPheralVal.IGN);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Output 1 (out1)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", PeriPheralVal.OP1);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Output 2 (out2)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", PeriPheralVal.OP2);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Input 2 (in2)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", PeriPheralVal.IP2);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);

    // Mains Voltage
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%04.1f,", PeriPheralVal.MainsVolt);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);

    // Battery Voltage
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%03.1f", PeriPheralVal.BattVolt);
    Ql_strncat(per, str, sizeof(per) - Ql_strlen(per) - 1);
    
    // Add newline before sending
    Ql_strncat(per, "\n", sizeof(per) - Ql_strlen(per) - 1);
    
    SendRS232String(per);
    LOGData(TAG_HARDWARE, "%s", per);
}

//$GPD,imei,tracksats,visiblesats,hdop,pdop,lat,long,speed,alt,heading\r\n
void GPSDataSend(void)
{
    char gpd[250], str[32];
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Initialize buffers
    memset(gpd, 0, sizeof(gpd));
    memset(str, 0, sizeof(str));
    
    // Start with $GPD header and IMEI
    Ql_sprintf(gpd, "$GPD,%s,", NetWork.IMEI);
    
    // Tracked satellites (from GGA - satellites used in navigation solution)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", GPS.NoOfSatalite);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Visible satellites (from GSV - total satellites in view across all constellations)
    // Note: SatTotal sums all constellation counts (GP+GL+GA+GB+GQ+GI+GN)
    // If modem sends GN (combined) sentences, this may count satellites multiple times
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%d,", GPS.SatTotal);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // HDOP
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.2f,", GPS.HDOP);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // PDOP
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.2f,", GPS.PDOP);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Latitude
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.6f,", GPS.Latitude);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Longitude
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.6f,", GPS.Longitude);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Speed
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.2f,", GPS.Speed);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Altitude
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.2f,", GPS.Altitude);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Heading (no trailing comma for last field)
    memset(str, 0, sizeof(str));
    Ql_sprintf(str, "%.2f", GPS.Heading);
    Ql_strncat(gpd, str, sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    // Add newline before sending
    Ql_strncat(gpd, "\n", sizeof(gpd) - Ql_strlen(gpd) - 1);
    
    SendRS232String(gpd);
    LOGData(TAG_HARDWARE, "%s", gpd);
}

