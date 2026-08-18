#include "PktSave.h"
#include "Server.h"
#include "GPRS.h"
#include "MOTA.h"
#include <time.h>
#ifndef HISTORY_DISABLED

uint16_t HistoryPacketCount = 0;
PacketConfigtypedef PacketConfig = {0};
FTKConfigtypedef FTKConfig = {
    .Altitude=0,
    .HDOP=0,
    .PDOP=0,
    .Heading=0,
    .IsEnabled=0,
    .Interval=0,
    .Lat=0,
    .Long=0,
    .Noofsats=0,
    .PacketCount=0,
    .Speed=0,
    .FTK_LastPacketTime = {0, 0, 0, 0, 0, 0, 0} // Initialize RTC to zero
};

int MemoryPercent = 0;


void UpdatePacketConfig(void)
{
    SaveToFlash(PKTCOUNT_LOC, (void *)&PacketConfig, sizeof(PacketConfigtypedef));
}

void CreateBlankPacketConfig(void)
{
    PacketConfig.DefData = PKTCOUNT_DEF;
    PacketConfig.LastPkt = 0;
    UpdatePacketConfig();
}

void CheckPacketCount(void)
{
    if (!LoadFromFlash(PKTCOUNT_LOC, (void *)&PacketConfig, sizeof(PacketConfigtypedef), CreateBlankPacketConfig))
        return;

    if (PacketConfig.DefData != PKTCOUNT_DEF)
    {
        LOGData(TAG_BACKUP, "History CONFIG file Def Mismatch! Creating New Blank...");
        CreateBlankPacketConfig();
        return;
    }
    
    LOGData(TAG_BACKUP, "History CONFIG Last Packet : %d", PacketConfig.LastPkt);
    
    int freeSpace = Ql_FS_GetFreeSpace(Ql_FS_UFS);
    LOGData(TAG_BACKUP, "History Remaining Space : %d", freeSpace);

    s64 totalSpace = Ql_FS_GetTotalSpace(Ql_FS_UFS);
    if (totalSpace > 0)
    {
        MemoryPercent = (int)((freeSpace * 100) / totalSpace);
    }
    else
    {
        MemoryPercent = 0;
    }
}

uint8_t WriteNewPacket(uint16_t pktnum)
{
    char packetname[20];
    Ql_memset(packetname, 0x00, sizeof(packetname));
    Ql_sprintf(packetname, "%s%d%s", PKT_NAME_HEADER, pktnum, PKT_NAME_FOOTER);

    // Ensure dataBuffer is strictly null-terminated within buffer bounds
    dataBuffer[sizeof(dataBuffer) - 1] = '\0';
    int len = Ql_strlen(dataBuffer) + 1;
    if (len > sizeof(dataBuffer)) len = sizeof(dataBuffer);
    LOGData(TAG_BACKUP, "Writing packet in History, Length: %d", len);
    return SaveToFlash(packetname, (void *)&dataBuffer, len);
}

uint8_t DeletePacket(uint16_t pktnum)
{
    char packetname[20];
    Ql_memset(packetname, 0x00, sizeof(packetname));
    Ql_sprintf(packetname, "%s%d%s", PKT_NAME_HEADER, pktnum, PKT_NAME_FOOTER);

    if (Ql_FS_Check(packetname) == QL_RET_OK)
    {
        int ret = Ql_FS_Delete(packetname);
        if (ret != QL_RET_OK)
        {
            LOGData(TAG_BACKUP, "History Packet %s Delete ERROR", packetname);
            return 0;
        }
        return 1;
    }
    
    LOGData(TAG_BACKUP, "History Packet %s does Not Exist", packetname);
    return 1;
}

uint8_t ReadPacket(uint16_t pktnum)
{
    char packetname[20];
    Ql_memset(packetname, 0x00, sizeof(packetname));
    Ql_sprintf(packetname, "%s%d%s", PKT_NAME_HEADER, pktnum, PKT_NAME_FOOTER);

    LOGData(TAG_BACKUP, "Reading packet %s", packetname);

    if (!LoadFromFlash(packetname, (void *)&dataBuffer, sizeof(dataBuffer), NULL))
        return 0;
    
    return 1;
}



