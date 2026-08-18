#include "MComm.h"
#include "Boot.h"

volatile uint32_t FWUpdateFlags = 0;  // ? DEFINITION: "Create it here"

uint8_t MCOMMTxBuff[MCOMM_TX_BUFF_SIZE];
uint8_t MCOMMRxBuff[MCOMM_RX_BUFF_SIZE];

PepheralTypedef RcvPhData;
__IO uint16_t RxCount;
uint16_t ExpectedLEN;
RXSTATETypedef RxState;
extern uint8_t IsRTCSet;

void ResetModem(void)
{
	MCOMM_FET=0;
	MCOMM_PWK=0;
	Delay(1200);
	MCOMM_FET=1;
	Delay(50);
	MCOMM_PWK=1;
	Delay(1000);
}

void InitModemComm(void)
{
	SYS_ResetModule(MCOMM_UART_SYS_RST);
	UART_Open(MCOMM_UART, MCOMM_UART_BAUD);
	NVIC_EnableIRQ(MCOMM_UART_IRQ);
  	UART_EnableInt(MCOMM_UART, (UART_INTEN_RDAIEN_Msk ));
	MCOMM_FET=0;
	GPIO_SetMode(MCOMM_FET_PORT, MCOMM_FET_PIN, GPIO_MODE_OUTPUT);
	GPIO_SetMode(MCOMM_PWK_PORT, MCOMM_PWK_PIN, GPIO_MODE_OUTPUT);
	ResetModem();
}

void SendDatatoModem(uint8_t *src, size_t len)
{
	while(len)
	{
		while(UART_IS_TX_FULL(MCOMM_UART));
		UART_WRITE(MCOMM_UART,*src);
		src++;
		len--;
	}
}

void UART02_IRQHandler(void)
{
    uint8_t tmp = 0xFF;

    // RS485 UART handling
    if (UART_GET_INT_FLAG(RS485_UART, UART_INTSTS_RDAINT_Msk))
    {
        while (UART_IS_RX_READY(RS485_UART))
        {
            tmp = UART_READ(RS485_UART);
            //RS485_RxByteHandler(tmp);  
        }
    }

    // Modem UART handling
    if (UART_GET_INT_FLAG(MCOMM_UART, UART_INTSTS_RDAINT_Msk))
    {
        while (UART_IS_RX_READY(MCOMM_UART))
        {
            tmp = UART_READ(MCOMM_UART);

            // --- ECHO BACK FOR DEBUGGING ----
            //SendChar(tmp);   // send the received byte to UART0
            // --------------------------------

            switch (RxState)
            {
                case WAIT_FOR_PARS:
                    //SendString("[DEBUG] RXSTATE = WAIT_FOR_PARS\r\n");
                    return;

                case SRCH_FOR_ST: 
                    //SendString("[DEBUG] RXSTATE = SRCH_FOR_ST\r\n");
                    if (tmp != MCOMM_COM_HEADER)
                        return;

                    MCOMMRxBuff[0] = tmp;
                    RxCount = 1;
                    RxState = SRCH_FOR_FUNC;
                    //SendString("[DEBUG] RXSTATE -> SRCH_FOR_FUNC\r\n");
                    return;

                case SRCH_FOR_FUNC:
                    //SendString("[DEBUG] RXSTATE = SRCH_FOR_FUNC\r\n");
                    if (tmp >= MCOMM_RX_FLAG_COUNT)
                    {
                        RxState = SRCH_FOR_ST;
                        //SendString("[DEBUG] RXSTATE -> SRCH_FOR_ST (invalid func)\r\n");
                        return;
                    }

                    ExpectedLEN = LENTABLE[tmp];
                    MCOMMRxBuff[RxCount++] = tmp;
                    RxState = SRCH_FOR_ED;
                    //SendString("[DEBUG] RXSTATE -> SRCH_FOR_ED\r\n");
                    return;

                case SRCH_FOR_ED:
                    //SendString("[DEBUG] RXSTATE = SRCH_FOR_ED\r\n");
                    MCOMMRxBuff[RxCount++] = tmp;

                    if (RxCount == ExpectedLEN)
                    {
                        if (tmp != MCOMM_COM_FOOTER)
                        {
                            RxState = SRCH_FOR_ST;
                            //SendString("[DEBUG] RXSTATE -> SRCH_FOR_ST (footer mismatch)\r\n");
                            return;
                        }

                        RxState = WAIT_FOR_PARS;
                        //SendString("[DEBUG] RXSTATE -> WAIT_FOR_PARS (frame complete)\r\n");
                        return;
                    }
                    break;

                default:
                    RxState = SRCH_FOR_ST;
                    //SendString("[DEBUG] RXSTATE -> SRCH_FOR_ST (default case)\r\n");
                    return;
            }
        }
    }
}

