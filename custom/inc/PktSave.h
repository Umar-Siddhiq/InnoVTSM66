#ifndef _PKTSAVE_H
#define _PKTSAVE_H


#include "VTS.h"
#include "File.h"


#define PKTCOUNT_DEF 0xA5A6
#define PKTCOUNT_LOC "BkCount.bin"
#define PKT_NAME_HEADER "BK_"
#define PKT_NAME_FOOTER ".bin"

#define MAX_PACKET_COUNT    256

extern uint16_t HistoryPacketCount;

typedef struct 
{
    uint8_t IsEnabled;
    uint16_t PacketCount;
    _RTC FTK_LastPacketTime;
    uint16_t Interval;
    double Lat;
    double Long;
    double Speed;
    double Altitude;
    double HDOP;
    double PDOP;
    double Heading;
    uint8_t Noofsats;
}FTKConfigtypedef;
extern FTKConfigtypedef FTKConfig;


typedef struct 
{
    uint16_t DefData;
    uint16_t LastPkt;
}PacketConfigtypedef;


extern PacketConfigtypedef PacketConfig;
void CheckPacketCount(void);
void SavePacket(void);
void DeleteLastPacket(void);
void ReadLastPacket(void);
uint8_t ReadPacket(uint16_t pktnum);
void DeleteAllPackets(void);



uint8_t EnableFTKLogs(uint16_t interval);
void FTK_ApplyVariation(FTKConfigtypedef *config);
_RTC FTK_DeductTime(_RTC currentTime, uint32_t secondsToDeduct);
#endif