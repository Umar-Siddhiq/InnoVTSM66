#include "FMCUser.h"
#ifdef PAGE_SIZE_2048
	#define FMC_PAGE_SIZE   2048
#else
	#define FMC_PAGE_SIZE   512
#endif


int set_IAP_boot_mode(void)
{
    uint32_t  au32Config[2];

    if (FMC_ReadConfig(au32Config, 2) < 0)
    {
        //printf("\nRead User Config failed!\n");
        return -1;
    }

    /*
        CONFIG0[7:6]
        00 = Boot from LDROM with IAP mode.
        01 = Boot from LDROM without IAP mode.
        10 = Boot from APROM with IAP mode.
        11 = Boot from APROM without IAP mode.
    */
    if (au32Config[0] & 0x40)          /* Check if it's boot from APROM/LDROM with IAP. */
    {
        FMC_ENABLE_CFG_UPDATE();       /* Enable User Configuration update. */
        au32Config[0] &= ~0x40;        /* Select IAP boot mode. */

        if (FMC_WriteConfig(au32Config, 2) != 0) /* Update User Configuration CONFIG0 and CONFIG1. */
        {
            //printf("FMC_WriteConfig failed!\n");
            return -1;
        }
        SYS->IPRST0 = SYS_IPRST0_CHIPRST_Msk;
    }
    return 0;
}


void ReadFMCDataBuffer(uint8_t* rxBuff, uint32_t strAdd ,uint32_t size)
{
	uint32_t u32add;
	uint32_t u32data;
	
	
	for(u32add = strAdd;u32add < (strAdd + size); u32add+=4)
	{
		u32data = FMC_Read(u32add);
		rxBuff[u32add-strAdd] = 	(u32data & 0xff000000) >> 24;
		if((u32add-strAdd+1) < size)
			rxBuff[u32add-strAdd+1] = (u32data & 0x00ff0000) >> 16;
		
		
		if((u32add-strAdd+2) < size)
			rxBuff[u32add-strAdd+2] = (u32data & 0x0000ff00) >> 8;
		
		
		if((u32add-strAdd+3) < size)
			rxBuff[u32add-strAdd+3] = (u32data & 0x000000ff);
		
	}
	
}

void EraseFMCData(uint32_t strAdd, uint32_t size)
{
	uint32_t u32add;
	uint32_t u32data;
	for(u32add = strAdd; u32add < (strAdd+size);u32add+=FMC_PAGE_SIZE)
	{
		if(FMC_Erase(u32add)<0)
		{
			//DelayMS(10);  // REMOVED DELAY CALL
			return;
		}
	}
	for(u32add = strAdd; u32add < (strAdd+size);u32add+=4)
	{
		u32data = FMC_Read(u32add);
		if(u32data!= 0xffffffff)
		{
			//DelayMS(10);  // REMOVED DELAY CALL
			return;
		}
	}
	
}

void WriteFMCData(uint8_t* DataBuff,uint32_t strAdd, uint32_t size)
{
	uint32_t u32add;
	uint32_t u32data;
	for(u32add = strAdd;u32add < (strAdd + size); u32add+=4)
	{
		u32data = DataBuff[u32add - strAdd] << 24;
		if((u32add - strAdd+1) <size)
			u32data|= DataBuff[u32add-strAdd+1] << 16;
		if((u32add - strAdd+2) <size)
			u32data|= DataBuff[u32add-strAdd+2] << 8;
		if((u32add - strAdd+3) <size)
			u32data|= DataBuff[u32add-strAdd+3];
		
		FMC_Write(u32add, u32data);
	}
}