void DeleteLastPacket(void)
{
    CheckPacketCount();
    if(PacketConfig.LastPkt <=0)
    {
        LOGData(TAG_BACKUP,"NO History Packets to Delete");
        return;
    }
    if(DeletePacket(PacketConfig.LastPkt))
    {
        PacketConfig.LastPkt--;
        UpdatePacketConfig();
        LOGData(TAG_BACKUP,"History Packets Decreased to %d",PacketConfig.LastPkt);
    }

}

void DeleteAllPackets(void)
{
    CheckPacketCount();
    for(int i = 0; i < MAX_PACKET_COUNT;i++)
    {
        if(PacketConfig.LastPkt <= 0)
        {
            LOGData(TAG_BACKUP,"All History Packets Cleared");
            return;
        }
        if(DeletePacket(PacketConfig.LastPkt))
        {
            PacketConfig.LastPkt--;
            UpdatePacketConfig();
            LOGData(TAG_BACKUP,"History Packets Decreased to %d",PacketConfig.LastPkt);
        }
    }
}

uint8_t ClearHistoryStorage(uint16_t *deletedCount, uint16_t *failedCount)
{
    uint16_t i;
    char packetname[20];
    uint8_t success = 1;

    if (deletedCount) *deletedCount = 0;
    if (failedCount) *failedCount = 0;

    /* This sweep is called from FTPStart() on the server thread and previously
     * logged nothing on the success path, so 256 UFS check/delete calls ran
     * completely silently — a stall in any one of them was indistinguishable
     * from a dead thread. Emit a progress breadcrumb so the stalling index is
     * visible in a field log without flooding it. */
    LOGData(TAG_BACKUP, "ClearHistoryStorage: sweeping 1..%d", MAX_PACKET_COUNT);

    // Do not rely on LastPkt: an interrupted write can leave orphaned packets.
    for (i = 1; i <= MAX_PACKET_COUNT; i++)
    {
        if ((i % 32) == 0)
            LOGData(TAG_BACKUP, "ClearHistoryStorage: at %d/%d", i, MAX_PACKET_COUNT);
        Ql_memset(packetname, 0x00, sizeof(packetname));
        Ql_sprintf(packetname, "%s%d%s", PKT_NAME_HEADER, i, PKT_NAME_FOOTER);
        if (Ql_FS_Check(packetname) == QL_RET_OK)
        {
            if (Ql_FS_Delete(packetname) == QL_RET_OK)
            {
                if (deletedCount) (*deletedCount)++;
            }
            else
            {
                success = 0;
                if (failedCount) (*failedCount)++;
                LOGData(TAG_BACKUP, "Unable to delete history packet %s", packetname);
            }
        }
    }

    Ql_memset(&PacketConfig, 0, sizeof(PacketConfig));
    PacketConfig.DefData = PKTCOUNT_DEF;
    PacketConfig.LastPkt = 0;
    // Removing the count file is safer than recreating it through SaveToFlash(),
    // whose legacy error path formats UFS. CheckPacketCount() creates a clean
    // count file on the next history write/read.
    if (Ql_FS_Check(PKTCOUNT_LOC) == QL_RET_OK &&
        Ql_FS_Delete(PKTCOUNT_LOC) != QL_RET_OK)
    {
        success = 0;
        if (failedCount) (*failedCount)++;
        LOGData(TAG_BACKUP, "Unable to delete history count file");
    }

    return success;
}

void ReadLastPacket(void)
{
    CheckPacketCount();
    if(PacketConfig.LastPkt <=0)
    {
        LOGData(TAG_BACKUP,"NO History Packets to Read");
        return;
    }
    LOGData(TAG_BACKUP,"Reading Last Packet...");
    while(!ReadPacket(PacketConfig.LastPkt))
    {
        PacketConfig.LastPkt--;
        if(PacketConfig.LastPkt==0)
            break;
    }
    UpdatePacketConfig();
    return;
}

