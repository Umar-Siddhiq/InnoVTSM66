#ifndef _FILE_H 
#define _FILE_H

#include "VTS.h"
#include "Hardware.h"
#include "Utilities.h"
#include "ql_fs.h"
#include "Systic.h"
#include "Server.h"
//#define TEST
#ifndef TEST
#define     DEFVAL                  0xA5A2
#else
#define     DEFVAL                  0xB5C1
#endif

#define     DEFSTATE               0xA4A1


#define     CONFIG_FILE_PATH        "UFSConfig.bin\0"
#define     FOTA_CONFIG_PATH        "FotaConfig.bin\0"
#define     STATE_FILE_PATH         "State.bin\0"


void LoadConfig(void);
void LoadState(void);
void LoadDefault(void);
void LoadDefaultState(void);
void UpdateConfigInFlash(void);
void UpdateStateInFlash(void);
uint8_t LoadFromFlash(char *filename, void *data, u32 size, void (*defaultFunc)(void));
uint8_t SaveToFlash(char *filename, void *data, u32 size);


#endif /* _FILE_H */