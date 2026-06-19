#include "EPO.h"
#include "GPS.h"
#include "ql_time.h"

#ifdef EOP_USE

#ifdef EPO_VERBOSE_LOG
#define EPO_LOGV(...) LOGData(TAG_EPO, __VA_ARGS__)
#else
#define EPO_LOGV(...) do { } while (0)
#endif

#define EPO_LOGE(...) LOGData(TAG_EPO, __VA_ARGS__)

#define EPO_ACK_RETRIES      3
#define EPO_ACK_TIMEOUT_MS   2000

static uint8_t calculate_nmea_checksum_local(const char* sentence)
{
    uint8_t checksum = 0;
    // sentence is expected to start with '$'
    if (!sentence || sentence[0] != '$') {
        return 0;
    }
    sentence++; // skip '$'
    while (*sentence && *sentence != '*') {
        checksum ^= (uint8_t)(*sentence++);
    }
    return checksum;
}

static bool send_pair_command_wait(const char* cmdBody, const char* expectSubstr, char* response, uint16_t resp_size, uint32_t timeout_ms)
{
    if (!cmdBody || !expectSubstr) {
        return false;
    }

    // Prepare NMEA sentence with checksum
    char sentence[200];
    char tmp[180];
    Ql_memset(sentence, 0, sizeof(sentence));
    Ql_memset(tmp, 0, sizeof(tmp));

    Ql_sprintf(tmp, "$%s", cmdBody);
    uint8_t cs = calculate_nmea_checksum_local(tmp);
    Ql_sprintf(sentence, "%s*%02X\r\n", tmp, cs);

    gps_cmd.type = GPS_CMD_READ_CONFIG;
    gps_cmd.cmd_prefix = expectSubstr;
    gps_cmd.response_pending = true;
    gps_cmd.response_received = false;
    gps_cmd.response_ok = false;
    Ql_memset(gps_cmd.response, 0, sizeof(gps_cmd.response));

    GPS_UartSendString(sentence);
    EPO_LOGV("PAIR send: %s", sentence);

    uint32_t start_time = Ql_GetMsSincePwrOn();
    while (Ql_GetMsSincePwrOn() - start_time < timeout_ms) {
        if (gps_cmd.response_received) {
            if (response && resp_size > 0) {
                Ql_strncpy(response, gps_cmd.response, resp_size - 1);
                response[resp_size - 1] = '\0';
            }
            gps_cmd.type = GPS_CMD_NONE;
            gps_cmd.response_pending = false;
            return gps_cmd.response_ok;
        }
        ThreadSleep(20);
    }

    gps_cmd.type = GPS_CMD_NONE;
    gps_cmd.response_pending = false;
    return false;
}

static bool parse_pair470_setcount(const char* resp, int* outSetCount)
{
    if (!resp || !outSetCount) {
        return false;
    }

    const char* p = Ql_strstr(resp, "PAIR470,");
    if (!p) {
        return false;
    }
    p += 8; // after "PAIR470,"

    // Field 0: System_ID
    int sysId = 0;
    while (*p && *p != ',' && *p != '*') {
        if (*p >= '0' && *p <= '9') {
            sysId = sysId * 10 + (*p - '0');
        }
        p++;
    }
    if (*p != ',') {
        return false;
    }
    p++;

    // Field 1: <Set>
    int setCount = 0;
    bool any = false;
    while (*p && *p != ',' && *p != '*') {
        if (*p >= '0' && *p <= '9') {
            any = true;
            setCount = setCount * 10 + (*p - '0');
        }
        p++;
    }
    if (!any) {
        return false;
    }
    (void)sysId;
    *outSetCount = setCount;
    return true;
}

typedef struct {
    int sysId;
    int set;
    int fwn;
    int ftow;
    int lwn;
    int ltow;
    int fcwn;
    int fctow;
    int lcwn;
    int lctow;
} epo_pair470_status_t;

