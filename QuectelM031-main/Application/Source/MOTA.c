#include "MOTA.h"
#include "boot.h"

uint16_t expected_chunk_number = 0;
uint16_t total_chunks;
uint8_t confirm_frame[5];
uint8_t chbuff[256];

void FW_EraseFirmwareSectors(void)
{
	int size = FW_FLASH_SIZE;
	int i = 0;
	while(size)
	{
		W25Q_EraseSector(FW_FLASH_ADDR_BASE + (i * W25Q_SECTOR_SIZE));
		size -= W25Q_SECTOR_SIZE;
		i++;
	}
	
}
void ProcessFirmwareUpdateSM(void)
{
    // Exit immediately if no FWUPDATE activity
    if (!(FWUpdateFlags & (1UL << FW_FLAG_CHUNK_AVAILABLE))) 
    {
        return;
    }
    
    memcpy(confirm_frame, (uint8_t[]){0x26, MCOMM_COM_FUNCTION_FWUPDATE, MCOMMRxBuff[2], MCOMMRxBuff[3], 0x7E}, 5);
    
    if(((MCOMMRxBuff[2] << 8) | MCOMMRxBuff[3]) != expected_chunk_number) 
    {
        // ERROR: Wrong chunk sequence
        FW_SET(FW_FLAG_CHUNK_MISSING_ERROR);
        SendString("[FWUPDATE] ERROR: Chunk out of sequence\r\n");
        // Don't execute any flash operations
    }
    else 
    {
        // CORRECT CHUNK SEQUENCE - Execute all flash operations
        
        if (!(FWUpdateFlags & (1UL << FW_FLAG_UPDATE_IN_PROGRESS))) 
        {
								W25QxxInit();
////            uint32_t flash_id = W25Q_ReadID();
////            for(int i = 20; i >= 0; i -= 4) 
////            {
////                uint8_t nibble = (flash_id >> i) & 0x0F;
////                SendChar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
////            }  
////            
////            SendString("\r\n");   
//            W25Q_ChipErase(); 
            FW_SET(FW_FLAG_UPDATE_IN_PROGRESS);
        }
        
        FW_SET_CHUNK_NUMBER((MCOMMRxBuff[2] << 8) | MCOMMRxBuff[3]);
        uint16_t chunk = FW_GET_CHUNK_NUMBER();
        W25Q_Write(&MCOMMRxBuff[4],FW_FLASH_ADDR_BASE + (chunk * FW_CHUNK_SIZE), FW_CHUNK_SIZE);
        //W25Q_Read(chbuff, FW_FLASH_ADDR_BASE + (chunk * FW_CHUNK_SIZE), FW_CHUNK_SIZE);
        //UART_Write(MCOMM_UART, MCOMMRxBuff, FW_CHUNK_SIZE);
        
        // Send confirmation only for correct chunks
        SendDatatoModem(confirm_frame, 5);
				// INCREMENT for next expected chunk
				expected_chunk_number++;
				
				if (expected_chunk_number >= total_chunks)
				{
					uint16_t appsize = (uint16_t)(total_chunks * FW_CHUNK_SIZE);
					SetUpdateREQ(appsize);
					SendString("[FWUPDATE] All chunks received. Update request set in BootConfig.\r\n");
					//FW_ReadAllSPIFlash();
				}
    }
    
    // Always clear these flags (for both correct and error cases)
    FW_CLEAR(FW_FLAG_CHUNK_AVAILABLE);
    FW_CLEAR(FW_FLAG_UART_IRQ_DISABLED);
    UART_EnableInt(MCOMM_UART, UART_INTEN_RDAIEN_Msk);
}


void FW_ReadAllSPIFlash(void)
{
    SendString("[FWUPDATE] Reading entire SPI flash...\r\n");

    uint8_t read_buf[FW_CHUNK_SIZE];
    unsigned long addr = FW_FLASH_ADDR_BASE;
    uint32_t end_addr = FW_FLASH_ADDR_BASE + FW_FLASH_SIZE;
    char msg[64];

    while (addr < end_addr)
    {
        // Read 256 bytes (one chunk)
        W25Q_Read(read_buf, addr, FW_CHUNK_SIZE);

        // Print base address for this line
        sprintf(msg, "\r\n[0x%06lX]: ", addr);
        RS232SendData((uint8_t*)msg, strlen(msg));

        // Send data as hex, 16 bytes per line
        for (uint16_t i = 0; i < FW_CHUNK_SIZE; i++)
        {
            char hex[4];
            sprintf(hex, "%02X ", read_buf[i]);
            RS232SendData((uint8_t*)hex, strlen(hex));

            if (((i + 1) % 16) == 0 && (i + 1) < FW_CHUNK_SIZE)
            {
                RS232SendData((uint8_t*)"\r\n          ", strlen("\r\n          "));
            }
        }

        addr += FW_CHUNK_SIZE;
    }

    SendString("\r\n[FWUPDATE] SPI flash read complete.\r\n");
}

