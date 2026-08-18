#ifndef MCOMM_H_
#define MCOMM_H_

#include "NuMicro.h"
#include "Hardware.h"
#include "MOTA.h"
#include <stdio.h>
#include <string.h>
#include "LSM6DSOX.h"

#define MCOMM_VERSION					"V0.1.3"

#define MCOMM_UART_SYS_RST		UART0_RST
#define MCOMM_UART						UART0
#define MCOMM_UART_BAUD				115200
#define MCOMM_UART_IRQ				UART02_IRQn

#define MCOMM_FET_PORT				PC
#define MCOMM_FET_PIN				BIT4
#define MCOMM_FET 					PC4

#define MCOMM_PWK_PORT				PF
#define MCOMM_PWK_PIN				BIT15
#define MCOMM_PWK 					PF15




#define MCOMM_RX_BUFF_SIZE (512*1)
#define MCOMM_TX_BUFF_SIZE (512*1)


extern uint8_t MCOMMTxBuff[];
extern uint8_t MCOMMRxBuff[];
extern uint8_t MCOMMRxFlags[];

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

typedef enum {
    MCOMM_COM_FUNCTION_ERROR       = 0,  // 0: Generic error or invalid function
    MCOMM_COM_FUNCTION_SUCCESS     = 1,  // 1: Operation success acknowledgment
    MCOMM_COM_FUNCTION_GETPH       = 2,  // 2: Get phone number / parameter
    MCOMM_COM_FUNCTION_SETPH       = 3,  // 3: Set phone number / parameter
    MCOMM_COM_FUNCTION_485TX       = 4,  // 4: Transmit data via RS-485
    MCOMM_COM_FUNCTION_485RX       = 5,  // 5: Receive data via RS-485
    MCOMM_COM_FUNCTION_232TX       = 6,  // 6: Transmit data via RS-232
    MCOMM_COM_FUNCTION_232RX       = 7,  // 7: Receive data via RS-232
    MCOMM_COM_FUNCTION_FWSTART     = 8,  // 8: Firmware update start command
    MCOMM_COM_FUNCTION_FWUPDATE    = 9,  // 9: Firmware update data transfer
    MCOMM_COM_FUNCTION_FWEND       = 10, // 10: Firmware update end confirmation
		MOMMM_COM_FUNCTION_VERSION		 = 11,
    MCOMM_COM_FUNCTION_COUNT       = 12  // 11: Total number of defined functions
} MCOMMComFunctionTypedef;

#define MCOMM_RX_FLAG_COUNT  (MCOMM_COM_FUNCTION_COUNT)

								
typedef enum {SRCH_FOR_ST=0,SRCH_FOR_FUNC,SRCH_FOR_ED,WAIT_FOR_PARS}RXSTATETypedef;


#define MCOMM_COM_URT_EXG_BUFF_SIZE   500
typedef struct 
{
    uint16_t datalen;
    uint8_t data[MCOMM_COM_URT_EXG_BUFF_SIZE];
}UartExchangetypedef;

typedef struct{
		char FirmwareVersion[10];
}FirmExchangetypedef;
#define MOTA_CHUCK_NUM_SIZE 2
// Firmware update chunk configuration
#define MCOMM_FWUPDATE_CHUNK_SIZE    256   // change this if you want a different chunk size
#define MCOMM_COM_LEN_EXTRAS 3
// FUNTION TOTAL LENGTHS

#define MCOMM_COM_LEN_ERROR_MCU         MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_SUCCESS_MCU       MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_GETPH_MDM         MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_GETPH_MCU         MCOMM_COM_LEN_EXTRAS+sizeof(PepheralTypedef)
#define MCOMM_COM_LEN_SETPH_MDM         MCOMM_COM_LEN_EXTRAS+sizeof(PepheralTypedef)
#define MCOMM_COM_LEN_485_MDM           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_485_MCU           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_232_MDM           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_232_MCU           MCOMM_COM_LEN_EXTRAS+sizeof(UartExchangetypedef)
#define MCOMM_COM_LEN_FWSTART           MCOMM_COM_LEN_EXTRAS+MOTA_CHUCK_NUM_SIZE
#define MCOMM_COM_LEN_FWUPDATE          MCOMM_COM_LEN_EXTRAS+MCOMM_FWUPDATE_CHUNK_SIZE+MOTA_CHUCK_NUM_SIZE
#define MCOMM_COM_LEN_FWEND             MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_VERSION						MCOMM_COM_LEN_EXTRAS
#define MCOMM_COM_LEN_VERREPLY					MCOMM_COM_LEN_EXTRAS+sizeof(FirmExchangetypedef)
// Explanation: MCOMM_COM_LEN_EXTRAS = header+function+footer (3), + 2 bytes chunk number, + chunk data TOTAL 261:


static const uint16_t LENTABLE[MCOMM_RX_FLAG_COUNT] = 
{
	MCOMM_COM_LEN_ERROR_MCU,
	MCOMM_COM_LEN_SUCCESS_MCU,
	MCOMM_COM_LEN_GETPH_MDM,
	MCOMM_COM_LEN_SETPH_MDM,
	MCOMM_COM_LEN_485_MDM,
	MCOMM_COM_LEN_485_MCU,
	MCOMM_COM_LEN_232_MDM,
	MCOMM_COM_LEN_232_MCU,
  MCOMM_COM_LEN_FWSTART,
	MCOMM_COM_LEN_FWUPDATE,
	MCOMM_COM_LEN_FWEND,
	MCOMM_COM_LEN_VERSION
};
// GET MVER  
// MCU VERSION - V1.2.6
void InitModemComm(void);
void ProcessModemComm(void);
void MCOMM_SendSerialData(uint8_t IsRS485, uint8_t *src, size_t len);
void SendDatatoModem(uint8_t *src, size_t len);

#endif


/* 
 * MComm.h - Modem Communication Protocol Header
 * 
 * PROTOCOL OVERVIEW:
 * - Frame-based communication: [HEADER | FUNCTION | DATA... | FOOTER]
 * - Header: 0x26, Footer: 0x7E
 * - Supports multiple function codes for different operations
 * 
 * KEY FEATURES:
 * 1. UART-based communication with modem (UART0, 115200 baud)
 * 2. Hardware control: FET and PWK pins for modem reset/power
 * 3. Multiple communication functions:
 *    - Phone number get/set (GETPH/SETPH)
 *    - RS485/RS232 data exchange
 *    - Firmware update protocol (FWSTART/FWUPDATE/FWEND)
 * 4. Fixed-length messaging with lookup table (LENTABLE)
 * 
 * BUFFER CONFIG:
 * - RX/TX buffers: 300 bytes each
 * - UART exchange buffer: 280 bytes
 * - Firmware update chunk size: 256 bytes
 * 
 * STATE MACHINE:
 * - SRCH_FOR_ST: Looking for start byte (0x26)
 * - SRCH_FOR_FUNC: Waiting for function code
 * - SRCH_FOR_ED: Receiving data until expected length
 * - WAIT_FOR_PARS: Frame complete, ready for processing
 */