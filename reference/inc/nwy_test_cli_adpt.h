#ifndef __NWY_TEST_CLI_ADPT_H__
#define __NWY_TEST_CLI_ADPT_H__

#include "cs_types.h"

#if 1
#include "nwy_osi_api.h"
#include "nwy_usb_serial.h"
#include "nwy_data.h"
#include "nwy_open_socket.h"
#include "nwy_ftp.h"
#include "nwy_file.h"
#include "nwy_http.h"
#include "nwy_md5.h"
#include "nwy_vir_at.h"
#include "nwy_sms.h"
#include "nwy_dm.h"
#include "nwy_pm.h"
#include "nwy_uart.h"
#include "nwy_gpio_open.h"
#include "nwy_spi.h"
#include "nwy_fota.h"
#include "nwy_sim.h"
#include "nwy_network.h"
#include "nwy_adc.h"
#include "nwy_i2c.h"
#include "nwy_lcd_bus.h"
#include "nwy_camera.h"
#include "nwy_sd.h"
#include "nwy_audio_api.h"
#endif

void nwy_test_cli_dbg(const char *func, int line, char* fmt, ... );
#define NWY_CLI_LOG(...) nwy_test_cli_dbg(__func__, __LINE__, __VA_ARGS__)

#define NWY_OPEN_TEST_ADC
#define NWY_OPEN_TEST_SPI
#define NWY_OPEN_TEST_GENERAL_FLASH_MOUNT
#define NWY_OPEN_TEST_I2C
#define NWY_OPEN_TEST_VIRT_AT
#define NWY_OPEN_TEST_CHIPID_NS
#define NWY_OPEN_TEST_BOOT_CAUSE_NS
#define NWY_OPEN_TEST_LCD
#define NWY_OPEN_TEST_AT_FWD
#define NWY_OPEN_TEST_HEAP_INFO
#define NWY_OPEN_TEST_CAMERA
#define NWY_OPEN_TEST_SD

#define NWY_OPEN_TEST_GNSS
#define NWY_OPEN_TEST_AGPS_NS
#define NWY_OPEN_TEST_LOC_NS

#define NWY_OPEN_TEST_WIFI_LOC_NS
#define NWY_OPEN_TEST_AUDIO
#define NWY_OPEN_TEST_VOICE
#define NWY_OPEN_TEST_RTC
#if 0
#define NWY_OPEN_TEST_SMS
#define NWY_OPEN_TEST_VIRT_AT
//this is not support feature
#define NWY_OPEN_TEST_FS_ADV_NS
#define NWY_OPEN_TEST_DIR_ADV_NS
#define NWY_OPEN_TEST_IMS_NS

//this is support feature
#define NWY_OPEN_TEST_FLOW_S
#define NWY_OPEN_TEST_CFGDFTPDN_S
#endif
#ifndef FEATURE_NWY_OPEN_CPU_NO_SMS
#define NWY_OPEN_TEST_SMS
#endif
#ifndef FEATURE_NWY_OPEN_CPU_NO_PAHO
#define FEATURE_NWY_PAHO_MQTT_V3
#endif
#ifdef FEATURE_NWY_OPEN_CPU_NO_HTTP
#define NWY_OPEN_TEST_HTTP_NS
#endif
#ifdef FEATURE_NWY_OPEN_CPU_NO_FTP
#define NWY_OPEN_TEST_FTP_NS
#endif
#define NWY_OPEN_TEST_NTP_TIME
#define RS485_GPIO_PORT (2)
#define RS485_DIR_TX (1) //hight level for send
#define RS485_DIR_RX (0) //low level for recv

#define SPI_DUMMY 0xFF
#define w25x_jedecDeviceID 0x9f

#define NWY_OPEN_MAX_SPI_CHANNEL  2
#define NWY_OPEN_MAX_UART_CHANNEL 3
#define NWY_OPEN_UART_CHANNEL_NS  0

#define NWY_BAND_LOCK_NUM         4
#define NWY_FREQ_LOCK_NUM         3
#define NWY_OPEN_LOCK_AUTO        0
#define NWY_OPEN_LOCK_GSM         2
#define NWY_OPEN_LOCK_WCDMA       3
#define NWY_OPEN_LOCK_LTE         4
#define NWY_OPEN_LOCK_NS          0xFF

#define NWY_OPEN_DEL_SMS_NUM         4
#define NWY_OPEN_DEL_SMS_READ         1
#define NWY_OPEN_DEL_SMS_READ_SEND        2
#define NWY_OPEN_DEL_SMS_READ_SEND_UNSEND    3
#define NWY_OPEN_DEL_SMS_ALL       4
#define NWY_OPEN_DEL_SMS_NS          0xFF
#define MWY_OPEN_READ_SMS_LIST 1

typedef enum nwy_pdp_status_type
{
  NWY_PDP_DISCONNECTED,
  NWY_PDP_CONNECTED,
  NWY_PDP_CONNECTTING,
  NWY_PDP_DISCONNECTTING,
}nwy_pdp_status_type;

int nwy_test_cli_wait_select(void);
void nwy_test_cli_select_enter(void);
void nwy_test_cli_send_trans_end(void);
int nwy_test_cli_wait_trans_end(void);
void nwy_cli_init_unsol_reg(void);
int nwy_test_cli_check_uart_mode(uint8_t uart_mode);
void nwy_exit_thread_self();

#endif
