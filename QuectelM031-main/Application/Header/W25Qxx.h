


#ifndef					_W25Q64_H
#define					_W25Q64_H

#include "NuMicro.h"

#define FLASH_ID								0x00EF4015

#define CMD_W25_WREN            0x06    /* Write Enable */
#define CMD_W25_WRDI            0x04    /* Write Disable */
#define CMD_W25_RDSR            0x05    /* Read Status Register */
#define CMD_W25_WRSR            0x01    /* Write Status Register */
#define CMD_W25_READ            0x03    /* Read Data Bytes */
#define CMD_W25_FAST_READ       0x0b    /* Read Data Bytes at Higher Speed */
#define CMD_W25_PP              0x02    /* Page Program */
#define CMD_W25_SE              0x20    /* Sector (4K) Erase */
#define CMD_W25_BE              0xd8    /* Block (64K) Erase */
#define CMD_W25_CE              0xc7    /* Chip Erase */
#define CMD_W25_DP              0xb9    /* Deep Power-down */
#define CMD_W25_RES             0xab    /* Release from DP, and Read Signature */

#define	W25Q_PageSize						0x100

#define SPI_CLK_FREQ    1000000
#define W25Q_SECTOR_SIZE        4096

#define	W25Q_PAGES							0x2000

#define 			FLASH_CS_PIN						BIT3
#define 			FLASH_CS_PORT						PA

#define 			SPI_FLASH_CS_HIGH()			PA3 = 1
#define 			SPI_FLASH_CS_LOW()			PA3 = 0



void W25QxxInit(void);
unsigned int W25Q_ReadID (void);
void W25Q_ChipErase(void);
void W25Q_EraseSector (uint32_t SectorAddress);
void W25Q_Read (unsigned char * pBuffer, uint32_t ReadAddress, unsigned long ReadByteNum);
uint8_t W25Q_Write(unsigned char *pucBuffer, unsigned long ulWriteAddr, unsigned long ulNumByteToWrite);
void W25Q_EraseSectorsDecrement(void);
extern unsigned char SPI_FLASH_SendByte(unsigned char byte);


#endif				//_W25Q64_H

