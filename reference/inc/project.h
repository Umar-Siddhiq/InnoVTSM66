#ifndef _PROJECT_H
#define _PROJECT_H

#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_adpt.h"
#include "nwy_osi_api.h"
#include "nwy_fota_api.h"

#include "Systic.h"
#include "GPRS.h"
#include "TCP.h"
#include "UDP.h"
#include "MCU.h"
#include "VTS.h"
#include "Server.h"
#include "SOS.h"
#include "Alert.h"
#include "Utilities.h"
#include "Hardware.h"
#include "SMS.h"
#include "PktSave.h"
#include "FTP.h"
#include "NTP_Custom.h"

//#define TEST

#define     CONFIG_FILE_PATH        "Config.bin"
#define     FOTA_CONFIG_PATH        "FotaConfig.bin"
#define     STATE_FILE_PATH         "State.bin"
#define     SENSOR_CONFIG_PATH      "SensorConfig.bin"

#ifndef TEST
#define     DEFVAL                  0xA5A5
#else
#define     DEFVAL                  0xB5C1
#endif

#define     DEFSTATE                0xCAAB 

extern uint8_t IsDebug;
extern uint8_t stkCb;
extern uint8_t PrfChanged;
extern uint8_t Vat_init,MemoryPercent,IsNeigh;


//-----------PROJECT MACROS--------------//
//#define         ASTRO_SUPPORT

#ifdef ASTRO_SUPPORT
#include "SunPos.h"
#endif

void nwy_dbg_log(char *fmt, ...);
void print_long_string(const char* long_string);
void UartStringOut(char *fmt, ...);
void LowerString(char	*str);
void UpdateConfigInFlash(void);
void UpdateStateInFlash(void);
void UpdateFOTAConfigInFlash(void);
void LoadConfig(void);
void LoadState(void);
void LoadFOTAConfig(void);
void InitSockets(void);
uint8_t LoadServerStringToSocket(TCPSocketTypedef *socket, char *serverstring);
extern nwy_osi_thread_t nwy_server_thread;



#define USE_DATA
#define USE_TCP
#ifdef USE_TCP
    #define _DATASEND
#endif


extern nwy_osi_thread_t nwy_systic_thread;
extern nwy_osi_thread_t nwy_test_cli_thread;
extern nwy_osi_thread_t nwy_mcu_thread;
extern nwy_osi_thread_t nwy_gprs_thread;
extern nwy_osi_thread_t nwy_gps_thread;
extern nwy_osi_thread_t IMU_thread;
#ifdef USE_TCP
extern nwy_osi_thread_t nwy_tcp_thread1;
extern nwy_osi_thread_t nwy_tcp_thread2;
extern nwy_osi_thread_t nwy_tcp_thread3;
#endif
extern nwy_osi_thread_t nwy_dht11_thread;

typedef void (*ptr)();
void CallBack(ptr cb);
#endif