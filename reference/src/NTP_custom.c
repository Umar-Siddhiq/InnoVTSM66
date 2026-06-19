#include "NTP_custom.h"

char ntp_rcv_buff[NTP_RCV_SIZE];




// Check if a year is a leap year
int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// Days in each month for regular and leap years
const uint8_t days_in_month[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, // Regular year
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}  // Leap year
};

// Custom function to convert time_t to _RTC
void time_t_to_rtc_custom(time_t timestamp, _RTC *rtc) {
    if (!rtc) return;

    // Initialize constants
    const uint32_t seconds_per_day = 86400;
    const uint32_t seconds_per_hour = 3600;
    const uint32_t seconds_per_minute = 60;

    // Epoch start (1970-01-01)
    uint16_t year = 1970;
    uint32_t days = timestamp / seconds_per_day;
    uint32_t seconds = timestamp % seconds_per_day;

    // Calculate current time (Hour, Minute, Second)
    rtc->Hour = seconds / seconds_per_hour;
    seconds %= seconds_per_hour;
    rtc->Min = seconds / seconds_per_minute;
    rtc->Sec = seconds % seconds_per_minute;

    // Calculate current day of the week (Epoch day 1970-01-01 is a Thursday)
    rtc->DaysOfWeek = (days + 4) % 7; // Adding 4 because 1970-01-01 was a Thursday

    // Calculate current year
    while (1) {
        uint16_t days_in_year = is_leap_year(year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }
    rtc->Year = year;

    // Calculate current month and date
    uint8_t leap = is_leap_year(year);
    for (rtc->Month = 0; rtc->Month < 12; rtc->Month++) {
        if (days < days_in_month[leap][rtc->Month]) break;
        days -= days_in_month[leap][rtc->Month];
    }
    rtc->Month += 1; // Months are 1-based (1 = January, 12 = December)
    rtc->Date = days + 1; // Days are 1-based
}

// Create NTP request packet
void create_ntp_packet(uint8_t *packet) {
    memset(packet, 0, NTP_PACKET_SIZE);
    packet[0] = 0x1B; // LI = 0, VN = 3 (Version 4), Mode = 3 (Client)
}

// Parse NTP response and get UNIX timestamp
time_t parse_ntp_response(uint8_t *response) {
    // Extract the transmit timestamp (bytes 40-43 for seconds)
    uint32_t seconds = (response[40] << 24) | (response[41] << 16) | 
                       (response[42] << 8)  | (response[43]);
    // Convert NTP timestamp to UNIX timestamp
    return seconds - NTP_EPOCH_OFFSET;
}



void init_ntp_socket(UDPSocketTypedef *socket)
{
    memset(socket,0,sizeof(UDPSocketTypedef));
    if(VTSData.ServerData.IPConfig[0])
        socket->isEnabled=1;
    socket->SocketNo = 1;
    socket->SocketIndex = 0;
    socket->SocketState = SOCKET_CLOSED;
    strcpy(socket->DNSorIP,VTSData.ServerData.IP1);
    socket->Port = atoi(VTSData.ServerData.Port1);
    socket->OnConnect = NULL;
    socket->Connected = NULL;
    socket->OnDisconnect = NULL;
    socket->rxSizeMAX = NTP_RCV_SIZE;
    socket->rxBuffer = ntp_rcv_buff;
}

// Custom NTP function
uint8_t get_ntp_time(_RTC *rtc) {
    if(GSM.GSMState != GPRS_ACTIVE)
    {
        nwy_dbg_log("ntp not possible due to gprs not active!!!");
        return 0;
    }
    uint8_t packet[NTP_PACKET_SIZE];
    uint8_t response[NTP_PACKET_SIZE];
    UDPSocketTypedef socket;

    init_ntp_socket(&socket);    

    // Step 1: Create the NTP request packet
    create_ntp_packet(packet);

    if(!UDPSocket_OPEN(&socket))
    {
        nwy_dbg_log("ntp Socket open fail!");
        return 0;
    }

    if(!UDPSocket_SendData(&socket,packet,NTP_PACKET_SIZE))
    {
        nwy_dbg_log("ntp data send fail!");
        return 0;
    }

    uint8_t tmout=100;
    while(!socket.isRXData)
    {
        nwy_udp_check_func(&socket);
        if(--tmout==0)
            break;
        nwy_sleep(10);
    }
    if(!socket.isRXData)
    {
        nwy_dbg_log("ntp data rcv timeout!");
        goto RET;
    }
    UDPSocket_Disconnect(&socket);
    time_t tm = parse_ntp_response(response);
    nwy_dbg_log("ntp tm val %i",tm);
    time_t_to_rtc_custom(tm,rtc);
    return 1;
    RET:
    UDPSocket_Disconnect(&socket);
    return 0;

}