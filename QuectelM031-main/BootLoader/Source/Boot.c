#include "Boot.h"
#include "FMCUser.h"
#include "W25Qxx.h"   // add this line with other includes
#include "string.h"
#include "Hardware.h"

BootConfigTypedef BootConfig;
uint8_t 	FDataBuff[FMC_FLASH_PAGE_SIZE], RemainingPackets, tries;
uint16_t  NoOfPackets, CurrentPacket, Appsize;
uint32_t 	ADDR,  word, Rword, Cword;
uint16_t 	WordIndex;

void FlashNewApp(void);

void LoadBootConfig(void)
{		
	uint16_t Fsize;
	Fsize = sizeof(BootConfig);
	ReadFMCDataBuffer((uint8_t*)&BootConfig, FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR, Fsize);
		
	if(BootConfig.DefValue != FLASH_DEFAULT_VALUE)
	{
		BootConfig.DefValue = FLASH_DEFAULT_VALUE;
		goto savereturn;
	}
	
	BootConfig.BootCode = BCODE_NORMAL_OPERATION;
	
	if(BootConfig.UpdateREQCode == FLASH_UPDATE_REQ_CODE)
	{
		FlashNewApp();
	}
	BootConfig.UpdateREQCode = BCODE_NORMAL_OPERATION;
	savereturn:
	Fsize = sizeof(BootConfig);
	FMC_ENABLE_LD_UPDATE();
	EraseFMCData(FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR,Fsize);
	WriteFMCData((uint8_t*)&BootConfig, FMC_LDROM_BASE+FLASH_BOOT_CONFIG_ADDR, Fsize );
	FMC_DISABLE_LD_UPDATE();
	return;
}


void FlashNewApp(void)
{
    char msg[64]; // small buffer for debug messages
		if(Flashid==0 || Flashid == 0xffff || Flashid == 0xffffffff)
		{
			RS232SendString("$RES,!Flash newapp error , flash invalid\r\n");
			return;
		}


    Appsize = BootConfig.NewAppSize;
    sprintf(msg, "$RES,Flashing New App, size:%d", Appsize);
    RS232SendData((uint8_t*)msg, strlen(msg));

    if (Appsize == 0 || Appsize > 0xF000)
    {
        RS232SendString("$RES,!Flash newapp error , Invalid Appsize\r\n");
        return;
    }

    NoOfPackets = Appsize / 512;
    RemainingPackets = Appsize % 512;

    RS232SendString("$RES,Enabling AP Update\r\n");
    FMC_ENABLE_AP_UPDATE();

    for (CurrentPacket = 0; CurrentPacket < Appsize; CurrentPacket += 512)
    {
        unsigned long spi_addr = (uint32_t)FW_FLASH_ADDR_BASE + (uint32_t)CurrentPacket;
        unsigned long dest_addr = (uint32_t)ROM_APP_BASE_ADDR + (uint32_t)CurrentPacket;


        sprintf(msg,"$RES,Flashing block @0x%08lX from SPI addr 0x%08lX\r\n", dest_addr, spi_addr);
        RS232SendData((uint8_t*)msg, strlen(msg));

        // Read 512 bytes (256 + 256)
        W25Q_Read(FDataBuff, spi_addr, 256);
        if ((Appsize - CurrentPacket) >= 256)
            W25Q_Read(FDataBuff + 256, spi_addr + 256, ((Appsize - CurrentPacket) >= 512) ? 256 : (Appsize - CurrentPacket - 256));
        else
            memset(FDataBuff + 256, 0xFF, 256);

        FMC_Erase(dest_addr);

        for (WordIndex = 0; WordIndex < 512; WordIndex += 4)
        {
            word = ((uint32_t)FDataBuff[WordIndex + 3] << 24) |
                   ((uint32_t)FDataBuff[WordIndex + 2] << 16) |
                   ((uint32_t)FDataBuff[WordIndex + 1] << 8) |
                   ((uint32_t)FDataBuff[WordIndex + 0]);
            FMC_Write(dest_addr, word);
            dest_addr += 4;
        }
    }

    RS232SendString("$RES,Flash write complete, verifying...\r\n");
    // Verify loop
    for (CurrentPacket = 0; CurrentPacket < Appsize; CurrentPacket += 512)
    {
        uint32_t spi_addr = (uint32_t)FW_FLASH_ADDR_BASE + (uint32_t)CurrentPacket;
        uint32_t check_addr = (uint32_t)ROM_APP_BASE_ADDR + (uint32_t)CurrentPacket;

        W25Q_Read(FDataBuff, spi_addr, 256);
        if ((Appsize - CurrentPacket) >= 256)
            W25Q_Read(FDataBuff + 256, spi_addr + 256, ((Appsize - CurrentPacket) >= 512) ? 256 : (Appsize - CurrentPacket - 256));
        else
            memset(FDataBuff + 256, 0xFF, 256);

        for (WordIndex = 0; WordIndex < 512; WordIndex += 4)
        {
            word = ((uint32_t)FDataBuff[WordIndex + 3] << 24) |
                   ((uint32_t)FDataBuff[WordIndex + 2] << 16) |
                   ((uint32_t)FDataBuff[WordIndex + 1] << 8) |
                   ((uint32_t)FDataBuff[WordIndex + 0]);
            Rword = FMC_Read(check_addr);
            if (Rword != word)
            {
                sprintf(msg, "$RES,!Flash verify error at addr 0x%08X\r\n", check_addr);
                RS232SendData((uint8_t*)msg, strlen(msg));
                BootConfig.UpdateREQCode = FLASH_UPDATE_REQ_CODE;
                BootConfig.BootCode = BCODE_UPDATE_ERR_VERS;
                FMC_DISABLE_AP_UPDATE();
                RS232SendString("$RES,!Flash update failed\r\n");
                return;
            }
            check_addr += 4;
        }
    }

    RS232SendString("$RES,Verification OK\r\n");
    RS232SendString("$RES,Disabling AP update\r\n");

    FMC_DISABLE_AP_UPDATE();

    BootConfig.UpdateREQCode = 0;
    BootConfig.CurrentAppSize = BootConfig.NewAppSize;
    BootConfig.BootCode = BCODE_FLASHED_NEW;

    RS232SendString("$RES,Flash update successful\r\n");
}

