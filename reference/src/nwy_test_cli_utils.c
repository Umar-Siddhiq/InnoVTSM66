#include "nwy_test_cli_adpt.h"
#include "nwy_test_cli_utils.h"

#define NWY_CLI_ECHO_LEN (2 * 1024)
#define NWY_CLI_RX_LEN (2 * 1024)
static char nwy_cli_opt_input_buff[NWY_CLI_RX_LEN] = {0};
static int nwy_ext_sio_len = 0;
static uint8 nwy_sio_mode = 0;
static nwy_sio_trans_cb nwy_test_cli_trans_func = NULL;
static char echo_str[NWY_CLI_ECHO_LEN];

void nwy_test_cli_echo(char *fmt, ...)
{
    static int flag = 0;
    static nwy_osi_mutex_t echo_mutex;
    va_list a;
    int i, size;

    if (0 == flag) {
        nwy_create_mutex(&echo_mutex);
        flag = 1;
    }
    nwy_lock_mutex(echo_mutex, NWY_OSA_SUSPEND);
    memset(echo_str,0x00,NWY_CLI_ECHO_LEN);
    va_start(a, fmt);
    vsnprintf(echo_str, NWY_CLI_ECHO_LEN, fmt, a);
    va_end(a);
    size = strlen((char *)echo_str);
    i = 0;
    //nwy_open_sdk_log(echo_str);
    while (1)
    {
        int tx_size;

        tx_size = nwy_uart_send_data(1, (char *)echo_str + i, size - i);
        if (tx_size <= 0)
            break;
        i += tx_size;
        if ((i < size))
            nwy_sleep(10);
        else
            break;
    }
    nwy_unlock_mutex(echo_mutex);
}

// void UartStringOut(char *fmt, ...)
// {

//     static int flag = 0;
//     static nwy_osi_mutex_t echo_mutex;
//     va_list a;
//     int i, size;
        
//     if (0 == flag) {
//         nwy_create_mutex(&echo_mutex);
//         flag = 1;
//     }
//     nwy_lock_mutex(echo_mutex, NWY_OSA_SUSPEND);
//     memset(echo_str,0x00,NWY_CLI_ECHO_LEN);
//     va_start(a, fmt);
//     vsnprintf(echo_str, NWY_CLI_ECHO_LEN, fmt, a);
//     va_end(a);
//     size = strlen((char *)echo_str);
//     i = 0;
//     while (1)
//     {
//         int tx_size;

//         tx_size = nwy_uart_send_data(1, (char *)echo_str + i, size - i);
//         if (tx_size <= 0)
//             break;
//         i += tx_size;
//         if ((i < size))
//             nwy_sleep(10);
//         else
//             break;
//     }
//     nwy_unlock_mutex(echo_mutex);
// }

void nwy_test_cli_output(void *data, uint32 length)
{
    char *out = (char *)data;
    int i = 0;
    while (1)
    {
        int tx_size;

        tx_size = nwy_uart_send_data(1, out + i, length - i);
        if (tx_size <= 0)
            break;
        i += tx_size;
        if ((i < length))
            nwy_sleep(10);
        else
            break;
    }
}

char *nwy_test_cli_input_gets(char *msg, ...)
{
    va_list a;
    va_start(a, msg);
    vsprintf(echo_str, msg, a);
    va_end(a);
    nwy_uart_send_data(1, echo_str, strlen((char *)echo_str));
    nwy_test_cli_wait_select();
    return nwy_cli_opt_input_buff;
}

char *nwy_test_input_rcv(void)
{
    nwy_test_cli_wait_select();
    return nwy_cli_opt_input_buff;
}

int nwy_test_cli_input_len_gets()
{
    return nwy_ext_sio_len;
}