static bool parse_pair470_status(const char* resp, epo_pair470_status_t* out)
{
    if (!resp || !out) {
        return false;
    }

    Ql_memset(out, 0, sizeof(*out));

    // Example from guide:
    // $PAIR470,0,1,2098,194400,2098,216000,2098,194400,2098,216000*38
    int n = Ql_sscanf(resp, "$PAIR470,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                      &out->sysId, &out->set, &out->fwn, &out->ftow, &out->lwn, &out->ltow,
                      &out->fcwn, &out->fctow, &out->lcwn, &out->lctow);
    return (n == 10);
}

static int epo_div_floor(int a, int b)
{
    // floor(a / b) for positive b
    int q = a / b;
    int r = a % b;
    if (r != 0 && a < 0) {
        q -= 1;
    }
    return q;
}

static int epo_days_in_month(int year, int month)
{
    // month: 1..12
    static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month < 1 || month > 12) {
        return 30;
    }
    if (month != 2) {
        return days[month - 1];
    }

    // Leap year rule (Gregorian)
    bool leap = ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
    return leap ? 29 : 28;
}

static void epo_adjust_datetime_seconds(ST_Time* t, int deltaSeconds)
{
    if (!t || deltaSeconds == 0) {
        return;
    }

    int sec = (int)t->second + deltaSeconds;
    int q = epo_div_floor(sec, 60);
    t->second = sec - q * 60;

    int minute = (int)t->minute + q;
    q = epo_div_floor(minute, 60);
    t->minute = minute - q * 60;

    int hour = (int)t->hour + q;
    q = epo_div_floor(hour, 24);
    t->hour = hour - q * 24;

    int dayDelta = q;
    while (dayDelta > 0) {
        int dim = epo_days_in_month((int)t->year, (int)t->month);
        if ((int)t->day < dim) {
            t->day++;
        } else {
            t->day = 1;
            if ((int)t->month < 12) {
                t->month++;
            } else {
                t->month = 1;
                t->year++;
            }
        }
        dayDelta--;
    }
    while (dayDelta < 0) {
        if ((int)t->day > 1) {
            t->day--;
        } else {
            if ((int)t->month > 1) {
                t->month--;
            } else {
                t->month = 12;
                t->year--;
            }
            t->day = (s32)epo_days_in_month((int)t->year, (int)t->month);
        }
        dayDelta++;
    }
}

static bool epo_send_ref_time_utc_now(void)
{
    ST_Time local = {0};
    if (!Ql_GetLocalTime(&local)) {
        return false;
    }

    // Avoid sending bogus time if network time hasn't been synchronized yet.
    if (local.year <= 2022) {
        return false;
    }

    // Convert local -> UTC using timezone in 15-minute units.
    // Positive timezone means local time ahead of UTC.
    ST_Time utc = local;
    int tzSeconds = (int)local.timezone * 15 * 60;
    epo_adjust_datetime_seconds(&utc, -tzSeconds);
    utc.timezone = 0;

    // $PAIR590,<YYYY>,<MM>,<DD>,<hh>,<mm>,<ss>
    char cmd[120];
    Ql_memset(cmd, 0, sizeof(cmd));
    Ql_sprintf(cmd, "PAIR590,%04d,%02d,%02d,%02d,%02d,%02d",
               (int)utc.year, (int)utc.month, (int)utc.day,
               (int)utc.hour, (int)utc.minute, (int)utc.second);

    EPO_LOGV("Send Ref UTC: %s (tz=%d)", cmd, (int)local.timezone);
    return send_pair_command_wait(cmd, "$PAIR001,590,0", NULL, 0, 1500);
}

static bool epo_send_ref_position_best_effort(void)
{
    // Use last known coordinates if available.
    // The guide recommends within 30km; if uncertain, skip rather than send junk.
    if (GPS.Latitude == 0.0 || GPS.Longitude == 0.0) {
        return false;
    }
    if (GPS.Latitude < -90.0 || GPS.Latitude > 90.0 || GPS.Longitude < -180.0 || GPS.Longitude > 180.0) {
        return false;
    }

    // $PAIR600,<Lat>,<Lon>,<Height>,<AccMaj>,<AccMin>,<Bear>,<AccVert>
    // Conservative accuracy values (meters).
    double height = GPS.Altitude;
    double accMaj = 30000.0;
    double accMin = 30000.0;
    int bear = 0;
    double accVert = 100.0;

    char cmd[160];
    Ql_memset(cmd, 0, sizeof(cmd));
    Ql_sprintf(cmd, "PAIR600,%.6f,%.6f,%.2f,%.0f,%.0f,%d,%.0f",
               GPS.Latitude, GPS.Longitude, height, accMaj, accMin, bear, accVert);

    EPO_LOGV("Send Ref Pos: %s", cmd);
    return send_pair_command_wait(cmd, "$PAIR001,600,0", NULL, 0, 1500);
}

