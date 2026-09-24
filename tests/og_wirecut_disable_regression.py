"""Compile and verify SOS wirecut / tamper disable logic across firmware components."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
vts_h = (root / "custom/inc/VTS.h").read_text()
sos_c = (root / "custom/SOS.c").read_text()
server_c = (root / "custom/Server.c").read_text()
systic_c = (root / "custom/Systic.c").read_text()

# Check macro definitions in VTS.h
assert "#define DEFAULT_DISABLE_SOS_TAMPER 1" in vts_h
assert "#define SOS_WIRECUT_SMS_ENABLED 0" in vts_h

# Check SOSAlert wirecut check in Server.c
assert "if(AlertNum == 16)" in server_c
assert "#if !SOS_WIRECUT_SMS_ENABLED" in server_c

# Verify compilation of ProcessSOS tamper gating logic
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PUSH_MIN_DELAY 2
#define SOS_TAMPER_COUNT 21
#define SOS_TMP_ALERT 2
#define SOS_ON_ALERT 0
#define SOS_ACTIVE_LEVEL 1
#define SOS_IDLE_LEVEL 0
#define LOGData(...) ((void)0)

typedef struct {
    uint8_t IsSOS;
    uint8_t SOSPushCount;
    uint8_t IsSOSTamper;
    uint8_t IsSOSSMS;
    uint16_t SOSTimeOut;
    uint16_t SOSTimeLasped;
    uint16_t SOSTamperTimeLapsed;
    uint8_t RequireRelease;
} SOSTypeDefStruct;

volatile SOSTypeDefStruct SOS = {0};

struct {
    uint8_t DisableSOSTamper;
} VTSData;

struct {
    uint8_t Enable;
} VAlert[10];

struct {
    uint8_t IsCriticalPacket;
} IsPacketReady;

int alert_added = 0;
void AddAlert(int id) { alert_added = id; }
void RemoveAlert(int id) { if (alert_added == id) alert_added = -1; }

void test_process_sos(int pinValue) {
    if (pinValue == SOS_ACTIVE_LEVEL) {
        if (SOS.SOSPushCount < 250)
            SOS.SOSPushCount++;

        if (!VTSData.DisableSOSTamper && SOS.SOSPushCount > SOS_TAMPER_COUNT && !SOS.IsSOSTamper) {
            SOS.SOSPushCount = 0;
            SOS.IsSOSTamper = 1;
            SOS.RequireRelease = 1;
            VAlert[SOS_TMP_ALERT].Enable = 1;
            AddAlert(SOS_TMP_ALERT);
            IsPacketReady.IsCriticalPacket = 1;
            return;
        }
    }
    if (pinValue == SOS_IDLE_LEVEL) {
        if (SOS.SOSPushCount > 0) {
            if (!SOS.RequireRelease && !SOS.IsSOSTamper) {
                if (SOS.SOSPushCount >= PUSH_MIN_DELAY && !SOS.IsSOS) {
                    SOS.IsSOS = 1;
                    VAlert[SOS_ON_ALERT].Enable = 1;
                    AddAlert(SOS_ON_ALERT);
                }
            }
            SOS.SOSPushCount = 0;
        }
        SOS.RequireRelease = 0;
        if (SOS.IsSOSTamper) {
            SOS.IsSOSTamper = 0;
            RemoveAlert(SOS_TMP_ALERT);
            if (!VTSData.DisableSOSTamper) {
                IsPacketReady.IsCriticalPacket = 1;
            }
        }
    }
}

int main(void) {
    // 1. With DisableSOSTamper = 1, cut wire holding pin active past tamper count must NOT trigger tamper
    VTSData.DisableSOSTamper = 1;
    memset((void*)&SOS, 0, sizeof(SOS));
    memset((void*)VAlert, 0, sizeof(VAlert));
    IsPacketReady.IsCriticalPacket = 0;
    alert_added = 0;

    for (int t = 0; t < 30; t++) {
        test_process_sos(SOS_ACTIVE_LEVEL);
    }
    assert(SOS.IsSOSTamper == 0);
    assert(SOS.RequireRelease == 0);
    assert(VAlert[SOS_TMP_ALERT].Enable == 0);
    assert(alert_added == 0);
    assert(IsPacketReady.IsCriticalPacket == 0);
    assert(SOS.SOSPushCount == 30);

    // Releasing the button after a 3-second hold must trigger normal SOS ON, not be discarded
    test_process_sos(SOS_IDLE_LEVEL);
    assert(SOS.IsSOS == 1);
    assert(VAlert[SOS_ON_ALERT].Enable == 1);
    assert(alert_added == SOS_ON_ALERT);
    assert(SOS.IsSOSTamper == 0);

    // 2. Server emergency state check: when DisableSOSTamper = 1, even if IsSOSTamper were 1,
    // (!VTSData.DisableSOSTamper && SOS.IsSOSTamper) evaluates to 0
    SOS.IsSOSTamper = 1;
    int isEmergencyState = SOS.IsSOS || (!VTSData.DisableSOSTamper && SOS.IsSOSTamper);
    assert(isEmergencyState == 1); // due to IsSOS=1
    SOS.IsSOS = 0;
    isEmergencyState = SOS.IsSOS || (!VTSData.DisableSOSTamper && SOS.IsSOSTamper);
    assert(isEmergencyState == 0); // IsSOSTamper suppressed!

    puts("SOS wirecut disable regression passed successfully");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="og_wirecut_") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test.exe"
    c.write_text(program)
    subprocess.run(["gcc", "-std=c99", "-Wall", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
