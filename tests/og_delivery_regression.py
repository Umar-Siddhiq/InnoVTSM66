"""Run production OG history/delivery functions with mocked transport/storage.

Usage: python tests/og_delivery_regression.py (requires host gcc).
Hardware acceptance steps are in tests/OG_DELIVERY_BENCH.md.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "custom/Server.c").read_text()


def function(signature):
    start = source.index(signature + "\n{") if signature + "\n{" in source else source.index(signature + " {")
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


history = "\n".join(function(s) for s in (
    "void ChangeToHistoryPacket(char *buf)",
    "void ChangeToHistoryEPB(char *buf)",
    "void ProcessHistoryPacket(void)",
))
normal = function("static void handleNormalPackets(void)")
# Compile the OG branch in isolation; the later legacy body belongs to other protocols.
normal = normal[:normal.index("\n\tif(!GSM.IsTimeSet)")] + "\n}"
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PROTO_OG
#define HISTORY_INTERNAL
#define DATA_MAX_BUFF 1024
#define SOCKET_CONNECTED 6
#define OTA_SRC_SCK_1 1
#define OTA_SRC_SCK_2 2
#define OTA_SRC_SCK_3 3
#define OTA_SRC_SCK_4 4
#define OTA_ACK_WAIT_TICKS 300
#define Ql_strstr strstr
#define Ql_strlen strlen
#define Ql_sprintf sprintf
#define Ql_memcpy memcpy
#define Ql_memset memset
#define LOGData(...) ((void)0)
char dataBuffer[DATA_MAX_BUFF], stored[DATA_MAX_BUFF], delivered[DATA_MAX_BUFF];
typedef struct { int SocketState, SocketNo; } TCPSocketTypedef;
TCPSocketTypedef ServerSocket[4];
struct { int Pending, Channel; char Source[64]; } LastOTAResponse;
int AIS140SocketReinitPending, AIS140ResetPending;
char AIS140ResetReason[32];
void InitSockets(void) {}
void SystemRecovery_RequestReset(char *reason) {}
void ThreadSleep(int ms) {}
void CheckAlerts(void) {}
static void handleHealthPackets(void) {}
struct { int LastPkt; } PacketConfig;
struct { struct { char IP2[50]; } ServerData; } VTSData;
struct { int IsTimeSet; } GSM;
struct { int IsSOS, IsSOSSMS; } SOS;
struct { int IsNormalPacket, IsHistoryPacket, IsHealthPacket; } IsPacketReady;
uint16_t StoredHistoryDataCount;
int MemoryPercent, deleted, saves, sends, target, fail_send, sms_calls, sms_success, pvt_calls;
uint8_t GetXORChecksum(const char *s, int n) {
    uint8_t x=0; while(n-->0) x^=*s++; return x;
}
void CheckPacketCount(void) {}
void ReadLastPacket(void) { strcpy(dataBuffer, stored); }
void DeleteLastPacket(void) { deleted++; PacketConfig.LastPkt--; }
void SavePacket(void) { saves++; strcpy(stored,dataBuffer); }
uint8_t TCPSocket_SendString(void *socket, char *data) {
    int i; for(i=0;i<4;i++) if(socket==&ServerSocket[i]) break;
    if(i==4 || ServerSocket[i].SocketState!=SOCKET_CONNECTED || fail_send) return 0;
    target=i; sends++; strcpy(delivered,data); return 1;
}
void EmergencyPacket(uint8_t on) { strcpy(dataBuffer,"$EPB,EMR,868329083285894,NM,08092026160804,A,18.524242,N,73.815533,E*00\r\n"); }
void InitBuffer(uint8_t type) {
    pvt_calls++; strcpy(dataBuffer,type==12 ? "OA,12" : "PVT");
    if(type==12) LastOTAResponse.Pending=0;
}
void SendDatatoServer0(void) {}
uint8_t SendSOSSMS(uint8_t fall) { sms_calls++; return sms_success; }
void reset(void) {
    memset(ServerSocket,0,sizeof(ServerSocket)); memset(&SOS,0,sizeof(SOS));
    memset(&VTSData,0,sizeof(VTSData));
    deleted=saves=sends=fail_send=sms_calls=sms_success=pvt_calls=0;
    GSM.IsTimeSet=1; PacketConfig.LastPkt=1;
}
void valid_checksum(const char *packet) {
    const char *star=strchr(packet,'*'); unsigned cs;
    assert(star && sscanf(star+1,"%2x",&cs)==1);
    assert(cs==GetXORChecksum(packet+1,star-packet-1));
}
'''
program += history + "\n" + normal + "\n" + function("static void handlePackets(void)")
program += r'''
int main(void) {
    reset();
    strcpy(stored,"$PVT,AKEN,01.05.09,NR,1,L,868329083285894,");
    memset(stored+strlen(stored),'0',350);
    strcat(stored,",()*00\r\n");
    ServerSocket[0].SocketState=SOCKET_CONNECTED;
    ProcessHistoryPacket();
    assert(deleted==1 && sends==1 && strlen(delivered)>256);
    assert(strstr(delivered,",NR,2,H,")); valid_checksum(delivered);
    ChangeToHistoryPacket(delivered); valid_checksum(delivered);
    strcpy(dataBuffer,"$PVT,AKEN,01.05.09,EA,10,L,868329083285894,()*00\r\n");
    ChangeToHistoryPacket(dataBuffer);
    assert(strstr(dataBuffer,",EA,10,H,")); valid_checksum(dataBuffer);
    reset(); EmergencyPacket(1); strcpy(stored,dataBuffer);
    ServerSocket[1].SocketState=SOCKET_CONNECTED;
    ProcessHistoryPacket();
    assert(deleted==1 && sends==1 && target==1);
    assert(strstr(delivered,",SP,")); valid_checksum(delivered);
    reset(); EmergencyPacket(1); strcpy(stored,dataBuffer);
    ServerSocket[0].SocketState=SOCKET_CONNECTED;
    ProcessHistoryPacket(); assert(deleted==0 && sends==0);
    strcpy(VTSData.ServerData.IP2,"NA");
    ProcessHistoryPacket(); assert(deleted==1 && target==0);
    reset(); SOS.IsSOS=1; ServerSocket[0].SocketState=SOCKET_CONNECTED;
    handleNormalPackets(); assert(saves==1 && sms_calls==1 && !SOS.IsSOSSMS);
    sms_success=1; handleNormalPackets(); assert(sms_calls==2 && SOS.IsSOSSMS);
    handleNormalPackets(); assert(sms_calls==2 && saves==3);
    reset(); SOS.IsSOS=1; ServerSocket[1].SocketState=SOCKET_CONNECTED;
    handleNormalPackets(); assert(sends==1 && target==1 && saves==0 && sms_calls==0);
    fail_send=1; handleNormalPackets(); assert(saves==1 && sms_calls==1);
    reset(); SOS.IsSOS=1; GSM.IsTimeSet=0;
    handleNormalPackets(); assert(saves==0 && sms_calls==1 && pvt_calls==0);
    reset(); ServerSocket[0].SocketState=SOCKET_CONNECTED;
    LastOTAResponse.Pending=1; LastOTAResponse.Channel=OTA_SRC_SCK_1;
    strcpy(LastOTAResponse.Source,"78.46.1.2:12345");
    handlePackets(); assert(!LastOTAResponse.Pending && target==0 && !strcmp(delivered,"OA,12"));
    reset(); ServerSocket[2].SocketState=SOCKET_CONNECTED;
    LastOTAResponse.Pending=1; LastOTAResponse.Channel=OTA_SRC_SCK_2;
    handlePackets(); assert(!LastOTAResponse.Pending && target==2);
    LastOTAResponse.Pending=1; fail_send=1;
    handlePackets(); assert(LastOTAResponse.Pending);
    reset(); LastOTAResponse.Pending=1; LastOTAResponse.Channel=OTA_SRC_SCK_1;
    IsPacketReady.IsNormalPacket=1;
    handlePackets(); assert(LastOTAResponse.Pending && pvt_calls==1);
    puts("OG delivery regression: all assertions passed"); return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="og_delivery_") as tmp:
    c = Path(tmp) / "regression.c"
    exe = Path(tmp) / "regression.exe"
    c.write_text(program)
    subprocess.run(["gcc", "-std=c99", "-Wall", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
