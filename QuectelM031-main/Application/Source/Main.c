/**************************************************************************//**
 * @file     main.c
 * @version  V1.00

 *
 * SPDX-License-Identifier: Apache-2.0
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "NuMicro.h"
#include "Hardware.h"
#include "FMCUser.h"
#include "Boot.h"
#include "MComm.h"
#include "W25Qxx.h"



#define SERVER_HANG_TIME				15
#define	DEFVAL							    0x1AAC
#define FLASH_CONFIG_COUNT			512
#define	FLASH_CONFIG_ADDR		 		0x000000 			// 0x1FE000 // 0X7F0000


__IO uint32_t TimingDelay;
__IO uint16_t Count1Sec, Count1Min, LED_Count;



uint8_t IsInit;
uint16_t Fsize;
uint32_t FlashID;
uint16_t IsFlash;



void LoadConfig(void);
void UpdateConfigInFlash(void);


void Delay(__IO uint32_t nTime)
{
  TimingDelay = nTime;

   while(TimingDelay != 0);
}


void TimingDelay_Decrement(void)
{
	if (TimingDelay != 0x00)
	{
		TimingDelay--;
	}
	if(RS485RxTimeout)
	{
		RS485RxTimeout--;
	}
	if(RS232RxTimeout)
	{
		RS232RxTimeout--;
	}
	Count1Sec++;
	if(Count1Sec >= 980)					//  1Sec Delay
	{
		Count1Sec=0;
		if(IsInit)
		{
			Count1Min++;
			
			if(Count1Min > 59)
			{
				
				Count1Min=0;
			}
		}
	}	
}

void SysTick_Handler(void)
{
	TimingDelay_Decrement();
}


//----------------------------------------------------------------------//
// Init System
//----------------------------------------------------------------------//

void Init(void)
{
	/* Init System, IP clock and multi-function I/O. */
  SYS_Init();
	__enable_irq();
	IsInit=0;
	NVIC_EnableIRQ(SysTick_IRQn);
	PeripheralInit();
	LoadBootConfig();
	//W25QxxInit();
	FW_EraseFirmwareSectors();
	InitModemComm();
	IsInit=1;
}

int main(void)
{
    Init();  	
    // Send sample data to test UART
    RS232SendString("$RES,MCU APPLICATION Start\r\n");
	
    //W25Q_EraseSectorsDecrement();
    while(1)
    {
				
				ProcessPeripheral();
				ProcessModemComm();
				LSM6DSOX_MonitorTilt();
    }
}
