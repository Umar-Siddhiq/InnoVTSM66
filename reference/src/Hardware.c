#include "Hardware.h"

uint16_t ADCVal;
PepheralTypedef PeriPheralVal;
uint8_t Vat_init;
uint8_t PrevMain,IsMCUTime;
extern uint8_t updntp;
uint8_t PrevIgn, PrevBLow, PrevMains, PrevTamp;
#define IsBODYTAMP
#define BodyThresh 9.5



uint8_t RFIDData[RFID_MAX_DATALEN];
uint16_t RFIDDataCount;


uint8_t IsSleepMode;


float batteryPercentageToVoltage(int percentage)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    float minVoltage = MIN_BATT;
    float maxVoltage = MAX_BATT;
    float voltage = minVoltage + (percentage / 100.0) * (maxVoltage - minVoltage);
    return voltage;
}

int batteryVoltageToPercentage(float voltage) 
{
    float minVoltage = MIN_BATT;
    float maxVoltage = MAX_BATT;
    if (voltage < minVoltage) voltage = minVoltage;
    if (voltage > maxVoltage) voltage = maxVoltage;
    int percentage = (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * 100);
    return percentage;
}

static void IOInit(void)
{
	// GPIO_SetMode(IP1_PORT, IP1_PIN, GPIO_MODE_INPUT);
	// GPIO_SetMode(IGN_PORT, IGN_PIN, GPIO_MODE_INPUT);
	// GPIO_SetMode(GPO_PORT, GPO_PIN, GPIO_MODE_OUTPUT);
	// GPIO_SetMode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);
	// GPIO_SetMode(BAT_PORT, BAT_PIN, GPIO_MODE_OUTPUT);

    nwy_gpio_set_direction(IP1_PIN,nwy_input);
    nwy_gpio_set_direction(IP2_PIN,nwy_input);

    nwy_gpio_set_direction(OP1_PIN,nwy_output);   
    nwy_gpio_set_direction(OP2_PIN,nwy_output);
    OP1_Set(0);
    OP2_Set(0);

    nwy_gpio_set_direction(LED_PIN,nwy_output);
	nwy_gpio_set_value(LED_PIN,nwy_low);   



}

void SendSystemStatus(void)
{
    char status[300];
    uint8_t gpsfix;

    if (IsGPSFault)
        gpsfix = 2;
    else if (GPS.GPSFix)
        gpsfix = 1;
    else
        gpsfix = 0;

    int len = snprintf(status, sizeof(status),
        "$SYS,%s,%s,%d,%s,%d,%d,%d,%04.1f,%03.1f,"
        "%s:%s,%s:%s,%s:%s,%d,%d*",
        NetWork.IMEI,
        NetWork.SIMNo,
        VTSState.CurrentProfile,
        NetWork.IMSI,
        GSM.GSMState,
        GSM.SignalStrength,
        gpsfix,
        PeriPheralVal.MainsVolt,
        PeriPheralVal.BattVolt,
        VTSData.ServerData.IP1, VTSData.ServerData.Port1,
        VTSData.ServerData.IP2, VTSData.ServerData.Port2,
        VTSData.ServerData.IP3, VTSData.ServerData.Port3,
        SOS.IsSOS,
        PeriPheralVal.IsMEMs
    );

    if (len < 0 || len >= sizeof(status)) {
        nwy_dbg_log("System Status buffer overflow");
        return;
    }

    nwy_dbg_log("System Status: %s", status);
    SendRS232String(status);
}

void RFIDRcv(const char *data, uint32 length)
{
    if(VTSData.SensorSetting.Uart2Mode != UART2_MODE_RFID)
        return;
    if(length > RFID_MAX_DATALEN)
    {
        nwy_dbg_log("RFID Rcv length = %d > max supported %d", length,500);
        return;
    }
   
    memcpy(RFIDData,data,length);
    nwy_dbg_log("RFID Rcv length = %d", length);
    RFIDDataCount = length;
    
}


void UpdateADC(void)
{
	ADCVal = nwy_adc_get(BAT_PIN,NWY_ADC_SCALE_2V444);
}


void PeripheralInit(void)
{
	IOInit();
    PrevMain=1;
    PeriPheralVal.MainsVolt = 10.1;
    PeriPheralVal.AN2 = 9.7;
    PeriPheralVal.IsMain = 1;
}

void EnableVat(void)
{
	if(!Vat_init)
	{
		nwy_sdk_at_parameter_init();
        nwy_cli_init_unsol_reg();
		Vat_init=1;
	}
}

void SleepModeON(void)
{
    SendSleepReq();
}

void SleepModeOFF(void)
{
    IsSleepMode=0;
}

