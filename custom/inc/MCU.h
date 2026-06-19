#ifndef _MCU_H_
#define _MCU_H_

#include "VTS.h"
#include "Hardware.h"
#include "File.h"
#include "GPS.h"
#include "MOTA.h"

#define MCOMM_UART_PORT UART_PORT1
#define MCOMM_UART_BAUDRATE 115200

#define MCOMM_RX_BUFF_SIZE 1024*1
#define MCOMM_TX_BUFF_SIZE 1024*1


extern uint8_t MCOMMTxBuff[];
extern uint8_t MCOMMRxBuff[];


static inline void ClearMCOMMRXBuffer(void)
{
    memset(MCOMMRxBuff,0x00,MCOMM_RX_BUFF_SIZE);
}

static inline void ClearMCOMMTXBuffer(void)
{
    memset(MCOMMTxBuff,0x00,MCOMM_TX_BUFF_SIZE);
}

extern int MCOMMUartHandle;
extern uint8_t IsMCOMM;

#define MCOMM_COM_HEADER  0x26
#define MCOMM_COM_FOOTER  0x7E

#define MCOMM_COM_HEADER_INDEX    0
#define MCOMM_COM_FUNCTION_INDEX  1
#define MCOMM_COM_DATA_INDEX      2
typedef enum 
{
    MCOMM_COM_FUNCTION_ERROR      = 0,   // 0: Generic error or invalid command
    MCOMM_COM_FUNCTION_SUCCESS    = 1,   // 1: Operation completed successfully
    MCOMM_COM_FUNCTION_GETPH      = 2,   // 2: Request PHERI from modem
    MCOMM_COM_FUNCTION_SETPH      = 3,   // 3: Set PHERI  to modem
    MCOMM_COM_FUNCTION_485TX      = 4,   // 4: Send data via RS485 (MCU -> external)
    MCOMM_COM_FUNCTION_485RX      = 5,   // 5: Receive data via RS485 (external -> MCU)
    MCOMM_COM_FUNCTION_232TX      = 6,   // 6: Send data via RS232 (MCU -> modem)
    MCOMM_COM_FUNCTION_232RX      = 7,   // 7: Receive data via RS232 (modem -> MCU)
    MCOMM_COM_FUNCTION_FWSTART    = 8,   // 8: Firmware update start command
    MCOMM_COM_FUNCTION_FWUPDATE   = 9,   // 9: Firmware chunk transfer command
    MCOMM_COM_FUNCTION_FWEND      = 10,  // 10: Firmware update end/verification command
    MOMMM_COM_FUNCTION_VERSION    = 11,  // 11: Request Firmware Version
    MCOMM_COM_FUNCTION_SLEEP      = 12,  // 12: Command MCU to enter sleep mode
    MCOMM_COM_FUNCTION_COUNT             // 11: Total number of defined commands
} MCOMMComFunctionTypedef;




#define MCOMM_RX_FLAG_COUNT  (MCOMM_COM_FUNCTION_COUNT)
extern uint8_t MCOMMRcvFlags[MCOMM_RX_FLAG_COUNT];
								
typedef enum {SRCH_FOR_ST=0,SRCH_FOR_FUNC,SRCH_FOR_ED,WAIT_FOR_PARS}RXSTATETypedef;


#define MCOMM_COM_URT_EXG_BUFF_SIZE   500
typedef struct 
{
    uint16_t datalen;
    uint8_t data[MCOMM_COM_URT_EXG_BUFF_SIZE];
}UartExchangetypedef;

// RS232/RS485 data availability flags and buffers
extern volatile uint8_t RS485_DataAvailable;
extern volatile uint8_t RS232_DataAvailable;
extern UartExchangetypedef RS485_Buffer;
extern UartExchangetypedef RS232_Buffer;

typedef struct
{
    uint8_t IP1;
    uint8_t IP2;
    uint16_t MainVolt;
    uint16_t BattVolt;
    uint16_t ADCVal1;
    uint16_t ADCVal2;
    uint8_t IsFlash;    // ADD THIS - Flash memory status
    uint8_t IsMems;     // ADD THIS - MEMS sensor status
    uint8_t IsTilt;     // ADD THIS - Tilt sensor status
} MCUPepheralTypedef;

typedef struct{
		char FirmwareVersion[10];
}FirmExchangetypedef;

typedef struct{
    uint16_t sleeptime;
}ModemSleepTypedef;

#define MOTA_CHUNK_SIZE 256
#define MOTA_CHUCK_NUM_SIZE 2


#define MCOMM_COM_LEN_EXTRAS 3
// FUNTION TOTAL LENGTHS

#define MCOMM_COM_LEN_ERROR_MCU         MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_SUCCESS_MCU       MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_GETPH_MDM         MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_GETPH_MCU         MCOMM_COM_LEN_EXTRAS+sizeof(MCUPepheralTypedef)
#define MCOMM_COM_LEN_SETPH_MDM         MCOMM_COM_LEN_EXTRAS+sizeof(MCUPepheralTypedef)
#define MCOMM_COM_LEN_485_MDM           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_485_MCU           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_232_MDM           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_232_MCU           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_FWSTART			MCOMM_COM_LEN_EXTRAS+MOTA_CHUCK_NUM_SIZE
#define MCOMM_COM_LEN_FWUPDATE			MCOMM_COM_LEN_EXTRAS+MOTA_CHUCK_NUM_SIZE
#define MCOMM_COM_LEN_FWEND				MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_VERSION_MCU       MCOMM_COM_LEN_EXTRAS+sizeof(FirmExchangetypedef)
#define MCOMM_COM_LEN_SLEEP_MDM         MCOMM_COM_LEN_EXTRAS+sizeof(ModemSleepTypedef)


static const uint16_t LENTABLE[MCOMM_RX_FLAG_COUNT] = 
{
	MCOMM_COM_LEN_ERROR_MCU,
	MCOMM_COM_LEN_SUCCESS_MCU,
	MCOMM_COM_LEN_GETPH_MCU,
	MCOMM_COM_LEN_SETPH_MDM,
	MCOMM_COM_LEN_485_MDM,
	MCOMM_COM_LEN_485_MCU,
	MCOMM_COM_LEN_232_MDM,
	MCOMM_COM_LEN_232_MCU,
	MCOMM_COM_LEN_FWSTART,
	MCOMM_COM_LEN_FWUPDATE,
	MCOMM_COM_LEN_FWEND,
    MCOMM_COM_LEN_VERSION_MCU,
    MCOMM_COM_LEN_SLEEP_MDM
};

void mcu_rcv_thread(s32 taskId);
int MCOMM_SendSerial(uint8_t Is485, const uint8_t* payload, int len);
int MCOMM_FetchPerihperal(void);
uint8_t MCOMM_SendData(uint8_t* data, int len, uint8_t isWait, uint8_t waitFlag, uint16_t timeout);
int MCOMM_FetchVER(void);
int MCOMM_SendSleep(uint16_t sleeptime);
extern int IsMCU;

#endif // _MCU_H_