#include "PktSave.h"
#include "Server.h"


uint16_t HistoryPacketCount;
PacketConfigtypedef PacketConfig;



void UpdatePacketConfig(void)
{
    int fd=-1,ret;
    fd = nwy_sdk_fopen(PKTCOUNT_LOC,NWY_WB_PLUS_MODE);
    if(fd<0)
    {
        nwy_dbg_log("\r\nHistory Config FIle Create ERROR\n");
        return;
    }

    ret = nwy_sdk_fwrite(fd,(void*)&PacketConfig,sizeof(PacketConfigtypedef));
    nwy_dbg_log("\r\nHistory Config file write size :%d\n",ret);
    nwy_sdk_fclose(fd);
}

void CreateBlankPacketConfig(void)
{
    PacketConfig.DefData = PKTCOUNT_DEF;
    PacketConfig.LastPkt = 0;
    UpdatePacketConfig();
}

void CheckPacketCount(void)
{
    int fd=-1;
    int ret;
    if(nwy_sdk_fexist(PKTCOUNT_LOC))
    {
        fd = nwy_sdk_fopen(PKTCOUNT_LOC,NWY_RDONLY);
        if(fd<0)
        {
            nwy_dbg_log("\r\nHistory CONFIG file Cant Open! Creating New Blank...");
            CreateBlankPacketConfig();
            return;
        }
        
        ret = nwy_sdk_fread(fd,(void*)&PacketConfig,sizeof(PacketConfigtypedef));
        if(PacketConfig.DefData != PKTCOUNT_DEF)
        {
            nwy_dbg_log("\r\nHistory CONFIG file Def Mismatch! Creating New Blank...");
            CreateBlankPacketConfig();
            return;
        }
        nwy_dbg_log("\r\nHistory CONFIG Last Packet :%d\n",PacketConfig.LastPkt);
        nwy_sdk_fclose(fd);
        ret = nwy_sdk_vfs_free_size("/");
        nwy_dbg_log("\r\nHistory Remaining Space :%d\n",ret);
        MemoryPercent = ret / 7000;
        return;
    }
    nwy_dbg_log("\r\nNO History Config file! Creating New Blank...");
    CreateBlankPacketConfig();
    return;
}


uint8_t WriteNewPacket(uint16_t pktnum)
{
    int fd=-1,ret, len=0;
    char packetname[20];
    memset(packetname,0x00,20);
    sprintf(packetname,"%s%d%s",PKT_NAME_HEADER,pktnum,PKT_NAME_FOOTER);
    len = strlen(dataBuffer)+1;
    fd = nwy_sdk_fopen((const char*)packetname,NWY_WB_PLUS_MODE);
    if(fd<0)
    {
        nwy_dbg_log("\r\nHistory packet %s Create ERROR\n",packetname);
        return 0;
    }

    ret = nwy_sdk_fwrite(fd,(void*)&dataBuffer,len);
    nwy_dbg_log("\r\nHistory Packet %s file write size :%d\n",packetname,ret);
    nwy_sdk_fclose(fd);
    return 1;
}

uint8_t DeletePacket(uint16_t pktnum)
{
    int ret;
    char packetname[20];  
    memset(packetname,0x00,20);
    sprintf(packetname,"%s%d%s",PKT_NAME_HEADER,pktnum,PKT_NAME_FOOTER); 
    if(nwy_sdk_fexist((const char*)packetname))
    {
        ret = nwy_sdk_file_unlink((const char*)packetname);
        if(ret!= NWY_SUCESS)
        {
            nwy_dbg_log("\r\nHistory Packet %s Delete ERROR \n",packetname);
            return 0;
        }
        return 1;
    }
    nwy_dbg_log("\r\nHistory Packet %s does Not Exist \n",packetname);
    return 1;
}

