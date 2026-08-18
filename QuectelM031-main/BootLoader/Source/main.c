/******************************************************************************
 * @file     main.c
 * @version  V1.00
 * @brief    Bootloader for M031LE3AE MCU.
 * @note  This Program is only for 128 kb flash ver m031 as it uses top 32kb as
					buffer for Flashing application, please take note of defined values in 
					Boot.h file.
 * Ayan Ansari
 * M.A INFOTECH PVT LTD.
*****************************************************************************/
#include <stdio.h>
#include "NuMicro.h"
#include "Boot.h"
#include "Hardware.h"
#include "FMCUser.h"
#include "W25Qxx.h"
#include <string.h>

#ifdef __ARMCC_VERSION
void __set_SP(uint32_t _sp)
{
	__asm("MSR MSP, r0");
	__asm("BX lr");
    
    
}
#endif

typedef void (FUNC_PTR)(void);
__IO uint32_t JumpAddress;
FUNC_PTR    *func;

__IO uint32_t TimingDelay;
__IO uint16_t Count1Sec;

uint8_t IsInit,FlashReq;

void DelayMS(__IO uint32_t nTime)
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
	Count1Sec++;
	if(Count1Sec >= 980)					//  1Sec Delay
	{
		Count1Sec=0;
	}
}

void SysTick_Handler(void)
{
	TimingDelay_Decrement();

}

uint8_t  set_IAP_boot_mode(void)
{
    uint32_t  au32Config[2];
    if (FMC_ReadConfig(au32Config, 2) < 0)
    {
        return 0;
    }

    /*
        CONFIG0[7:6]
        00 = Boot from LDROM with IAP mode.
        01 = Boot from LDROM without IAP mode.
        10 = Boot from APROM with IAP mode.
        11 = Boot from APROM without IAP mode.
    */
    if (au32Config[0] & 0x40)      /* Boot from APROM with IAP mode */
    {
        FMC_ENABLE_CFG_UPDATE();
        au32Config[0] &= ~0x40;
        FMC_WriteConfig(au32Config, 2);
        // Perform chip reset to make new User Config take effect
        SYS_ResetChip();//SYS->IPRST0 = SYS_IPRST0_CHIPRST_Msk;
				
    }
    return 1;
}

void InitSystem(void)
{
	IsInit = 0;
	SYS_UnlockReg();
	SYS_Init();
	RS232Init();
	NVIC_EnableIRQ(SysTick_IRQn);
	CLK_EnableModuleClock(ISP_MODULE);
	FMC_Open();
	set_IAP_boot_mode();
	W25QxxInit();                     // Initialize SPI and flash
	uint32_t id = W25Q_ReadID(); 
	
}

int main()
{
		
		uint8_t FDataBuff[22];
		InitSystem();
		RS232SendString("$RES,MCU Bootloader Start\r\n");
		Flashid = W25Q_ReadID();
		while(Flashid==0)
		{
			Flashid = W25Q_ReadID();
		}
		
//		sprintf((char*)FDataBuff, "[BOOT] ID: 0x%08lX\r\n", id);
//		    /* --- Read entire external SPI flash and dump via UART --- */
//    RS232SendData((uint8_t*)"\r\n[BOOT] Reading entire SPI flash...\r\n",
//                  strlen("\r\n[BOOT] Reading entire SPI flash...\r\n"));
//		uint8_t read_buf[FW_CHUNK_SIZE];
//    uint32_t addr = FW_FLASH_ADDR_BASE;
//    uint32_t end_addr = FW_FLASH_ADDR_BASE + FW_FLASH_SIZE;
//    char msg[64];
//		while (addr < end_addr)
//    {
//        // Read 256 bytes (one chunk)
//        W25Q_Read(read_buf, addr, FW_CHUNK_SIZE);

//        // Print base address for this line
//        sprintf(msg, "\r\n[0x%06lX]: ", addr);
//        RS232SendData((uint8_t*)msg, strlen(msg));

//        // Send data as hex, 16 bytes per line
//        for (uint16_t i = 0; i < FW_CHUNK_SIZE; i++)
//        {
//            char hex[4];
//            sprintf(hex, "%02X ", read_buf[i]);
//            RS232SendData((uint8_t*)hex, strlen(hex));

//            if (((i + 1) % 16) == 0 && (i + 1) < FW_CHUNK_SIZE)
//            {
//                RS232SendData((uint8_t*)"\r\n          ", strlen("\r\n          "));
//            }
//        }

//        addr += FW_CHUNK_SIZE;
//    }
		RS232SendString("$RES,Loading BootConfig\r\n");
		LoadBootConfig();

		RS232SendString("$RES,BootConfig Loaded... Jumping to APP\r\n");
    
		_JUMPTOAPP:
	//DelayMS(10);  // REMOVED DELAY CALL
//	DisablePeripherals();
	__disable_irq();
	//DisableSysTic();	
	//SYS->RSTSTS = 3; //clear bit
	CLK_EnableModuleClock(ISP_MODULE);//CLK->AHBCLK |= CLK_AHBCLK_ISPCKEN_Msk;
	
	//FMC->ISPCTL |= (FMC_ISPCTL_ISPEN_Msk | FMC_ISPCTL_APUEN_Msk);
	
	 /* Change vector page address to APROM */
            FMC_SetVectorPageAddr(ROM_APP_BASE_ADDR);
						SYS_ResetCPU();
						SYS_LockReg();
            /* Point to the reset handler address which application uses */
            func = (FUNC_PTR *) * (uint32_t *)(ROM_APP_BASE_ADDR + 4);
						//JumpAddress = (ROM_APP_BASE_ADDR+0xd4);
						//func= (FUNC_PTR*)JumpAddress;
            /* Set stack pointer base address to the one which application uses */
            __set_SP(*(uint32_t *)ROM_APP_BASE_ADDR);
            func();
   
    while(1);
	
}