void MCOMM_SendSuccess(void)
{
	ClearMCOMMTXBuffer();
	MCOMMTxBuff[MCOMM_COM_HEADER_INDEX] = MCOMM_COM_HEADER;
	MCOMMTxBuff[MCOMM_COM_FUNCTION_INDEX] = MCOMM_COM_FUNCTION_SUCCESS;
	MCOMMTxBuff[MCOMM_COM_LEN_SUCCESS_MCU-1]=MCOMM_COM_FOOTER;
	SendDatatoModem(MCOMMTxBuff,MCOMM_COM_LEN_SUCCESS_MCU);
}

void MCOMM_SendError(void)
{
	ClearMCOMMTXBuffer();
	MCOMMTxBuff[MCOMM_COM_HEADER_INDEX] = MCOMM_COM_HEADER;
	MCOMMTxBuff[MCOMM_COM_FUNCTION_INDEX] = MCOMM_COM_FUNCTION_ERROR;
	MCOMMTxBuff[MCOMM_COM_LEN_ERROR_MCU-1]=MCOMM_COM_FOOTER;
	SendDatatoModem(MCOMMTxBuff,MCOMM_COM_LEN_ERROR_MCU);
}
void MCOMM_SendVer(void)
{
	ClearMCOMMTXBuffer();
	FirmExchangetypedef firmdata = {0};
	strncpy(firmdata.FirmwareVersion,MCOMM_VERSION,sizeof(firmdata.FirmwareVersion)-1);
	firmdata.FirmwareVersion[sizeof(firmdata.FirmwareVersion)] = 0;
	MCOMMTxBuff[MCOMM_COM_HEADER_INDEX] = MCOMM_COM_HEADER;
	MCOMMTxBuff[MCOMM_COM_FUNCTION_INDEX] = MOMMM_COM_FUNCTION_VERSION;
	memcpy((void*)&MCOMMTxBuff[MCOMM_COM_DATA_INDEX],(void*)&firmdata,sizeof(FirmExchangetypedef));
	MCOMMTxBuff[MCOMM_COM_LEN_VERREPLY-1]=MCOMM_COM_FOOTER;
	SendDatatoModem(MCOMMTxBuff,MCOMM_COM_LEN_VERREPLY);
}

void MCOMM_SendPhpr(void)
{
	ClearMCOMMTXBuffer();
	MCOMMTxBuff[MCOMM_COM_HEADER_INDEX] = MCOMM_COM_HEADER;
	MCOMMTxBuff[MCOMM_COM_FUNCTION_INDEX] = MCOMM_COM_FUNCTION_GETPH;
	memcpy((void*)&MCOMMTxBuff[MCOMM_COM_DATA_INDEX],&PeriPheralVal,sizeof(PepheralTypedef));
	MCOMMTxBuff[MCOMM_COM_LEN_GETPH_MCU-1]=MCOMM_COM_FOOTER;
	SendDatatoModem(MCOMMTxBuff,MCOMM_COM_LEN_GETPH_MCU);
}

void MCOMM_SendSerialData(uint8_t IsRS485, uint8_t *src, size_t len)
{
    if (!src || len > MCOMM_COM_URT_EXG_BUFF_SIZE)
        return;

    ClearMCOMMTXBuffer();

    MCOMMTxBuff[MCOMM_COM_HEADER_INDEX] = MCOMM_COM_HEADER;
    MCOMMTxBuff[MCOMM_COM_FUNCTION_INDEX] = IsRS485 ? MCOMM_COM_FUNCTION_485RX : MCOMM_COM_FUNCTION_232RX;
    
    // Populate UartExchangetypedef structure directly into buffer
    // datalen is uint16_t (2 bytes) - write as little-endian
    uint16_t dataLen = (uint16_t)len;
    MCOMMTxBuff[MCOMM_COM_DATA_INDEX] = (uint8_t)(dataLen & 0xFF);         // Low byte
    MCOMMTxBuff[MCOMM_COM_DATA_INDEX + 1] = (uint8_t)((dataLen >> 8) & 0xFF); // High byte
    memcpy(&MCOMMTxBuff[MCOMM_COM_DATA_INDEX + 2], src, len);  // data[]
    // The rest of the buffer can remain as is (zeroed out) if needed

    // Footer goes at the end of full UartExchangetypedef structure
    MCOMMTxBuff[MCOMM_COM_DATA_INDEX + sizeof(UartExchangetypedef)] = MCOMM_COM_FOOTER;

    size_t totalLen = MCOMM_COM_LEN_EXTRAS + sizeof(UartExchangetypedef);
    SendDatatoModem(MCOMMTxBuff, totalLen);
}