bool EPO_SendReferenceTimeNow(void)
{
    return epo_send_ref_time_utc_now();
}

bool EPO_SendReferencePositionNow(void)
{
    return epo_send_ref_position_best_effort();
}

static bool epo_query_flash_status_gps(int* outSetCount)
{
    char resp[160];
    Ql_memset(resp, 0, sizeof(resp));

    // Expect the data response, not the $PAIR001 ack
    if (!send_pair_command_wait("PAIR470,0", "$PAIR470,0,", resp, sizeof(resp), 1500)) {
        return false;
    }

    EPO_LOGV("PAIR470 resp: %s", resp);

    return parse_pair470_setcount(resp, outSetCount);
}

static bool epo_query_flash_status_gps_full(epo_pair470_status_t* out)
{
    char resp[160];
    Ql_memset(resp, 0, sizeof(resp));

    if (!send_pair_command_wait("PAIR470,0", "$PAIR470,0,", resp, sizeof(resp), 1500)) {
        return false;
    }
    EPO_LOGV("PAIR470 resp: %s", resp);
    return parse_pair470_status(resp, out);
}

static bool epo_erase_flash(void)
{
    // Guide: $PAIR472*CS, ack: $PAIR001,472,0
    bool ok = send_pair_command_wait("PAIR472", "$PAIR001,472,0", NULL, 0, 1500);
    EPO_LOGV("PAIR472 erase %s", ok ? "OK" : "FAIL");
    return ok;
}

static int16_t encode_binary_packet(uint8_t* buffer, uint16_t max_size, const binary_payload_t* payload)
{
    uint16_t required_length;
    uint8_t* pbyte;

    required_length = payload->data_size + BINARY_CONTROL_SIZE + BINARY_HEADER_SIZE;
    if(max_size < required_length) {
        return -1;
    }

    // Preamble
    buffer[0] = BINARY_PREAMBLE1;
    buffer[1] = BINARY_PREAMBLE2;

    // Header (little-endian) + data
    pbyte = &buffer[2];
    *pbyte++ = (uint8_t)(payload->message_id & 0xFF);
    *pbyte++ = (uint8_t)((payload->message_id >> 8) & 0xFF);
    *pbyte++ = (uint8_t)(payload->data_size & 0xFF);
    *pbyte++ = (uint8_t)((payload->data_size >> 8) & 0xFF);
    Ql_memcpy(pbyte, payload->data, payload->data_size);
    pbyte += payload->data_size;

    // Checksum: XOR over msgid+len+payload
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < BINARY_HEADER_SIZE; i++) {
        checksum ^= buffer[2 + i];
    }
    for (uint16_t i = 0; i < payload->data_size; i++) {
        checksum ^= payload->data[i];
    }

    *pbyte++ = checksum;
    *pbyte++ = BINARY_ENDWORD1;
    *pbyte = BINARY_ENDWORD2;

    return required_length;
}

static bool epo_send_with_ack(uint16_t msgId, const uint8_t* frame, uint16_t frameLen)
{
    for (int attempt = 0; attempt < EPO_ACK_RETRIES; attempt++) {
        GPS_BinaryAckReset();
        int ret = GPS_UartSendRaw(frame, frameLen);
        if (ret < 0) {
            return false;
        }
        if (GPS_WaitBinaryAck(msgId, EPO_ACK_TIMEOUT_MS)) {
            EPO_LOGV("ACK ok msgId=%u attempt=%d", msgId, attempt + 1);
            return true;
        }
        EPO_LOGV("ACK timeout msgId=%u attempt=%d/%d", msgId, attempt + 1, EPO_ACK_RETRIES);
        ThreadSleep(50);
    }
    return false;
}

