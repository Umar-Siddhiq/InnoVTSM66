
#include "W25Qxx.h"
#include "MComm.h"

uint8_t TempChar;


extern void Delay(__IO uint32_t nTime);

typedef enum 
{
  RESET = 0U, 
  SET = !RESET
} FlagStatus, ITStatus;


static void W25QxxLowLevelInit(void)
{
	SPI_Open(SPI0, SPI_MASTER, SPI_MODE_0, 8, SPI_CLK_FREQ);
	GPIO_SetMode(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_MODE_OUTPUT);
	SPI_FLASH_CS_HIGH();
}

extern unsigned char SPI_FLASH_SendByte(unsigned char byte)
{
	unsigned char rt;
//	while(SPI_IS_BUSY(SPI0));
	while((SPI_GET_TX_FIFO_FULL_FLAG(SPI0)));
	SPI_WRITE_TX(SPI0,byte);
	//while(SPI_IS_BUSY(SPI0));
	while(SPI_GET_RX_FIFO_EMPTY_FLAG(SPI0)); 
	TempChar = SPI0->RX;
	rt = TempChar;
	return rt;
}

void W25QxxInit(void)
{
	W25QxxLowLevelInit();
	SPI_FLASH_CS_HIGH();  
	Delay(10);
	SPI_FLASH_CS_LOW();
	Delay(5);
	SPI_FLASH_SendByte(CMD_W25_WREN);
	SPI_FLASH_CS_HIGH();
}



void W25Q_WaitWriteEnd (void)
{
	uint8_t Flash_Status = 0;
	SPI_FLASH_CS_LOW(); 
	Flash_Status = SPI_FLASH_SendByte (0x05);
	Flash_Status = SPI_FLASH_SendByte (0xFF);
	while ((Flash_Status & 0x01) == SET)
	{
		Flash_Status = SPI_FLASH_SendByte (0xFF); // send the OxFF just for offering the clock to the slave
	}
	SPI_FLASH_CS_HIGH();
}

uint32_t W25Q_ReadID (void)
{
	uint32_t ID_Temp, temp_h, temp_m, temp_l;
	SPI_FLASH_CS_LOW();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte (0x9F);
	temp_h = SPI_FLASH_SendByte (0xFF);
	temp_m = SPI_FLASH_SendByte (0xFF);
	temp_l = SPI_FLASH_SendByte (0xFF);
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_HIGH();
	ID_Temp = (temp_h << 16) | (temp_m <<8) | temp_l ;
	return ID_Temp;
}

void W25Q_EraseSector (uint32_t SectorAddress)
{
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte(CMD_W25_RES);
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte(CMD_W25_WREN);
	SPI_FLASH_CS_HIGH();
	W25Q_WaitWriteEnd ();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte (0x20);
	SPI_FLASH_SendByte (SectorAddress >> 16);
	SPI_FLASH_SendByte (SectorAddress >> 8);
	SPI_FLASH_SendByte (00);
	SPI_FLASH_CS_HIGH();
	W25Q_WaitWriteEnd ();
}

void W25Q_EraseSectorsDecrement(void)
{
    uint32_t currentSector = FW_FLASH_ADDR_BASE + FW_FLASH_SIZE - W25Q_SECTOR_SIZE;
    uint32_t baseSector = FW_FLASH_ADDR_BASE;
    
    // Erase sectors from highest to base address
    while (currentSector >= baseSector) {
        W25Q_EraseSector(currentSector);
        currentSector -= W25Q_SECTOR_SIZE;
    }
}

void W25Q_ChipErase(void)
{
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte(CMD_W25_WREN);
	SPI_FLASH_CS_HIGH();
	W25Q_WaitWriteEnd ();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte (CMD_W25_CE);
	SPI_FLASH_CS_HIGH();
	
	W25Q_WaitWriteEnd ();
}

void W25Q_Read (unsigned char * pBuffer, uint32_t ReadAddress, unsigned long ReadByteNum)
{
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte (0x03);
	SPI_FLASH_SendByte (ReadAddress >> 16);
	SPI_FLASH_SendByte (ReadAddress >> 8);
	SPI_FLASH_SendByte (ReadAddress);
	while (ReadByteNum--)
	{
		*pBuffer = SPI_FLASH_SendByte (0xFF);
		pBuffer ++;
	}
	SPI_FLASH_CS_HIGH();

}

uint8_t CompareBuffer(unsigned char* buff1,unsigned char* buff2, uint16_t sz)
{
	uint16_t i;
	for(i=0;i<sz;i++)
	{
		if(buff1[i] != buff2[i])
			return 1;
		
	}
	return 0;
}



uint8_t W25Q_PageWrite (uint8_t * pBuffer, uint32_t PageAddress, uint16_t WriteByteNum)
{
	unsigned char comp[256];
	uint8_t* pbuff;
	pbuff = pBuffer;
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte(CMD_W25_RES);
	SPI_FLASH_CS_HIGH();
	W25Q_WaitWriteEnd();
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte(CMD_W25_WREN);
	SPI_FLASH_CS_HIGH();
	SPI_FLASH_CS_LOW();
	SPI_FLASH_SendByte (0x02);
	SPI_FLASH_SendByte (PageAddress >> 16);
	SPI_FLASH_SendByte (PageAddress >> 8);
	SPI_FLASH_SendByte (PageAddress);
	 
	if (WriteByteNum> 256) // the biggest num is 256
	{
	WriteByteNum = 256;
	}
	while (WriteByteNum--) 
	{
		SPI_FLASH_SendByte (* pBuffer);
		pBuffer ++;
	}
	SPI_FLASH_CS_HIGH();
	W25Q_WaitWriteEnd ();
	SPI_FLASH_CS_HIGH();
	W25Q_Read(comp,PageAddress,256);
	if(CompareBuffer(comp,pbuff,256))
		return 0;
	else
		return 1;
}

uint8_t W25Q_Write(unsigned char *pucBuffer, unsigned long ulWriteAddr, unsigned long ulNumByteToWrite)
{
	uint8_t resp;
	uint16_t pCount;
	uint16_t remainBytes;
	uint16_t i;
	if(ulWriteAddr >= W25Q_PageSize * W25Q_PAGES) return 0;
	if(ulNumByteToWrite > 256)
	{
		pCount=(ulNumByteToWrite / 256) + 1;
		remainBytes=256;
		for(i=0;i<pCount;i++)
		{
			resp = W25Q_PageWrite((uint8_t*)pucBuffer,ulWriteAddr,remainBytes);
			ulWriteAddr = ulWriteAddr + 256;
			pucBuffer = pucBuffer + 256;
			ulNumByteToWrite= ulNumByteToWrite - 256;
			if(ulNumByteToWrite > 256)
			{
				remainBytes=256;
				
			}
			else
				remainBytes=ulNumByteToWrite;
		}
	}
	else
		resp = W25Q_PageWrite((uint8_t*)pucBuffer,ulWriteAddr,256);
	
	return resp;
}