void DeleteFirstPacket(void)
{
    uint16_t i;
    char ss[30];
    char ff[30];
    if(PacketConfig.LastPkt<=0)
        return;

    if(PacketConfig.LastPkt == 1)
    {
        DeletePacket(1);
        PacketConfig.LastPkt--;
        UpdatePacketConfig();
        return;

    }

    DeletePacket(1);

    for(i = 2;i<=PacketConfig.LastPkt;i++)
    {
        Ql_memset(ss,0x00,30);
        Ql_memset(ff,0x00,30);
        Ql_sprintf(ss,"%s%d%s",PKT_NAME_HEADER,i-1,PKT_NAME_FOOTER);
        Ql_sprintf(ff,"%s%d%s",PKT_NAME_HEADER,i,PKT_NAME_FOOTER);
        Ql_FS_Rename(ff, ss);
    }

    PacketConfig.LastPkt--;
    UpdatePacketConfig();
    return;
}

void SavePacket(void)
{
    // Skip saving history packets during FOTA/MOTA updates
    if (IsMotaProcessing || IsFotaProcessing)
    {
        LOGData(TAG_BACKUP, "FOTA/MOTA update in progress, skipping history save");
        return;
    }

    // Don't save history packets if datetime is not available
    if (!GSM.IsTimeSet)
    {
        LOGData(TAG_BACKUP, "DateTime not available, skipping history save");
        return;
    }
    
    // Additional validation - check if date/time values are reasonable
    if (CurrentDateTime.Year < 20 || CurrentDateTime.Month == 0 || CurrentDateTime.Month > 12 || 
        CurrentDateTime.Date == 0 || CurrentDateTime.Date > 31)
    {
        LOGData(TAG_BACKUP, "Invalid DateTime values, skipping history save (Y:%d M:%d D:%d)", 
                CurrentDateTime.Year, CurrentDateTime.Month, CurrentDateTime.Date);
        return;
    }
    
    if(PacketConfig.LastPkt >= MAX_PACKET_COUNT)
    {
        #ifdef ROLLOVER
        DeleteFirstPacket();
        #else
        LOGData(TAG_BACKUP,"Max History Packets Reached, Can't Save More");
        return;
        #endif
    }
    

    if(WriteNewPacket(PacketConfig.LastPkt+1))
    {
        PacketConfig.LastPkt++;
        UpdatePacketConfig();
        LOGData(TAG_BACKUP,"History Packets Increased to %d",PacketConfig.LastPkt);
    }
    
}

uint16_t GetPacketCount(void)
{
    CheckPacketCount();
    return PacketConfig.LastPkt;
}

