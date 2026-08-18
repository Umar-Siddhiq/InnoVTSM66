#ifndef _BOOT_H_
#define _BOOT_H_

#include "NuMicro.h"


#define FLASH_SIZE 						0x20000UL // 128kb

#define FLASH_NEWAPP_ADDR 		0x14000UL
#define FLASH_NEWAPP_SIZE 		0xC000UL

#define FLASH_DEFAPP_ADDR			0x2000UL
#define	FLASH_DEFAPP_SIZE			0xC000UL

#define FLASH_UPDATE_REQ_CODE               0xA5A3
#define FLASH_DEFAULT_VALUE									0x5A3A

#define FLASH_BOOT_CONFIG_ADDR	0x400UL

#define ROM_APP_BASE_ADDR     0x00002000UL

/*************************************
 *************BOOT CODE LIST*********
 ************************************/

#define     BCODE_APP_WORKING       0x11
#define     BCODE_NORMAL_OPERATION  0x15
#define     BCODE_FLASHED_DEFAULT   0x23
#define     BCODE_NO_FLASH          0x55
#define		  BCODE_NO_APPLICATION 		0x5A
#define			BCODE_UPDATE_ERR_VERS   0xA5
#define     BCODE_FLASHED_NEW   		0xAA

 /*************************************
 */ 


typedef struct
{
	uint8_t DefaultAppVer;
	uint16_t DefaultAppSize;
	uint8_t CurrentAppVer;
	uint16_t CurrentAppSize;
	uint16_t UpdateREQCode;
	uint8_t NewAppVer;
	uint16_t NewAppSize;
	uint16_t BootCode;
	uint16_t DefValue;
} BootConfigTypedef;

extern BootConfigTypedef BootConfig;
void LoadBootConfig(void);

#endif