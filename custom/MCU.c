#include "MCU.h"



uint8_t MCOMMTxBuff[MCOMM_TX_BUFF_SIZE] = {0};
uint8_t MCOMMRxBuff[MCOMM_RX_BUFF_SIZE] = {0};
u8 mcu_tmpdata[MCOMM_RX_BUFF_SIZE] = {0};
uint8_t MCOMMRcvFlags[MCOMM_RX_FLAG_COUNT];

uint8_t mcu_data_available = 0;
int MCUTimeout = 0, IsMCU=0;

volatile uint8_t RS485_DataAvailable = 0;
volatile uint8_t RS232_DataAvailable = 0;

UartExchangetypedef RS485_Buffer = {0};
UartExchangetypedef RS232_Buffer = {0};


float mcu_mainsadc_to_voltage(int adc_value)
{

    const float VREF = 3.3f;           // ADC reference voltage
    const float DIODE_DROP = 0.67f;    // Constant diode voltage drop
    const float DIVIDER_RATIO = 7.32f; // From your setup

    
    float adc_voltage = ((float)adc_value / 1023.0f) * VREF;
    float input_voltage = (adc_voltage * DIVIDER_RATIO) + DIODE_DROP;
    if(input_voltage<3.0f)
    {
        input_voltage = 0; 
    }
    return input_voltage;  // Final external voltage before divider & diode
}

void mcu_uart_cb(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
void mcu_rcv_thread_init(u32 taskId);

    uint8_t MCOMM_WaitFlag(uint8_t Flag, int timeout)
    {
        int tmout = timeout/10;
        while (1)
        {
            if (MCOMMRcvFlags[Flag] == 1)
            {
                MCOMMRcvFlags[Flag] = 0;
                return 1;
            }
            if (--tmout <= 0)
            {
                return 0;
            }
            ThreadSleep(10);
        }
    }


//--------------

void PrintHexBuffer(const char *tag, const char* action, const uint8_t *data, size_t len) 
{
    char line[128]; // Temporary buffer for one line
    uint8_t offset = 0;

    LOGData(tag, "%s[%d]:", action, len); // Print action and total length

    for (size_t i = 0; i < len; i++) 
    {
        if (i % 8 == 0) 
        {
            if (i != 0) 
            {
                LOGData(tag, "%s", line); // Print the current line
                offset = 0;               // Reset for next line
            }
        }

        Ql_sprintf(line + offset, "%02X ", data[i]);
        offset += 3; // Each byte prints as 2 digits + 1 space
    }

    if (offset > 0) 
    {
        LOGData(tag, "%s", line); // Print any remaining bytes
    }
}


void InitMCOMM(void)
{
    s32 ret;
    Ql_memset(MCOMMTxBuff, 0x00, MCOMM_TX_BUFF_SIZE);
    Ql_memset(MCOMMRxBuff, 0x00, MCOMM_RX_BUFF_SIZE);
    ret = Ql_UART_Register(MCOMM_UART_PORT, (CallBack_UART_Notify )mcu_uart_cb, NULL);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_MCU,"Fail to register serial port[%d], ret=%d\r\n", MCOMM_UART_PORT, ret);
    }
    ret = Ql_UART_Open(MCOMM_UART_PORT, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_MCU,"Fail to open serial port[%d], ret=%d\r\n", MCOMM_UART_PORT, ret);
    }
    LOGData(TAG_MCU,"UART port opened successfully\r\n");
    //IsMCOMM = 0;
}

s32 mcu_uart_read(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        LOGData(TAG_MCU,"<-- Invalid buffer or length! -->\r\n");
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        // Check for buffer overflow before reading
        if (rdTotalLen >= bufLen) {
            LOGData(TAG_MCU,"Buffer full at %d bytes\r\n", rdTotalLen);
            break;
        }
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0)  // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        LOGData(TAG_MCU,"Fail to read from port[%d], ret:%d\r\n", port,rdLen);
        return -99;
    }
    return rdTotalLen;
}
#define MCU_UART_DEBUG
void mcu_uart_cb(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{

    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    #ifdef MCU_UART_DEBUG
    //LOGData(TAG_MCU,"MCU cb: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    #endif
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (MCOMM_UART_PORT == port)
            {
                
                s32 totalBytes = mcu_uart_read(port, mcu_tmpdata, MCOMM_RX_BUFF_SIZE);
                if (totalBytes <= 0)
                {
                    LOGData(TAG_MCU,"read error ret:%d-->",totalBytes);
                    return;
                }

                // RAW UART RX debug print
                PrintHexBuffer(TAG_MCU, "RAW UART RX", mcu_tmpdata, totalBytes > 64 ? 64 : totalBytes);

                if(mcu_data_available)
                {
                    LOGData(TAG_MCU,"MCU Rcv %d While Processing Data\r\n",totalBytes);
                    return;
                }

                // Bounds check before copy
                if (totalBytes > MCOMM_RX_BUFF_SIZE) {
                    LOGData(TAG_MCU,"Data too large: %d bytes\r\n", totalBytes);
                    return;
                }

                Ql_memcpy((void*)MCOMMRxBuff, (void*)mcu_tmpdata, totalBytes);
                mcu_data_available = totalBytes;
                MCUTimeout = 25;
            }
            break;
        }
        
    case EVENT_UART_READY_TO_WRITE: 
        break;
    default:
        break;
    }
}