uint8_t ReadPacket(uint16_t pktnum)
{
    int fd=-1,ret,len;
    long rd;
    char packetname[20];
    
    memset(packetname,0x00,20);
    sprintf(packetname,"%s%d%s",PKT_NAME_HEADER,pktnum,PKT_NAME_FOOTER); 
    nwy_dbg_log("\r\nReading packet %s",packetname);
    if(nwy_sdk_fexist((const char*)packetname))
    {
        fd = nwy_sdk_fopen((const char*)packetname,NWY_RDONLY);
        if(fd<0)
        {
            nwy_dbg_log("\r\nHistory packet file %s Cant Open",packetname);
            return 0;
        }
        ret = nwy_sdk_fsize_fd(fd);
        if((ret<0) || (ret > 350))
        {
            nwy_dbg_log("\r\nHistory packet file %s Size ERROR, ret: %d",packetname, ret);
            return 0;
        }
        len = ret;
        rd = nwy_sdk_fread(fd,(void*)&dataBuffer,len);
        if(rd < 0)
        {
            nwy_dbg_log("\r\nHistory packet file %s Read ERROR, ret: %d",packetname, ret);
            return 0;
        }
        nwy_sdk_fclose(fd);
        return 1;
    }
    nwy_dbg_log("\r\nHistory Packet %s does Not Exist \n",packetname);
    return 0;
}




void DeleteLastPacket(void)
{
    CheckPacketCount();
    if(PacketConfig.LastPkt <=0)
    {
        nwy_dbg_log("\r\nNO History Packets to Delete \n");
        return;
    }
    if(DeletePacket(PacketConfig.LastPkt))
    {
        PacketConfig.LastPkt--;
        UpdatePacketConfig();
        nwy_dbg_log("\r\nHistory Packets Decreased to %d \n",PacketConfig.LastPkt);
    }

}

void DeleteAllPackets(void)
{
    CheckPacketCount();
    for(int i = 0; i < MAX_PACKET_COUNT;i++)
    {
        if(PacketConfig.LastPkt <= 0)
        {
            nwy_dbg_log("All History Packets Cleared");
            return;
        }
        if(DeletePacket(PacketConfig.LastPkt))
        {
            PacketConfig.LastPkt--;
            UpdatePacketConfig();
            nwy_dbg_log("\r\nHistory Packets Decreased to %d \n",PacketConfig.LastPkt);
        }
    }
}

void ReadLastPacket(void)
{
    CheckPacketCount();
    if(PacketConfig.LastPkt <=0)
    {
        nwy_dbg_log("\r\nNO History Packets to Read \n");
        return;
    }
    nwy_dbg_log("\r\nReading Last Packet...");
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
        memset(ss,0x00,30);
        memset(ff,0x00,30);
        sprintf(ss,"%s%d%s",PKT_NAME_HEADER,i-1,PKT_NAME_FOOTER);
        sprintf(ff,"%s%d%s",PKT_NAME_HEADER,i,PKT_NAME_FOOTER);
        nwy_sdk_frename(ff,ss);
    }

    PacketConfig.LastPkt--;
    UpdatePacketConfig();
    return;
}

void SavePacket(void)
{
    int rem;
    #ifdef HISTORY_DISABLED
    nwy_dbg_log("History Disabled in Compile, returning");
    return;
    #endif

    CheckPacketCount();
    if(PacketConfig.LastPkt >= MAX_PACKET_COUNT)
        DeleteFirstPacket();
    

    if(WriteNewPacket(PacketConfig.LastPkt+1))
    {
        PacketConfig.LastPkt++;
        UpdatePacketConfig();
        nwy_dbg_log("\r\nHistory Packets Increased to %d \n",PacketConfig.LastPkt);
        rem = nwy_sdk_vfs_free_size("/");
        nwy_dbg_log("\r\nSpace Left :  %d \n",rem);
    }
    
}

uint16_t GetPacketCount(void)
{
    #ifdef HISTORY_DISABLED
    return 0;
    #endif
    CheckPacketCount();
    return PacketConfig.LastPkt;
}



