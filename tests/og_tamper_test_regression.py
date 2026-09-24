"""Compile actual OG test-rig dispatch/delivery with mocked sockets (host gcc)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
server = (root / "custom/Server.c").read_text()
sms = (root / "custom/SMS.c").read_text()
start = server.index("static uint8_t SOSTamperTestPending")
end = server.index("\n#endif\n\nvoid CheckAlerts", start)
delivery = server[start:end]
start = sms.index("\t\t\t\t#ifdef PROTO_OG", sms.index('"activating alert %d"'))
end = sms.index("\n\t\t\t\treturn 1;", sms.index("\t\t\t\tAddAlert(i);", start))
dispatch = "int dispatch(int i) {\n" + sms[start:end] + "\nreturn 1;\n}"
start = server.index("\tif(VAlert[SOS_TMP_ALERT].Enable)", server.index("void CheckAlerts(void)\n{"))
end = server.index("\n\tif(VAlert[MAINS_FAIL_ALERT]", start)
physical = "void physical(void) {\n" + server[start:end] + "\n}"
assert "HandleSOSTamperTest();" in server[server.index("static void handlePackets(void) {"):]
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PROTO_OG
#define SOCKET_CONNECTED 6
#define SOS_TMP_ALERT 2
#define LOGData(...) ((void)0)
struct { int SocketState; } ServerSocket[4];
struct { int DisableSOSTamper; } VTSData;
struct { int Enable; } VAlert[3];
int IsEMRTSend, built, sends, failed, added, destinations;
char dataBuffer[128];
void InitBuffer(uint8_t type) {
    assert(type==16); built++; strcpy(dataBuffer,",DT,16,L,");
}
uint8_t TCPSocket_SendString(void *socket, char *buf) {
    int i; for(i=0;i<4;i++) if(socket==&ServerSocket[i]) break;
    assert(i<4);
    if(failed || ServerSocket[i].SocketState!=SOCKET_CONNECTED) return 0;
    assert(!strcmp(buf,",DT,16,L,")); sends++; destinations|=1<<i; return 1;
}
void AddAlert(int i) { added++; }
'''
program += delivery + "\n" + dispatch + "\n" + physical
program += r'''
int main(void) {
    VTSData.DisableSOSTamper=1; IsEMRTSend=1;
    dispatch(2); assert(SOSTamperTestPending && !added);
    HandleSOSTamperTest(); assert(!built && SOSTamperTestPending);
    ServerSocket[0].SocketState=SOCKET_CONNECTED;
    failed=1; HandleSOSTamperTest(); assert(SOSTamperTestPending && !sends);
    failed=0; HandleSOSTamperTest(); assert(!SOSTamperTestPending && sends==1);
    assert(VTSData.DisableSOSTamper==1 && IsEMRTSend==1 && !VAlert[2].Enable);
    HandleSOSTamperTest(); assert(sends==1);
    dispatch(2); HandleSOSTamperTest(); assert(sends==2);
    VAlert[2].Enable=1; physical(); assert(!VAlert[2].Enable && sends==2);
    physical(); assert(!IsEMRTSend);
    VTSData.DisableSOSTamper=0; VAlert[2].Enable=1;
    physical(); assert(sends==3 && IsEMRTSend==1);
    physical(); assert(sends==3);
    ServerSocket[0].SocketState=0; ServerSocket[2].SocketState=SOCKET_CONNECTED;
    dispatch(2); HandleSOSTamperTest(); assert(sends==4 && (destinations & 4));
    dispatch(3); assert(added==1 && !SOSTamperTestPending);
#ifdef EXTENDED_IPS
    ServerSocket[2].SocketState=0; ServerSocket[3].SocketState=SOCKET_CONNECTED;
    dispatch(2); HandleSOSTamperTest(); assert(sends==5 && (destinations & 8));
#endif
    puts("OG DT-16 test-rig regression passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="og_tamper_") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test.exe"
    c.write_text(program)
    for flags in ([], ["-DEXTENDED_IPS"]):
        subprocess.run(["gcc", "-std=c99", "-Wall", *flags, str(c), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