void ProcessMCUData(uint8_t* data)
{
    if (!data) return;

    MCUTimeout= 300; // Reset MCU timeout on data reception

    uint8_t header = data[MCOMM_COM_HEADER_INDEX];
    uint8_t function = data[MCOMM_COM_FUNCTION_INDEX];

    // Validate header and function
    if (header != MCOMM_COM_HEADER || function >= MCOMM_RX_FLAG_COUNT)
    {
        LOGData(TAG_MCU, "Invalid header or function: %02X %02X", header, function);
        return;
    }

    uint16_t expectedLen = LENTABLE[function];

    // Validate footer
    if (data[expectedLen - 1] != MCOMM_COM_FOOTER)
    {
        LOGData(TAG_MCU, "Invalid footer at index %d: %02X", expectedLen - 1, data[expectedLen - 1]);
        return;
    }
    

    PrintHexBuffer(TAG_MCU,"Rx", data, expectedLen);

    switch (function)
    {
        case MCOMM_COM_FUNCTION_ERROR:
            LOGData(TAG_MCU, "Function: ERROR");
            break;

        case MCOMM_COM_FUNCTION_SUCCESS:
            LOGData(TAG_MCU, "Function: SUCCESS");
            break;

        case MCOMM_COM_FUNCTION_GETPH:
        {
            LOGData(TAG_MCU, "Function: GETPH");
            MCUPepheralTypedef * mcuData = (MCUPepheralTypedef*)&data[MCOMM_COM_DATA_INDEX];
            PeriPheralVal.AN1 = mcuData->ADCVal1; 
            PeriPheralVal.AN2 = mcuData->ADCVal2;
            PeriPheralVal.IP1 = mcuData->IP1;
            PeriPheralVal.IP2 = mcuData->IP2;
            
            PeriPheralVal.MainsVolt = mcu_mainsadc_to_voltage(mcuData->MainVolt);

            PeriPheralVal.IsFlash = mcuData->IsFlash;
            PeriPheralVal.IsMems = mcuData->IsMems;
            if(!PeriPheralVal.IsTilt && mcuData->IsTilt)
            {
                LOGData(TAG_MCU, "[TILT_DEBUG] TILT DETECTED! Calling AddAlert(TILT_ALERT=%d)", TILT_ALERT);
                AddAlert(TILT_ALERT);
            }
            PeriPheralVal.IsTilt = mcuData->IsTilt;

            //PeriPheralVal.BattVolt = mcuData->BattVolt;
            LOGData(TAG_MCU, "Mains ADC Value: %d, final:%f",mcuData->MainVolt,PeriPheralVal.MainsVolt);
            break;
        }
        case MCOMM_COM_FUNCTION_SETPH:
            LOGData(TAG_MCU, "Function: SETPH");
            break;

        case MCOMM_COM_FUNCTION_485RX:
        {
            LOGData(TAG_MCU, "Function: 485RX");

            UartExchangetypedef* uartData = (UartExchangetypedef*)&data[MCOMM_COM_DATA_INDEX];

            // Validate data length before copying
            if (uartData->datalen > 0 && uartData->datalen <= MCOMM_COM_URT_EXG_BUFF_SIZE)
            {
                Ql_memcpy(&RS485_Buffer, uartData, sizeof(UartExchangetypedef));
                RS485_DataAvailable = 1;
            }
            else
            {
                LOGData(TAG_MCU, "485RX Data invalid size: %d", uartData->datalen);
            }
            break;
        }

        case MCOMM_COM_FUNCTION_232RX:
        {
            LOGData(TAG_MCU, "Function: 232RX");

            UartExchangetypedef* uartData = (UartExchangetypedef*)&data[MCOMM_COM_DATA_INDEX];

            // Validate data length before copying
            if (uartData->datalen > 0 && uartData->datalen <= MCOMM_COM_URT_EXG_BUFF_SIZE)
            {
                Ql_memcpy(&RS232_Buffer, uartData, sizeof(UartExchangetypedef));
                RS232_DataAvailable = 1;
            }
            else
            {
                LOGData(TAG_MCU, "232RX Data invalid size: %d", uartData->datalen);
            }
            break;
        }
        case MCOMM_COM_FUNCTION_FWSTART:
        {
            LOGData(TAG_MCU, "Function: FWSTART");
            break;
        }
        case MCOMM_COM_FUNCTION_FWUPDATE:
        {
            //memcpy((void*)&mota_rcv_chunk_num,(void*)&data[MCOMM_COM_DATA_INDEX],MOTA_CHUCK_NUM_SIZE);
            mota_rcv_chunk_num = (uint16_t)(data[MCOMM_COM_DATA_INDEX] << 8) | data[MCOMM_COM_DATA_INDEX + 1];;
            LOGData(TAG_MCU, "Function: MOTA_PACKET, Chunk Num: %u", mota_rcv_chunk_num);
            break;
        }
        case MOMMM_COM_FUNCTION_VERSION:
        {
            LOGData(TAG_MCU, "Function: VERSION");
            FirmExchangetypedef* firmData = (FirmExchangetypedef*)&data[MCOMM_COM_DATA_INDEX];
            strncpy(PeriPheralVal.MCUFirmwareVersion, firmData->FirmwareVersion, sizeof(PeriPheralVal.MCUFirmwareVersion)-1);
            LOGData(TAG_MCU, "Firmware Version: %s", PeriPheralVal.MCUFirmwareVersion);
            break;
        }


        default:
            LOGData(TAG_MCU, "Function: Unknown Handler %d", function);
            break;
    }
    if(function < MCOMM_RX_FLAG_COUNT)
        MCOMMRcvFlags[function] = 1;
}