_RTC FTK_DeductTime(_RTC currentTime, uint32_t secondsToDeduct)
{
    LOGData(TAG_BACKUP, "DeductTime called with secondsToDeduct: %u", secondsToDeduct);

    struct tm timeStruct = {0};
    int fullYear = (currentTime.Year < 70) ? (2000 + currentTime.Year) : (1900 + currentTime.Year);
    timeStruct.tm_year = fullYear - 1900;
    timeStruct.tm_mon  = currentTime.Month - 1;   // tm_mon is 0-based
    timeStruct.tm_mday = currentTime.Date;
    timeStruct.tm_hour = currentTime.Hour;
    timeStruct.tm_min  = currentTime.Min;
    timeStruct.tm_sec  = currentTime.Sec;

    LOGData(TAG_BACKUP, "Current time to struct tm: %04d-%02d-%02d %02d:%02d:%02d",
            timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday,
            timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);

    // mktime treats struct tm as local time, but on embedded systems this often behaves like UTC
    time_t unixTime = mktime(&timeStruct);
    if (unixTime == (time_t)-1)
    {
        LOGData(TAG_BACKUP, "Error: mktime failed, returning zeroed time");
        currentTime.Year = currentTime.Month = currentTime.Date = 0;
        currentTime.Hour = currentTime.Min = currentTime.Sec = 0;
        return currentTime;
    }

    LOGData(TAG_BACKUP, "Unix time before deduction: %ld", unixTime);

    // Deduct seconds
    unixTime -= secondsToDeduct;

    LOGData(TAG_BACKUP, "Unix time after deduction: %ld", unixTime);

    // Convert back to UTC
    struct tm *newTimeStruct = gmtime(&unixTime);
    if (!newTimeStruct)
    {
        LOGData(TAG_BACKUP, "Error: gmtime failed, returning zeroed time");
        currentTime.Year = currentTime.Month = currentTime.Date = 0;
        currentTime.Hour = currentTime.Min = currentTime.Sec = 0;
        return currentTime;
    }

    currentTime.Year  = (newTimeStruct->tm_year + 1900) % 100; // Year is stored as last two digits
    currentTime.Month = newTimeStruct->tm_mon + 1;
    currentTime.Date  = newTimeStruct->tm_mday;
    currentTime.Hour  = newTimeStruct->tm_hour;
    currentTime.Min   = newTimeStruct->tm_min;
    currentTime.Sec   = newTimeStruct->tm_sec;

    LOGData(TAG_BACKUP, "New UTC time after deduction: %04d-%02d-%02d %02d:%02d:%02d",
            currentTime.Year, currentTime.Month, currentTime.Date,
            currentTime.Hour, currentTime.Min, currentTime.Sec);

    return currentTime;
}


static uint32_t ftk_rand_seed = 0;

void FTK_CustomSeed(uint32_t baseEntropy)
{
    // You can call this at boot with baseEntropy = time + checksum or similar
    ftk_rand_seed = baseEntropy ^ 0xA5A5A5A5;  // Mix with fixed pattern
}

uint32_t FTK_CustomRand(void)
{
    // Simple Linear Congruential Generator
    ftk_rand_seed = (ftk_rand_seed * 1664525UL + 1013904223UL);  // Common LCG
    return ftk_rand_seed;
}


static double generate_jitter(double max_range)
{
    uint32_t rand_val = FTK_CustomRand() % 10000;  // [0, 9999]
    double scaled = ((rand_val / 5000.0) - 1.0) * max_range;  // [-1, +1] * max
    return scaled;
}

void FTK_ApplyVariation(FTKConfigtypedef *config)
{
    if (!config) return;

    // Base indoor fixed position
    double latBase = 18.523895;
    double longBase = 73.815845;
    double altBase = 627.00;

    // Jitter ranges (you can tweak these for realism)
    double latJitterMax = 0.000006;    // ~0.6m
    double longJitterMax = 0.000006;
    double altJitterMax = 2.5;         // ¡À1.5m
    double headingJitterMax = 2.0;     // ¡À1¡ã
    double hdopJitterMax = 0.1;
    double pdopJitterMax = 0.1;

    // Position with jitter
    config->Lat = latBase + generate_jitter(latJitterMax);
    config->Long = longBase + generate_jitter(longJitterMax);
    config->Altitude = altBase + generate_jitter(altJitterMax);

    // Speed mostly 0, with rare small blips
    config->Speed = ((Ql_rand() % 100) < 5) ? (Ql_rand() % 20) / 100.0 : 0.0;

    // Heading small random walk
    config->Heading += generate_jitter(headingJitterMax);
    if (config->Heading < 0) config->Heading += 360.0;
    if (config->Heading >= 360.0) config->Heading -= 360.0;

    // DOP values
    config->HDOP = 2.0 + generate_jitter(hdopJitterMax);
    config->PDOP = 2.0 + generate_jitter(pdopJitterMax);
    if (config->HDOP < 0.5) config->HDOP = 0.5;
    if (config->PDOP < 0.5) config->PDOP = 0.5;

    // Satellite count variation
    int delta = (Ql_rand() % 3) - 1; // -1, 0, or +1
    config->Noofsats += delta;
    if (config->Noofsats < 4) config->Noofsats = 4;
    if (config->Noofsats > 8) config->Noofsats = 8;
    LOGData(TAG_BACKUP, "IndoorGPS: Sats=%d", config->Noofsats);
}




