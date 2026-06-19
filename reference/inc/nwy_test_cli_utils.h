#ifndef __NWY_TEST_CLI_UTILS_H__
#define __NWY_TEST_CLI_UTILS_H__

#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "nwy_test_cli_adpt.h"

#define NWY_UART_RECV_SINGLE_MAX   512
#define NWY_EXT_SIO_RX_MAX         (2*1024)
#define CERT_EFS_CHECKSUM_STR_SIZE 4


#define NWY_EXT_INPUT_RECV_MSG          (NWY_APP_EVENT_ID_BASE + 1)
#define NWY_EXT_DATA_REC_END_MSG        (NWY_APP_EVENT_ID_BASE + 2)

typedef void (*nwy_sio_trans_cb) (const char *data, int length);

void nwy_test_cli_echo(char* fmt, ...);
void nwy_test_cli_output(void *data, uint32 length);
char* nwy_test_cli_input_gets(char* msg, ...);
void nwy_test_cli_sio_data_proc(const char *data, uint32 length);
int nwy_test_cli_input_len_gets(void);
char *nwy_test_input_rcv(void);
int nwy_test_cli_sio_enter_trans_mode(nwy_sio_trans_cb recv_cb);
void nwy_test_cli_sio_quit_trans_mode(void);
void nwy_sms_test_recv_cb(void *data, size_t size);
void nwy_nw_register_callback_cb(nwy_nw_regs_ind_type_t ind_type, void * ind_struct);
#endif