void GetBatteryVolt(void)
{
    nwy_dbg_log("Batt AdcVal : %d",ADCVal);
    PeriPheralVal.BattVolt=(double)ADCVal/501;
    PeriPheralVal.BattPerc=(PeriPheralVal.BattVolt*100)/4.2f;
    if(PeriPheralVal.BattPerc>100)
        PeriPheralVal.BattPerc=100;
    nwy_dbg_log("Calculated Batt Volt : %f",PeriPheralVal.BattVolt);
}


void ProcessPeripheral(void)
{
    char resp[100];
	UpdateADC();
    #ifdef NEW_SERIAL_INTERFACE
    static int sysstatcount=0;
    if(++sysstatcount>=6) // every 3 sec
    {
        sysstatcount=0;
        SendSystemStatus();
    }
    #endif
	if(updntp && GSM.GSMState==GPRS_ACTIVE)
	{	
		nwy_dbg_log("Sending NTP AT cmd");
        #ifdef PROTO_CDAC
        SendAtCmd("AT+UPDATETIME=1,time.nist.gov,10,\"E5:30\",0\r\n",resp,"OK");
        #else
        SendAtCmd("AT+UPDATETIME=1,time.nist.gov,10,\"E0\",0\r\n",resp,"OK");
        #endif
        updntp=0;
	}
	PeriPheralVal.IP1=!IP1_VAL;
    if(VTSData.SensorSetting.IP2Mode == IP2_MODE_NORMAL)
        PeriPheralVal.IP2=!IP2_VAL;
    else
        PeriPheralVal.IP2 = 0;

	GetBatteryVolt();

    if(GSM.GSMState>SIM_DETECTED)
    {
        if(IsPacketReady.IsGSMParam==1)
        {
            GetNeighbourCells();
            IsPacketReady.IsGSMParam=0;
        }
    }

    #ifdef IsBODYTAMP
    if(PeriPheralVal.AN2 < BodyThresh)
    {
        if(!PrevTamp)
        {
            nwy_dbg_log("********* BODY TAMPER********\n");
            PrevTamp=1;
            VAlert[TAMPER_ALERT].Enable=1;
            AddAlert(TAMPER_ALERT);
        }
    }
    else
        PrevTamp=0;
    #endif

    //nwy_dbg_log("BattVolt: %.2f, Adc : %d",PeriPheralVal.BattVolt, ADCVal);
    #ifndef PROTO_CDAC
    if(PeriPheralVal.IGN)
    {
        if(IsSleepMode)
        {
            SleepModeON();
            nwy_sleep(5000);
        }

        if(!PrevIgn)
        {
            nwy_dbg_log("********* IGNITION ON********\n");
            PrevIgn=1;
            VAlert[IGN_ON_ALERT].Enable = 1;
        }
    }
    else
    {
        if(PrevIgn)
        {
            nwy_dbg_log("********* IGNITION OFF********\n");
            PrevIgn=0;
            VAlert[IGN_OFF_ALERT].Enable=1;
        }
    }
    #endif
    if(PeriPheralVal.BattVolt < (VTSData.BattThrs-0.1))
    {
        if(!PrevBLow)
        {
            PrevBLow=1;
            SendCellCmd(1);
            VAlert[BATT_LOW_ALERT].Enable=1;
            AddAlert(BATT_LOW_ALERT);
            nwy_dbg_log("********* BATTERY LOW ALERT********\n");
        }
    }
    else if (PeriPheralVal.BattVolt >= (VTSData.BattThrs+0.1))
    {
        if(PrevBLow)
        {
            PrevBLow=0;
            SendCellCmd(0);
            VAlert[BATT_LOW_ALERT].Enable=0;
            RemoveAlert(BATT_LOW_ALERT);
            nwy_dbg_log("********* BATTERY LOW RESTORE ********\n");
            VAlert[BATT_LOW_RES_ALERT].Enable=1;
            AddAlert(BATT_LOW_RES_ALERT);
        }   
    }
    
    if(IsMCUTime)
    {
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
                nwy_dbg_log("********* MAINS RESTORE ALERT********\n");
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
                nwy_dbg_log("********* MAINS FAULT ALERT********\n");
                #ifdef PROTO_CDAC
                // Defer SMS until after TCP success to avoid network congestion
                // SMS will be sent in handleCriticalPackets after server confirms receipt
                PeriPheralVal.PendingSMSAlert = MAINS_FAIL_ALERT;
                #endif
            }
        }
        if(SOS.IsSOS){
            OP2_Set(1);}
        else{
            OP2_Set(0);}

    }
	    //nwy_dbg_log("IP1 : %d ADC :%f",PeriPheralVal.IP1, PeriPheralVal.BattVolt);
}