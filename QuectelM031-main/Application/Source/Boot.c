#include "Boot.h"
#include "FMCUser.h"

BootConfigTypedef BootConfig;
uint8_t FirmWareUpdated;


void SetUpdateREQ(uint16_t Appsize)
{
	uint16_t Fsize;
	SYS_UnlockReg();
	FMC_Open();
	FMC_ENABLE_LD_UPDATE();
	Fsize = sizeof(BootConfig);
	ReadFMCDataBuffer((uint8_t*)&BootConfig,FLASH_BOOT_CONFIG_ADDR+FMC_LDROM_BASE,Fsize);
	if(BootConfig.DefValue != FLASH_DEFAULT_VALUE)
		return;
	
	BootConfig.UpdateREQCode = FLASH_UPDATE_REQ_CODE;
	BootConfig.NewAppSize = Appsize;
	EraseFMCData(FLASH_BOOT_CONFIG_ADDR+FMC_LDROM_BASE,Fsize);
	WriteFMCData((uint8_t*)&BootConfig,FLASH_BOOT_CONFIG_ADDR+FMC_LDROM_BASE,Fsize);
	FMC_DISABLE_LD_UPDATE();
	FMC_Close();
	SYS_LockReg();

}

void JumpToBOOT()
{
	Delay(100);
	__disable_irq();

	SYS_UnlockReg();
	
	CLK_EnableModuleClock(ISP_MODULE);//CLK->AHBCLK |= CLK_AHBCLK_ISPCKEN_Msk;
	FMC_Open();
  //FMC->ISPCON |= (FMC_ISPCON_ISPEN_Msk);
	 /* Change vector page address to APROM */
	FMC_SetVectorPageAddr(FMC_APROM_BASE);
	
	NVIC_SystemReset();

}

void EraseNEWAPP(void)
{
	SYS_UnlockReg();
	FMC_Open();
	FMC_ENABLE_AP_UPDATE();
	EraseFMCData(FLASH_NEWAPP_ADDR,FLASH_NEWAPP_SIZE);
	FMC_DISABLE_AP_UPDATE();
	FMC_Close();
	SYS_LockReg();
	
}

void LoadBootConfig(void)
{		
	//BootConfigTypedef Boot;
	
	uint16_t Fsize;
	SYS_UnlockReg();
	FMC_Open();
	Fsize = sizeof(BootConfig);
	ReadFMCDataBuffer((uint8_t*)&BootConfig, FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR, Fsize);	
	if(BootConfig.DefValue == FLASH_DEFAULT_VALUE)
	{

		if(BootConfig.BootCode == BCODE_FLASHED_NEW)
			FirmWareUpdated=1;
		
		BootConfig.BootCode = BCODE_APP_WORKING;
		FMC_ENABLE_LD_UPDATE();
		EraseFMCData(FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR,Fsize);
		WriteFMCData((uint8_t*)&BootConfig, FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR, Fsize );
		ReadFMCDataBuffer((uint8_t*)&BootConfig, FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR, Fsize);	
		FMC_DISABLE_LD_UPDATE();

	
	}
	FMC_Close();
	SYS_LockReg();
	
	return;
}


