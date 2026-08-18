#include "FMCUser.h"


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
	for(u32add = strAdd; u32add < (strAdd+size);u32add+=FMC_FLASH_PAGE_SIZE)
	{
		if(FMC_Erase(u32add)<0)
		{
			DelayMS(10);
			return;
		}
	}
	for(u32add = strAdd; u32add < (strAdd+size);u32add+=4)
	{
		u32data = FMC_Read(u32add);
		if(u32data!= 0xffffffff)
		{
			DelayMS(10);
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