extern uint8 nwy_app_update_flag;
extern void nwy_ext_app_info_proc(const uint8 *data, int length);
void nwy_test_cli_sio_data_proc(const char *data, uint32 length)
{
    static int len = 0;
    NWY_CLI_LOG("\r\nnwy length = %d, len = %d", length,len);
    NWY_CLI_LOG("\r\nnwy nwy_sio_mode = %d, size = %d", nwy_sio_mode,len + length);
    // nwy_dbg_log("\r\nMCUCB");
    // MCURcv((char*)data,length);
    if (nwy_app_update_flag == 0) {
        if (nwy_sio_mode == 0)
        {
            if (length == 2 && data[length - 1] == '\n' && data[length - 2] == '\r')
            {
                NWY_CLI_LOG("\r\ndrop \\r\\n and input end nwy_ext_sio_len = %d len = %d", nwy_ext_sio_len, len);
                if (len == 0)
                {
                    memset(nwy_cli_opt_input_buff, 0, NWY_CLI_RX_LEN);
                    nwy_ext_sio_len = 0;
                }
                len = 0;
                nwy_test_cli_select_enter();
                return;
            }
            // clear the nwy_ext_sio_recv_buff,in next time
            if (len == 0)
            {
                NWY_CLI_LOG("\r\nnwy reset buff");
                nwy_ext_sio_len = 0;
                memset(nwy_cli_opt_input_buff, 0, NWY_CLI_RX_LEN);
            }
            if (len + length > NWY_CLI_RX_LEN)
            {
                memcpy(nwy_cli_opt_input_buff + len, data, (NWY_CLI_RX_LEN - len));
                nwy_ext_sio_len = nwy_ext_sio_len + (NWY_CLI_RX_LEN - len);
                len = 0;
                //nwy_test_cli_echo("\r\n%s", nwy_cli_opt_input_buff);
                nwy_test_cli_select_enter();
            }
            else
            {
                memcpy(nwy_cli_opt_input_buff + len, data, length);
                NWY_CLI_LOG("\r\nnwy length = %d, len = %d", length,len);
        
                if (length > 1)
                {
                    if (data[length - 1] == '\n' && data[length - 2] == '\r')
                    {
                        nwy_cli_opt_input_buff[len + length - 2] = '\0';
                        nwy_ext_sio_len = nwy_ext_sio_len + length - 2;
                        len = 0;
        
                        NWY_CLI_LOG("nwy: recv over nwy_ext_sio_len = %d", nwy_ext_sio_len);
                        //nwy_test_cli_echo("\r\n%s", nwy_cli_opt_input_buff);
                        nwy_test_cli_select_enter();
                        return;
                    }
                    else
                    {
                        nwy_ext_sio_len = nwy_ext_sio_len + length;
                    }
                }
                else if (length == 1)
                {
                    if (data[length - 1] == '\r' || data[length - 1] == '\n')
                    {
                        nwy_cli_opt_input_buff[nwy_ext_sio_len - 1] = '\0';
                        nwy_ext_sio_len = nwy_ext_sio_len - 1;
                        len = 0;
                        //nwy_test_cli_echo("\r\n%s", nwy_cli_opt_input_buff);
                        nwy_test_cli_select_enter();
                        return;
                    }
                    else
                    {
                        memcpy(nwy_cli_opt_input_buff + len, data, length);
                        nwy_ext_sio_len = nwy_ext_sio_len + length;
                    }
                }
            }
            len = len + length;
        }
        else if (nwy_sio_mode == 1)
        {
            nwy_test_cli_trans_func(data, length);
        }
    }
    else if(nwy_app_update_flag == 1)
    {
        NWY_CLI_LOG("\r\nnwy fota length = %d, len = %d", length,len);
        nwy_ext_app_info_proc(data, length);
    }
}

int nwy_test_cli_sio_enter_trans_mode(nwy_sio_trans_cb recv_cb)
{
    if (recv_cb == NULL)
    {
        return -1;
    }

    if (nwy_sio_mode == 1)
    {
        return -1;
    }

    nwy_sio_mode = 1;
    nwy_test_cli_trans_func = recv_cb;
    return 0;
}

void nwy_test_cli_sio_quit_trans_mode()
{
    nwy_sio_mode = 0;
}
