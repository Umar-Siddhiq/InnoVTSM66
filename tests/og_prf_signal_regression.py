"""Compile the production reporting helper and test manual PRF isolation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sms = (root / "custom/SMS.c").read_text()
start = sms.index("uint8_t GetReportedSignalStrength(void)\n{")
end = sms.index("\nuint8_t DecodeSMS", start)
helper = sms[start:end]
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct { uint8_t SignalStrength; } GSM;
struct { uint8_t IsCustomSPN; char mSPN[20]; } VTSData;
'''
program += helper
program += r'''
int main(void) {
    unsigned raw, manual, n;
    const char *names[] = {"", "BSNL", "bsnl", "BsNl", "VI", "AIRTEL", "BSNL2"};
    for(manual=0;manual<2;manual++) {
        VTSData.IsCustomSPN=manual;
        for(n=0;n<sizeof(names)/sizeof(names[0]);n++) {
            strcpy(VTSData.mSPN,names[n]);
            for(raw=0;raw<256;raw++) {
                unsigned expected=raw;
                GSM.SignalStrength=raw;
#ifdef PROTO_OG
                if(manual && n>=1 && n<=3)
                    expected=raw<10 ? 10 : raw>13 ? 13 : raw;
#endif
                assert(GetReportedSignalStrength()==expected);
                assert(GSM.SignalStrength==raw);
            }
        }
    }
    /* Clearing PRF must take effect even if the stored name remains BSNL. */
    strcpy(VTSData.mSPN,"BSNL"); GSM.SignalStrength=25;
    VTSData.IsCustomSPN=0; assert(GetReportedSignalStrength()==25);
    puts("PRF signal reporting regression passed");
}
'''
with tempfile.TemporaryDirectory(prefix="prf_signal_") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test.exe"
    c.write_text(program)
    for flags in (["-DPROTO_OG"], []):
        subprocess.run(["gcc", "-std=c99", "-Wall", *flags, str(c), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