static uint8_t calculate_checksum(const binary_payload_t* payload)
{
    uint8_t checksum = 0;
    uint8_t* pheader = (uint8_t*)payload;
    uint16_t i;

    // Calculate checksum for header
    for(i = 0; i < BINARY_HEADER_SIZE; i++) {
        checksum ^= *pheader++;
    }

    // Calculate checksum for data
    for(i = 0; i < payload->data_size; i++) {
        checksum ^= payload->data[i];
    }

    return checksum;
}

bool EPO_FlashInjectFile(const char* filename, char* status, uint16_t statusLen)
{
    if (status && statusLen) {
        Ql_memset(status, 0, statusLen);
    }
    if (!filename) {
        if (status && statusLen) Ql_strncpy(status, "Invalid file", statusLen - 1);
        return false;
    }

    u32 fileSize = Ql_FS_GetSize((char*)filename);
    EPO_LOGV("EPO file: %s size=%lu", filename, fileSize);
    if (fileSize == 0 || (fileSize % EPO_SET_SIZE) != 0) {
        if (status && statusLen) Ql_sprintf(status, "Bad size:%lu", fileSize);
        return false;
    }

    int f;
    if (Ql_strncmp(filename, "RAM:", 4) == 0) {
        f = Ql_FS_OpenRAMFile((char*)filename, QL_FS_READ_ONLY, fileSize);
    } else {
        f = Ql_FS_Open((char*)filename, QL_FS_READ_ONLY);
    }

    if(f < 0) {
        if (status && statusLen) Ql_sprintf(status, "Open fail:%d", f);
        return false;
    }

    // Erase existing flash EPO (safe for manual update)
    if (!epo_erase_flash()) {
        Ql_FS_Close(f);
        if (status && statusLen) Ql_strncpy(status, "PAIR472 fail", statusLen - 1);
        return false;
    }

    int beforeSet = 0;
    if (epo_query_flash_status_gps(&beforeSet)) {
        EPO_LOGV("EPO status before inject: set=%d", beforeSet);
    }

    binary_payload_t payload;
    uint8_t frame[BINARY_MAX_DATA_SIZE];
    uint8_t set_buf[EPO_SET_SIZE];
    u32 readsize = 0;
    const char sys_type = 'G';

    // Start
    Ql_memset(&payload, 0, sizeof(payload));
    payload.message_id = EPO_START_MSG_ID;
    payload.data_size = 1;
    payload.data[0] = (uint8_t)sys_type;
    int16_t frameLen = encode_binary_packet(frame, sizeof(frame), &payload);
    if(frameLen <= 0 || !epo_send_with_ack(EPO_START_MSG_ID, frame, (uint16_t)frameLen)) {
        Ql_FS_Close(f);
        if (status && statusLen) Ql_strncpy(status, "Start ACK fail", statusLen - 1);
        return false;
    }

    // Data
    uint32_t setIndex = 0;
    while(1)
    {
        int ret = Ql_FS_Read(f, set_buf, EPO_SET_SIZE, &readsize);
        if(ret != QL_RET_OK || readsize != EPO_SET_SIZE) {
            break;
        }

        EPO_LOGV("Injecting set %lu", (unsigned long)setIndex);

        for(int i = 0; i < EPO_SET_SIZE; i += SAT_SIZE)
        {
            Ql_memset(&payload, 0, sizeof(payload));
            payload.message_id = EPO_DATA_MSG_ID;
            payload.data_size = SAT_SIZE;
            Ql_memcpy(payload.data, &set_buf[i], SAT_SIZE);

            frameLen = encode_binary_packet(frame, sizeof(frame), &payload);
            if(frameLen <= 0 || !epo_send_with_ack(EPO_DATA_MSG_ID, frame, (uint16_t)frameLen)) {
                Ql_FS_Close(f);
                if (status && statusLen) Ql_strncpy(status, "Data ACK fail", statusLen - 1);
                return false;
            }
        }

        setIndex++;
    }

    // End
    Ql_memset(&payload, 0, sizeof(payload));
    payload.message_id = EPO_END_MSG_ID;
    payload.data_size = 1;
    payload.data[0] = (uint8_t)sys_type;
    frameLen = encode_binary_packet(frame, sizeof(frame), &payload);
    if(frameLen <= 0 || !epo_send_with_ack(EPO_END_MSG_ID, frame, (uint16_t)frameLen)) {
        Ql_FS_Close(f);
        if (status && statusLen) Ql_strncpy(status, "End ACK fail", statusLen - 1);
        return false;
    }

    Ql_FS_Close(f);

    // Verify
    int setCount = 0;
    epo_pair470_status_t st470;
    Ql_memset(&st470, 0, sizeof(st470));
    if (!epo_query_flash_status_gps_full(&st470) || st470.set <= 0) {
        if (status && statusLen) Ql_strncpy(status, "Verify fail", statusLen - 1);
        return false;
    }

    setCount = st470.set;

    EPO_LOGV("EPO status after inject: set=%d flash=%d/%d..%d/%d used=%d/%d..%d/%d",
             st470.set,
             st470.fwn, st470.ftow, st470.lwn, st470.ltow,
             st470.fcwn, st470.fctow, st470.lcwn, st470.lctow);

    // Best-effort aiding (per AGNSS guide): send reference time/location after receiver boots.
    // We do it here as well so the next TTFF benefits immediately.
    bool refTimeOk = epo_send_ref_time_utc_now();
    bool refPosOk  = epo_send_ref_position_best_effort();
    EPO_LOGV("Aiding sent: time=%d pos=%d", refTimeOk ? 1 : 0, refPosOk ? 1 : 0);

    // Important: for Flash EPO, GNSS typically needs a reboot to reload EPO from NVM.
    // Request a safe re-init from GPS thread (won't interfere with baudrate switching).
    GPS_RequestReinit();
    EPO_LOGV("Requested GNSS reinit after EPO inject");

    if (status && statusLen) {
        // Report flash coverage window so the host can validate EPO freshness.
        // (FWN/FTOW..LWN/LTOW correspond to the first/last sets stored in flash.)
        Ql_sprintf(status, "OK,set=%d,time=%d,pos=%d,rst=1,fw=%d/%d,lw=%d/%d",
                   setCount, refTimeOk ? 1 : 0, refPosOk ? 1 : 0,
                   st470.fwn, st470.ftow, st470.lwn, st470.ltow);
    }
    return true;
}

