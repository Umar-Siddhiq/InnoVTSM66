#ifndef								_FMCU_H
#define								_FMCU_H


#include "NuMicro.h"
#include "Hardware.h"

int set_IAP_boot_mode(void);
void ReadFMCDataBuffer(uint8_t* rxBuff, uint32_t strAdd ,uint32_t size);
void EraseFMCData(uint32_t strAdd, uint32_t size);
void WriteFMCData(uint8_t* DataBuff,uint32_t strAdd, uint32_t size);

#endif