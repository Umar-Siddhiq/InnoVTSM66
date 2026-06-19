#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_adpt.h"

#define NWY_TASK_STACK_SIZE     1024
static nwy_osi_timer_t s_nwy_test_timer = NULL;
static int s_nwy_semaphore_count = 0;
nwy_osi_semaphore_t s_nwy_cli_semaphore;
static nwy_osi_thread_t s_nwy_task_A = NULL;
static nwy_osi_thread_t s_nwy_task_B = NULL;

#ifdef NWY_OPEN_TEST_VIRT_AT
static int s_nwy_at_init_flag = 0;
#endif

static int s_timer_count = 0;
static void nwy_test_cli_timer_cb(nwy_timer_cb_para_t ctx)
{
    s_timer_count++;
    nwy_sdk_timer_start(s_nwy_test_timer, 1000, nwy_test_cli_timer_cb, 0, NWY_TIMER_PERIODIC);
}

static void nwy_cli_semaphore_taskA_proc(void *ctx)
{
    int i = 0;
    nwy_semaphore_acquire(s_nwy_cli_semaphore, NWY_OSA_SUSPEND);
    for (i = 0; i < 100; i++)
    {
        nwy_test_cli_echo("\r\nTaskA semaphore_count = %d", ++s_nwy_semaphore_count);
        nwy_sleep(10);
    }
    nwy_semahpore_release(s_nwy_cli_semaphore);

    if (s_nwy_task_A != NULL) {
        nwy_suspend_thread(s_nwy_task_A);
    }
    nwy_exit_thread_self();
}
void nwy_cli_semaphore_taskB_proc(void *ctx)
{
    int i = 0;
    nwy_semaphore_acquire(s_nwy_cli_semaphore, NWY_OSA_SUSPEND);
    for (i = 0; i < 50; i++)
    {
        nwy_test_cli_echo("\r\nTaskB semaphore_count = %d", --s_nwy_semaphore_count);
        nwy_sleep(10);
    }
    nwy_semahpore_release(s_nwy_cli_semaphore);
    if (s_nwy_task_B != NULL) {
        nwy_suspend_thread(s_nwy_task_B);
    }
    nwy_exit_thread_self();
}

#ifdef NWY_OPEN_TEST_VIRT_AT
static void nwy_cli_virtual_at_test()
{
    char *sptr = NULL;
    int ret = -1;
    char resp[2048] = {0};
    nwy_at_info_t at_cmd;
    nwy_sleep(500);

    sptr = nwy_test_cli_input_gets("\r\nSend AT cmd :");

    memset(&at_cmd, 0, sizeof(nwy_at_info_t));
    memcpy(at_cmd.at_command, sptr, strlen(sptr));
    at_cmd.at_command[strlen(sptr)] = '\r';
    at_cmd.at_command[strlen(sptr) + 1] = '\n';
    at_cmd.length = strlen(at_cmd.at_command);

    sptr = nwy_test_cli_input_gets("\r\nAT OK FORMAT:");
    if(strlen(sptr) > 0){
       memcpy(at_cmd.ok_fmt, sptr, strlen(sptr));
    }

    sptr = nwy_test_cli_input_gets("\r\nAT ERROR FORMAT:");
    if(strlen(sptr) > 0){
       memcpy(at_cmd.err_fmt, sptr, strlen(sptr));
    }

    ret = nwy_sdk_at_cmd_send(&at_cmd, resp,sizeof(resp), NWY_AT_TIMEOUT_DEFAULT);
    if (ret == NWY_SUCESS)
    {
        nwy_test_cli_echo("\r\n Resp:%s", resp);
    }
    else if (ret == NWY_AT_GET_RESP_TIMEOUT)
    {
        nwy_test_cli_echo("\r\n AT timeout");
    }
    else
    {
        nwy_test_cli_echo("\r\n AT ERROR");
    }
}

#ifdef NWY_OPEN_TEST_AT_FWD
static void nwy_at_cmd_process_callback(void *handle, char *atcmd, int type, char *para0, char *para1, char *para2)
{
    switch (type)
    {
    case 0:
        nwy_at_forward_send(handle, "\r\n", 2);
        nwy_at_forward_send(handle, atcmd, strlen(atcmd));
        if (para0)
        {
            nwy_at_forward_send(handle, para0, strlen(para0));
        }

        if (para1)
        {
            nwy_at_forward_send(handle, para1, strlen(para1));
        }

        if (para2)
        {
            nwy_at_forward_send(handle, para2, strlen(para2));
        }

        nwy_at_forward_send(handle, "\r\nOK\r\n", 6);
        break;
    case 1:
        nwy_at_forward_send(handle, "\r\nTEST\r\n", 8);
        break;
    case 2:
        nwy_at_forward_send(handle, "\r\nREAD\r\n", 8);
        break;
    case 3:
        nwy_at_forward_send(handle, "\r\nEXE\r\n", 7);
        break;
    }
}
#endif