uint8_t* ParseMCOMMString(uint8_t* Buff)
{
    if (!Buff) return NULL;

    // Check bounds before accessing array
    if (Buff < MCOMMRxBuff || Buff >= MCOMMRxBuff + MCOMM_RX_BUFF_SIZE)
    {
        LOGData(TAG_MCU, "Buffer pointer out of bounds");
        return NULL;
    }

    if (Buff[MCOMM_COM_HEADER_INDEX] != MCOMM_COM_HEADER)
    {
        return NULL;
    }

    uint8_t function = Buff[MCOMM_COM_FUNCTION_INDEX];

    if (function >= MCOMM_RX_FLAG_COUNT)
    {
        LOGData(TAG_MCU, "Invalid Function: %02X", function);
        return NULL;
    }

    uint16_t expectedLen = LENTABLE[function];

    // Verify we don't read past buffer end
    if ((Buff + expectedLen) > (MCOMMRxBuff + MCOMM_RX_BUFF_SIZE))
    {
        LOGData(TAG_MCU, "Packet extends beyond buffer bounds");
        return NULL;
    }

    if (Buff[expectedLen - 1] != MCOMM_COM_FOOTER)
    {
        LOGData(TAG_MCU, "Invalid Footer: %02X Expected at %d", Buff[expectedLen - 1], expectedLen - 1);
        return NULL;
    }

    // Process this packet
    ProcessMCUData(Buff);

    // Return pointer to next possible packet, but check bounds
    uint8_t* nextPacket = Buff + expectedLen;
    if (nextPacket >= MCOMMRxBuff + MCOMM_RX_BUFF_SIZE)
    {
        return NULL;  // Reached end of buffer
    }

    return nextPacket;
}

void mcu_rcv_thread(s32 taskId)
{
    mcu_rcv_thread_init(taskId);
    InitMCOMM();
    while(1)
    {
        if (MCUTimeout == 0 && IsMCU == 1)
        {
            IsMCU = 0;
        }
        else if (IsMCU == 0 && MCUTimeout > 0)
        {
            IsMCU = 1;
        }
        if (MCUTimeout > 0)
            MCUTimeout--;

        if (mcu_data_available > 0)
        {
            uint8_t* ret = MCOMMRxBuff;
            int safety_counter = 0;
            const int MAX_PACKETS = 10;  // Prevent infinite loop

            while (ret != NULL && safety_counter < MAX_PACKETS)
            {
                ret = ParseMCOMMString(ret);
                safety_counter++;
            }

            if (safety_counter >= MAX_PACKETS)
            {
                LOGData(TAG_MCU, "Warning: Max packet limit reached in parsing");
            }

            ClearMCOMMRXBuffer();  // clear buffer
            mcu_data_available = 0;
        }
        ThreadSleep(30);
    }

}