#define FTK_LOG_LAT 18.523919 
#define FTK_LOG_LONG    73.815871
#define FTK_LOG_ALTITUDE  635
#define FTK_LOG_SPEED 0.0
#define FTK_LOG_HDOP 1.3
#define FTK_LOG_PDOP 2.1
#define FTK_LOG_HEADING 0.0
#define FTK_LOG_SATS 8

uint8_t EnableFTKLogs(uint16_t interval)
{
    if(!GPS.GPSFix)
    {
        LOGData(TAG_BACKUP,"GPS is not fixed for Sampling..., using Default Values");
    }
    if(ServerSocket[0].SocketState != SOCKET_CONNECTED)
    {
        LOGData(TAG_BACKUP,"Cant Enable FTK as Server is not Sending Data...");
        return 0;
    }
    uint32_t baseEntropy = Ql_GetMsSincePwrOn();  // or use RTC time converted to seconds
    baseEntropy ^= lastcrc;              // XOR with checksum or IMEI bits
    FTK_CustomSeed(baseEntropy);
    memset(&FTKConfig, 0x00, sizeof(FTKConfigtypedef));
    FTKConfig.IsEnabled = 1;
    FTKConfig.Interval = interval;
    FTKConfig.PacketCount = 0;
    memcpy(&FTKConfig.FTK_LastPacketTime, &CurrentDateTime, sizeof(_RTC));
    if(!GPS.GPSFix)
    {
        FTKConfig.Lat = FTK_LOG_LAT;
        FTKConfig.Long = FTK_LOG_LONG;
        FTKConfig.Speed = FTK_LOG_SPEED;
        FTKConfig.Altitude = FTK_LOG_ALTITUDE;
        FTKConfig.HDOP = FTK_LOG_HDOP;
        FTKConfig.PDOP = FTK_LOG_PDOP;
        FTKConfig.Heading = FTK_LOG_HEADING;
        FTKConfig.Noofsats = FTK_LOG_SATS;

        LOGData(TAG_BACKUP,"FTK Enabling with Default Values: Sats=%d", FTKConfig.Noofsats);
    }
    else
    {
        FTKConfig.Lat = GPS.Latitude;
        FTKConfig.Long = GPS.Longitude;
        FTKConfig.Speed = GPS.Speed;
        FTKConfig.Altitude = GPS.Altitude;
        FTKConfig.HDOP = GPS.HDOP;
        FTKConfig.PDOP = GPS.PDOP;
        FTKConfig.Heading = GPS.Heading;
        FTKConfig.Noofsats = GPS.NoOfSatalite;
        LOGData(TAG_BACKUP,"FTK Enabled with GPS Values: Sats=%d", FTKConfig.Noofsats);
    }
    
    FTKConfig.FTK_LastPacketTime = FTK_DeductTime(FTKConfig.FTK_LastPacketTime, 60 * 60); // Deduct 1 hour for initial time
    LOGData(TAG_BACKUP,"FTK Latest Packet Time set to: %04d-%02d-%02d %02d:%02d:%02d",
            FTKConfig.FTK_LastPacketTime.Year, FTKConfig.FTK_LastPacketTime.Month, FTKConfig.FTK_LastPacketTime.Date,
            FTKConfig.FTK_LastPacketTime.Hour, FTKConfig.FTK_LastPacketTime.Min, FTKConfig.FTK_LastPacketTime.Sec);

    FTK_ApplyVariation(&FTKConfig); // Apply jitter to initial values


    return 1;
    
}





#endif

