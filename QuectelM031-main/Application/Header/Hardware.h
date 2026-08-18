
#ifndef					_HARDWARE_H
#define					_HARDWARE_H

#include <stdio.h>
#include "NuMicro.h"
#include <stdbool.h>
void SendChar(char ch);
void SendString(const char *str);


#define							IP1_PIN						BIT8
#define							IP1_PORT					PA
#define							IP1_VAL						PA8

#define							IP2_PIN						BIT9
#define							IP2_PORT					PA
#define							IP2_VAL						PA9

#define 						ADC1_PORT					PB
#define							ADC1_PIN					BIT1

#define 						ADC2_PORT					PB
#define							ADC2_PIN					BIT0

#define 						VMAIN_PORT					PB
#define							VMAIN_PIN					BIT13

#define 						VSEN_PORT					PB
#define							VSEN_PIN					BIT14




typedef struct
{
	uint8_t IP1;
	uint8_t IP2;
	uint16_t MainVolt;
	uint16_t BattVolt;
	uint16_t ADCVal1;
	uint16_t ADCVal2;
	uint8_t IsFlash;
	uint8_t IsMems;
	uint8_t IsTilt;
}PepheralTypedef;

#define RS485_UART UART2
#define RS485_UART_BAUD 38400
#define RS485_UART_SYS_RST UART2_RST
#define RS485_UART_IRQ UART02_IRQn
#define RS485_DIR_PORT PB
#define RS485_DIR_PIN BIT7
#define RS485_DIR PD2
#define RS485_DIR_EN	PB7=1
#define RS485_DIR_DIS 	PB7=0
#define RS485_RX_BUFF_SIZE 250

#define RS232_UART UART1
#define RS232_UART_BAUD 38400
#define RS232_UART_SYS_RST UART1_RST
#define RS232_UART_IRQ UART1_IRQn
#define RS232_RX_BUFF_SIZE 250

#define CELL_CNT_PORT	PC
#define CELL_CNT_PIN	BIT3
#define CELL_CNT	PC3


extern uint8_t RS485RxBuff[];
extern volatile uint16_t RS485RxCount;
extern volatile bool RS485RxActive;
extern volatile uint16_t RS485RxTimeout;

extern uint8_t RS232RxBuff[];
extern volatile uint16_t RS232RxCount;
extern volatile bool RS232RxActive;
extern volatile uint16_t RS232RxTimeout;




extern PepheralTypedef PeriPheralVal;

//  ---------------------------------------------------------
// Prototype Declaration
//

void Delay(__IO uint32_t nTime);
void SYS_Init(void);
void PeripheralInit(void);
void ProcessPeripheral(void);
void RS485SendData(uint8_t *src, size_t len);
void RS232SendData(uint8_t *src, size_t len);
void RS232SendString(char* str);
void RS485_RxByteHandler(uint8_t byte);
void RS232_RxByteHandler(uint8_t byte);


#endif