void nwy_cli_pull_out_sim(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\n SIM pull out");
}
#ifdef NWY_OPEN_TEST_SMS
void nwy_cli_recv_sms(uint8 *data, int len)
{
    nwy_sms_recv_info_type_t sms_data = {0};

    nwy_sms_recv_message(&sms_data);

    if (1 == sms_data.cnmi_mt)
    {
        nwy_test_cli_echo("\r\n recv one sms index:%d", sms_data.nIndex);
    }
    else if (2 == sms_data.cnmi_mt)
    {
        nwy_test_cli_echo("\r\n recv one sms from:%s msg_context:%s", sms_data.oa, sms_data.pData);
    }
}
#endif
void nwy_cli_tcpsetup_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nTCPSETUP: ");
    nwy_test_cli_echo("%s\r\n", data);
}
void nwy_cli_tcprecvs_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nTCPRECV(S): ");
    nwy_test_cli_echo("%s\r\n", data);
}
void nwy_cli_tcp_close_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nTCPCLOSE:");
    nwy_test_cli_echo("%s\r\n", data);
}

void nwy_cli_tcprecv_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nTCPRECV:");
    nwy_test_cli_echo("%s\r\n", data);
}
void nwy_cli_acpt_close_cb(char *data)
{
    nwy_test_cli_echo("\r\nCLOSECLIENT: ");
    nwy_test_cli_echo("%s\r\n", data);
}

void nwy_cli_udprecv_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nUDPRECV:");
    nwy_test_cli_echo("%s\r\n", data);
}

void nwy_cli_client_acpt_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nConnect AcceptSocket=");
    nwy_test_cli_echo("%s\r\n", data);
}

void nwy_cli_gprs_disconnect_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\nGPRS DISCONNECTION");
    nwy_test_cli_echo("%s\r\n", data);
}

void nwy_cli_sms_list_resp_cb(uint8 *data, int len)
{
    nwy_test_cli_echo("\r\n+CMGL: ");
    nwy_test_cli_echo("%s\r\n", data);
}

#endif