void mcu_rcv_thread_init(u32 taskId)
{
    s32 ret;
    OSThread MCURcv_Thread = {0};
    MCURcv_Thread.taskId = taskId;
    Ql_strcpy(MCURcv_Thread.taskName, "MCU Rcv Thread");
    MCURcv_Thread.taskEnable = 1;
    MCURcv_Thread.taskState = TASK_STATE_NORMAL;
    MCURcv_Thread.taskPriority = 1;
    ret = InitializeThread(&MCURcv_Thread);
    if (ret < 0)
    {
        LOGData(TAG_MCU,"Fail to initialize thread[%d], ret=%d\r\n", taskId, ret);
        return;
    }
}

uint8_t IsSent=0;

uint8_t MCOMM_SendData(uint8_t* data, int len, uint8_t isWait, uint8_t waitFlag, uint16_t timeout)
{
    // Wait if a previous transmission is ongoing
    while (IsSent)
    {
        ThreadSleep(20);
    }

    IsSent = 1;

    // Clear the response flag before sending, if waiting for a reply
    if (isWait)
    {
        MCOMMRcvFlags[waitFlag] = 0;
    }

    LOGData(TAG_MCU,"Sending data: len=%d, isWait=%d, waitFlag=%d, timeout=%d",len, isWait, waitFlag, timeout);

    int ret = Ql_UART_Write(MCOMM_UART_PORT, data, len);
    if (ret != len)
    {
        IsSent = 0;
        LOGData(TAG_MCU, "UART write error: %d", ret);
        return 0;   // UART write error
    }

    // Wait for the corresponding flag if required
    if (isWait == 1)
    {
        if (MCOMM_WaitFlag(waitFlag, timeout) == 0)
        {
            IsSent = 0;
            LOGData(TAG_MCU, "Timeout waiting for flag %d", waitFlag);
            return 0;   // Timeout
        }
    }

    IsSent = 0;
    return 1;   // Success
}


int MCOMM_SendSerial(uint8_t Is485, const uint8_t* payload, int len)
{
    if (!payload || len <= 0 || len > MCOMM_COM_URT_EXG_BUFF_SIZE)
    {
        LOGData(TAG_MCU, "Invalid payload or length: %d", len);
        return -1;
    }

    uint8_t buffer[MCOMM_COM_LEN_EXTRAS + sizeof(UartExchangetypedef)];
    int idx = 0;

    uint8_t function = Is485 ? MCOMM_COM_FUNCTION_485TX : MCOMM_COM_FUNCTION_232TX;
    buffer[idx++] = MCOMM_COM_HEADER;
    buffer[idx++] = function;

    UartExchangetypedef uartexhange = {0};
    uartexhange.datalen = len;
    Ql_memcpy(uartexhange.data, payload, len); 

    Ql_memcpy(&buffer[idx], &uartexhange, sizeof(UartExchangetypedef)); 
    idx += sizeof(UartExchangetypedef); 


    buffer[idx++] = MCOMM_COM_FOOTER; 

    
    return MCOMM_SendData(buffer, idx, 1, MCOMM_COM_FUNCTION_SUCCESS, 1000);
}

int MCOMM_FetchPerihperal(void)
{
    uint8_t buffer[MCOMM_COM_LEN_EXTRAS + sizeof(MCUPepheralTypedef)];
    int idx = 0;

    buffer[idx++] = MCOMM_COM_HEADER;
    buffer[idx++] = MCOMM_COM_FUNCTION_GETPH;
    buffer[idx++] = MCOMM_COM_FOOTER;

    return MCOMM_SendData(buffer, idx, 1, MCOMM_COM_FUNCTION_GETPH, 1000);
}

int MCOMM_FetchVER(void)
{
    uint8_t buffer[MCOMM_COM_LEN_EXTRAS + sizeof(FirmExchangetypedef)];
    int idx = 0;

    buffer[idx++] = MCOMM_COM_HEADER;
    buffer[idx++] = MOMMM_COM_FUNCTION_VERSION;
    buffer[idx++] = MCOMM_COM_FOOTER;

    return MCOMM_SendData(buffer, idx, 1, MOMMM_COM_FUNCTION_VERSION, 1000);
}

int MCOMM_SendSleep(uint16_t sleeptime)
{
    uint8_t buffer[MCOMM_COM_LEN_EXTRAS + sizeof(ModemSleepTypedef)];
    int idx = 0;

    buffer[idx++] = MCOMM_COM_HEADER;
    buffer[idx++] = MCOMM_COM_FUNCTION_SLEEP;

    ModemSleepTypedef sleepCmd = {0};
    sleepCmd.sleeptime = sleeptime;

    Ql_memcpy(&buffer[idx], &sleepCmd, sizeof(ModemSleepTypedef)); 
    idx += sizeof(ModemSleepTypedef); 

    buffer[idx++] = MCOMM_COM_FOOTER; 

    
    return MCOMM_SendData(buffer, idx, 1, MCOMM_COM_FUNCTION_SUCCESS, 1000);
}



