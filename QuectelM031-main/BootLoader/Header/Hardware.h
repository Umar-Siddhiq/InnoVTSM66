#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include "NuMicro.h"
#include <stdio.h>
#include <stdbool.h> 

#define RS232_UART UART1
#define RS232_UART_BAUD 38400
#define RS232_UART_SYS_RST UART1_RST
#define RS232_UART_IRQ UART1_IRQn
#define RS232_RX_BUFF_SIZE 250



void DelayMS(__IO uint32_t nTime);
void SYS_Init(void);
void RS232Init(void);
void RS232SendData(uint8_t *src, size_t len);
void RS232SendString(char* src);

#endif