void ParseModemData(void)
{
    uint8_t function = MCOMMRxBuff[MCOMM_COM_FUNCTION_INDEX];
		
		if (function == MCOMM_COM_FUNCTION_GETPH)
    {
        MCOMM_SendPhpr();
        return;
    }
		
		if (function == MCOMM_COM_FUNCTION_485TX || function == MCOMM_COM_FUNCTION_232TX)
    {
        UartExchangetypedef *uartData = (UartExchangetypedef *)&MCOMMRxBuff[MCOMM_COM_DATA_INDEX];

        if ((uartData->datalen > 0) && (uartData->datalen <= MCOMM_COM_URT_EXG_BUFF_SIZE))
        {
            if (function == MCOMM_COM_FUNCTION_485TX)
						{
                RS485SendData(uartData->data, uartData->datalen);
						}
            else
						{
                RS232SendData(uartData->data, uartData->datalen);
								MCOMM_SendSuccess();
						}
        }
				else
				{
						MCOMM_SendError();
						return;
				}
				return;
		}
		
    if(function == MCOMM_COM_FUNCTION_FWSTART)
    {
        expected_chunk_number = 0;
        total_chunks = (MCOMMRxBuff[MCOMM_COM_DATA_INDEX] << 8) | MCOMMRxBuff[MCOMM_COM_DATA_INDEX + 1]; 
        MCOMM_SendSuccess();
        return; 
    }
		
		if (function == MCOMM_COM_FUNCTION_FWUPDATE)
    {
        if(total_chunks == 0)
        {
            // ERROR: FWSTART not received
            SendString("[FWUPDATE] ERROR: FWSTART not received\r\n");
            MCOMM_SendError();
            return;
        }
        UART_DisableInt(MCOMM_UART, UART_INTEN_RDAIEN_Msk);
				FW_SET(FW_FLAG_UART_IRQ_DISABLED);
				FW_SET(FW_FLAG_CHUNK_AVAILABLE);
				//FW_SET(FW_FLAG_UPDATE_IN_PROGRESS);
        return;
    }

    if (function == MCOMM_COM_FUNCTION_FWEND)
    {
        if(expected_chunk_number == total_chunks)
        {
            FW_SET(FW_FLAG_UPDATE_COMPLETED);
            SendString("[FWUPDATE] SUCCESS: All chunks received\r\n");
            MCOMM_SendSuccess();
						Delay(7000);
						JumpToBOOT();
        }
        else
        {
            FW_SET(FW_FLAG_UPDATE_ABORTED);
            SendString("[FWUPDATE] ERROR: Missing chunks\r\n");
            MCOMM_SendError();
        }
        total_chunks = 0;   
        return; 
    }
		
		if (function == MOMMM_COM_FUNCTION_VERSION)
    {
			MCOMM_SendVer();
			return;
		}
			
}

void ProcessModemComm(void)
{
	if(RxState!=WAIT_FOR_PARS)
	{
		return;
	}
	ParseModemData();
	ProcessFirmwareUpdateSM();
	ClearMCOMMRXBuffer();
	RxState=SRCH_FOR_ST;
}



/* 
 * MComm.c - Modem Communication Protocol Implementation
 * 
 * CORE COMPONENTS:
 * 1. HARDWARE CONTROL:
 *    - ResetModem(): Controls FET/PWK pins for modem hard reset
 *    - InitModemComm(): UART initialization and modem setup
 * 
 * 2. COMMUNICATION PROTOCOL:
 *    - UART02_IRQHandler(): Interrupt-driven frame reception state machine
 *    - SendDatatoModem(): Blocking UART transmission
 *    - ParseModemData(): Frame parsing and command routing
 * 
 * 3. PROTOCOL MESSAGES:
 *    - MCOMM_SendSuccess()/Error(): Acknowledgement responses
 *    - MCOMM_SendPhpr(): Sends peripheral data to modem
 *    - MCOMM_SendSerialData(): Forwards UART data to modem
 * 
 * 4. FIRMWARE UPDATE HANDLING:
 *    - Multi-stage update process (START ? UPDATE ? END)
 *    - Chunk-based transfer with sequence verification
 *    - Interrupt management during firmware writes
 * 
 * WORKFLOW:
 * 1. IRQ Handler receives bytes and assembles frames using state machine
 * 2. ProcessModemComm() checks for complete frames and triggers parsing
 * 3. ParseModemData() routes to appropriate handler based on function code
 * 4. Response sent back to modem via matching function code
 * 
 * ERROR HANDLING:
 * - Frame validation (header/footer, function code range, length)
 * - Firmware update sequence verification
 * - Automatic state reset on protocol errors
 */