void SendEPOFile(const char* filename)
{
    char st[64];
    if (!EPO_FlashInjectFile(filename, st, sizeof(st))) {
        EPO_LOGE("EPO transfer failed: %s", st);
        return;
    }
    EPO_LOGE("EPO transfer completed: %s", st);
}

// Check if EPO data exists in flash
bool CheckEPODataInFlash(void)
{
    int setCount = 0;
    if (!epo_query_flash_status_gps(&setCount)) {
        return false;
    }
    return (setCount > 0);
}

// Erase expired EPO data
bool EraseEPOData(void)
{
    return epo_erase_flash();
}

// Legacy helpers kept for compatibility/testing.
// NOTE: Prefer the internal UTC/PAIR600-full versions above.
bool SendReferenceTime(const _RTC* time)
{
    if (!time) return false;
    char cmd[120];
    Ql_memset(cmd, 0, sizeof(cmd));
    Ql_sprintf(cmd, "PAIR590,%04d,%02d,%02d,%02d,%02d,%02d",
               (int)time->Year + 2000, (int)time->Month, (int)time->Date,
               (int)time->Hour, (int)time->Min, (int)time->Sec);
    return send_pair_command_wait(cmd, "$PAIR001,590,0", NULL, 0, 1500);
}

bool SendReferencePosition(double latitude, double longitude, double altitude)
{
    // Send with conservative accuracy fields per guide.
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        return false;
    }
    char cmd[160];
    Ql_memset(cmd, 0, sizeof(cmd));
    Ql_sprintf(cmd, "PAIR600,%.6f,%.6f,%.2f,30000,30000,0,100", latitude, longitude, altitude);
    return send_pair_command_wait(cmd, "$PAIR001,600,0", NULL, 0, 1500);
}

#endif