static int nwy_cli_data_valid_check(nwy_time_t *time_info, int timezone)
{
    int monthLen[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (time_info == NULL) {
        return NWY_GEN_E_INVALID_PARA;
    }

    if ((time_info->year > 2100 || time_info->year < 2000) || (time_info->mon > 12 || time_info->mon < 1) ||
        ((time_info->day > monthLen[time_info->mon-1]) || time_info->day < 1)) {
            return NWY_GEN_E_INVALID_PARA;
        }

    if ((time_info->hour > 23 || time_info->hour < 0) || (time_info->min > 59 || time_info->min < 0) ||
        (time_info->sec > 59 || time_info->sec < 0)) {
            return NWY_GEN_E_INVALID_PARA;
        }

    if (timezone > 96 || timezone < -96) {
        return NWY_GEN_E_INVALID_PARA;
    }

    return NWY_SUCESS;
}

void nwy_test_cli_get_model()
{
    char buff[128] = {0};

    nwy_dm_get_dev_model(buff, sizeof(buff));
    nwy_test_cli_echo("\r\nDev model : %s", buff);
}

void nwy_test_cli_get_imei()
{
    char imei[16] = {0};
    nwy_error_e ret = NWY_GEN_E_UNKNOWN;

    ret = nwy_dm_get_imei(imei, sizeof(imei));
    if (NWY_SUCESS != ret)
    {
        nwy_test_cli_echo("\r\n Get IMEI error \r\n");
        return;
    }

    nwy_test_cli_echo("\r\nIMEI:%s \r\n", imei);
}

void nwy_test_cli_get_chipid()
{
#ifdef NWY_OPEN_TEST_CHIPID_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    uint8_t uid[8] = {0};
    int i;
    nwy_get_chip_id(uid);
    nwy_test_cli_echo("\r\nChip ID : 0x");
    for (i = 0; i < 8; i++)
    {
        nwy_test_cli_echo("%02x", uid[i]);
    }
    nwy_test_cli_echo("\r\n");
#endif
}

void nwy_test_cli_set_app_version()
{
#ifdef NWY_OPEN_TEST_APP_VER_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr = NULL;

    sptr = nwy_test_cli_input_gets("\r\nPlease input app version: ");

    if(strlen(sptr) == 0 || strlen(sptr) >= 128){
        nwy_test_cli_echo("\r\nInput string is invalid!!\r\n");
        return;
    }

    nwy_dm_set_app_version(sptr,strlen(sptr));

    nwy_test_cli_echo("\r\nSet APP version success(AT+NAPPCHECK?)!! ver:%s\r\n",sptr);
#endif
}


void nwy_test_cli_get_boot_cause()
{
#ifdef NWY_OPEN_TEST_BOOT_CAUSE_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    unsigned int causes = nwy_get_boot_causes();
    char string_buf[256];
    memset(string_buf, 0, sizeof(string_buf));
    int idx = sprintf(string_buf, "boot cause[%02x]:", causes);
    if (causes == NWY_BOOTCAUSE_UNKNOWN)
        idx += sprintf(string_buf + idx, " %s", "UNKNOWN");
    else
    {
        if (causes & NWY_BOOTCAUSE_PWRKEY)
            idx += sprintf(string_buf + idx, " %s", "PWRKEY");
        if (causes & NWY_BOOTCAUSE_PIN_RESET)
            idx += sprintf(string_buf + idx, " %s", "PIN_RESET");
        if (causes & NWY_BOOTCAUSE_ALARM)
            idx += sprintf(string_buf + idx, " %s", "ALARM");
        if (causes & NWY_BOOTCAUSE_CHARGE)
            idx += sprintf(string_buf + idx, " %s", "CHARGE");
        if (causes & NWY_BOOTCAUSE_WDG)
            idx += sprintf(string_buf + idx, " %s", "WDG");
        if (causes & NWY_BOOTCAUSE_PIN_WAKEUP)
            idx += sprintf(string_buf + idx, " %s", "PIN_WAKEUP");
        //if (causes & NWY_BOOTCAUSE_PSM_WAKEUP)
            //idx += sprintf(string_buf + idx, " %s", "PSM_WAKEUP");
    }
    nwy_test_cli_echo("\r\n%s\r\n", string_buf);
#endif
}

void nwy_test_cli_get_sw_ver()
{
    char ver[64] = {0};

    nwy_dm_get_inner_version(ver, sizeof(ver));

    nwy_test_cli_echo("\r\nThe sw ver is :%s", ver);

	memset(ver, 0, 64);
	nwy_dm_get_open_sdk_version(ver, sizeof(ver));
	nwy_test_cli_echo("\r\nThe open sdk ver is :%s", ver);
}

void nwy_test_cli_get_hw_ver()
{
    char ver[64] = {0};

    nwy_dm_get_hw_version(ver, sizeof(ver));

    nwy_test_cli_echo("\r\nThe hw ver is :%s", ver);
}

void nwy_test_cli_get_heap_info()
{
#ifdef NWY_OPEN_TEST_HEAP_INFO
    char heapinfo[100] = {0};

    nwy_dm_get_device_heapinfo(heapinfo);
    nwy_test_cli_echo("\r\n dev heapinfo:%s\r\n", heapinfo);
#else
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#endif
}
void nwy_test_cli_get_cpu_temp()
{
#ifdef NWY_OPEN_TEST_CPU_TEMP
    float value = 0;
    nwy_dm_get_rftemperature(&value);
    nwy_test_cli_echo("\r\n temperature is %.2f\r\n", value);
#else
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#endif
}

void nwy_test_cli_start_timer(void)
{
    int ret = NWY_GEN_E_UNKNOWN;
    nwy_osi_thread_t hdl = NULL;
    s_timer_count = 0;
    if (s_nwy_test_timer == NULL)
    {
        nwy_get_current_thread(&hdl);
        if (hdl == NULL) {
            return;
        }
        nwy_sdk_timer_create(&s_nwy_test_timer, hdl, nwy_test_cli_timer_cb, NULL);
        if (s_nwy_test_timer == NULL) {
            nwy_test_cli_echo("\r\nCreate timer fail");
            return;
        }
    }

    ret = nwy_sdk_timer_start(s_nwy_test_timer, 1000, nwy_test_cli_timer_cb, 0, NWY_TIMER_PERIODIC);
    if (ret != NWY_SUCESS) {
        nwy_test_cli_echo("\r\nstart timer fail");
    } else {
        nwy_test_cli_echo("\r\nstart timer succes");
    }
}

void nwy_test_cli_stop_timer(void)
{
    int ret = false;
    ret = nwy_sdk_timer_stop(s_nwy_test_timer);
    if (ret == NWY_SUCESS)
    {
        nwy_test_cli_echo("\r\nStop timer sucess,s_timer_count = %d", s_timer_count);
    }
    else
    {
        nwy_test_cli_echo("\r\nStop timer fail");
    }
}

void nwy_test_cli_get_time(void)
{
    nwy_time_t julian_time = {0};
    int timezone = 0;
    nwy_get_time(&julian_time, &timezone);
    nwy_test_cli_echo("\r\n%d-%d-%d-w%d %d:%d:%d timezone = %d\r\n", julian_time.year, julian_time.mon, julian_time.day, julian_time.wday, julian_time.hour, julian_time.min, julian_time.sec, timezone);
}

void nwy_test_cli_set_time(void)
{
    char *sptr = NULL;
    int timezone = 0;
    nwy_time_t nwy_time = {2020, 3, 19, 20, 33, 30};

    sptr = nwy_test_cli_input_gets("\r\nPlease input data(2020-1-1): ");
    nwy_time.year = (atoi(strtok(sptr, "-")));
    nwy_time.mon = (char)(atoi(strtok(NULL, "-")));
    nwy_time.day = (char)(atoi(strtok(NULL, "-")));
    NWY_CLI_LOG("nwy_time: year = %d mon = %c day = %c ", nwy_time.year, nwy_time.mon, nwy_time.day);

    sptr = nwy_test_cli_input_gets("\r\nPlease input time(1:1:1): ");

    nwy_time.hour = (atoi(strtok(sptr, ":")));
    nwy_time.min = (char)(atoi(strtok(NULL, ":")));
    nwy_time.sec = (char)(atoi(strtok(NULL, ":")));

    sptr = nwy_test_cli_input_gets("\r\nPlease input timezone: ");
    timezone = atoi(sptr);

    if (nwy_cli_data_valid_check(&nwy_time, timezone) != NWY_SUCESS) {
        nwy_test_cli_echo("\r\nInput data invalid\n");
        return;
    }

    nwy_set_time(&nwy_time, timezone);

    nwy_test_cli_echo("\r\nset time sucess\r\n");
    nwy_test_cli_get_time();
}

void nwy_test_cli_set_semp(void)
{
    int ret = NWY_GEN_E_UNKNOWN;
    s_nwy_semaphore_count = 0;

    ret = nwy_semaphore_create(&s_nwy_cli_semaphore, 1, 1);
    if (ret != NWY_SUCESS) {
        nwy_test_cli_echo("\r\nsem test fail");
        return ;
    }

    if (s_nwy_task_A != NULL) {
        nwy_exit_thread(s_nwy_task_A);
        s_nwy_task_A = NULL;
    }

    if (s_nwy_task_B != NULL) {
        nwy_exit_thread(s_nwy_task_B);
        s_nwy_task_B = NULL;
    }
    nwy_create_thread(&s_nwy_task_A, NWY_TASK_STACK_SIZE,NWY_OSI_PRIORITY_NORMAL,"nwy_test_cli_semaphoreA", nwy_cli_semaphore_taskA_proc, NULL, 8);
    nwy_create_thread(&s_nwy_task_B , NWY_TASK_STACK_SIZE, (NWY_OSI_PRIORITY_NORMAL + 1),"nwy_test_cli_semaphoreB", nwy_cli_semaphore_taskB_proc, NULL, 8);
}

#ifdef NWY_OPEN_TEST_VIRT_AT
void nwy_test_cli_send_virt_at(void)
{
    if (s_nwy_at_init_flag == 0)
    {
        nwy_sdk_at_parameter_init();
        nwy_cli_init_unsol_reg();
        s_nwy_at_init_flag = 1;
    }

    nwy_cli_virtual_at_test();
}

void nwy_test_cli_reg_at_fwd(void)
{
#ifdef NWY_OPEN_TEST_AT_FWD
    nwy_set_at_forward_cb(1, "+FYTEST1", nwy_at_cmd_process_callback);
#else
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#endif
}
#endif
