#ifndef _PKTSAVE_H
#define _PKTSAVE_H


#include "project.h"

#define PKTCOUNT_DEF 0xA5A6
#define PKTCOUNT_LOC "BkCount.bin"
#define PKT_NAME_HEADER "BK_"
#define PKT_NAME_FOOTER ".bin"

#define MAX_PACKET_COUNT    256

extern uint16_t HistoryPacketCount;



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
#endif