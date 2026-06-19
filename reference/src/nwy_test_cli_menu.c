#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_func_def.h"

#define TEST_CLI_MENU_STACK_MAX 5
typedef void (*item_proc_func)();

struct item
{
    const char *name_str;
    struct item *sub_menu;
    item_proc_func func;
};

typedef struct item cli_menu_item;

/*******************************************************************************
                        1. Mobile Network
********************************************************************************/
// 1.Sim
static cli_menu_item sim_menu_arry[] =
    {
        {"1. Sim status", NULL, nwy_test_cli_get_sim_status},
        {"2. Verify PIN", NULL, nwy_test_cli_verify_pin},
        //{"3. Get PIN mode", NULL, nwy_test_cli_get_pin_mode},
        {"3. PIN disable/enable", NULL, nwy_test_cli_set_pin_mode},
        {"4. Change PIN", NULL, nwy_test_cli_change_pin},
        {"5. Verify PUK", NULL, nwy_test_cli_verify_puk},
        {"6. Get IMSI", NULL, nwy_test_cli_get_imsi},
        {"7. Get ICCID", NULL, nwy_test_cli_get_iccid},
#if 0
        {"9. Get MSISDN", NULL, nwy_test_cli_get_msisdn},
        {"10. Set MSISDN", NULL, nwy_test_cli_set_msisdn},
        {"11. Get SIM Slot", NULL, nwy_test_cli_get_sim_slot},
        {"12. Set SIM Slot", NULL, nwy_test_cli_set_sim_slot},
        {"13. Get pin puk times", NULL, nwy_test_cli_get_pin_puk_times},
        {"14. Set CSIM", NULL, nwy_test_cli_set_csim},
#endif
        {NULL, NULL, NULL}};

// 2. Data connection
static cli_menu_item data_menu_arry[] =
    {
        {"1. Create connection", NULL, nwy_test_cli_data_create},
        {"2. Get profile", NULL, nwy_test_cli_get_profile},
        {"3. Set profile", NULL, nwy_test_cli_set_profile},
        {"4. Start connection", NULL, nwy_test_cli_data_start},
        {"5. Get connections info", NULL, nwy_test_cli_data_info},
        {"6. Stop connection", NULL, nwy_test_cli_data_stop},
        {"7. Release connection", NULL, nwy_test_cli_data_release},
        {"8. Get flowcalc info", NULL, nwy_test_cli_data_get_flowcalc},
        {NULL, NULL, NULL}};

// 3. Network
static cli_menu_item nw_menu_arry[] =
    {
        {"1. Get network mode", NULL, nwy_test_cli_nw_get_mode},
        {"2. Set network band lock", NULL, nwy_test_cli_nw_band_lock},
        {"3. Get current operator info", NULL, nwy_test_cli_nw_operator_info},
        /*{"2. Set network mode", NULL, nwy_test_cli_nw_set_mode},
        {"3. Get Forbidden netwrok list", NULL, nwy_test_cli_nw_get_fplmn},
        {"4. Manual network scan", NULL, nwy_test_cli_nw_manual_scan},
        {"5. Set network band lock", NULL, nwy_test_cli_nw_band_lock},*/
        {"4. Set network freq lock", NULL, nwy_test_cli_nw_freq_lock},
        /*{"7. Get network IMS state", NULL, nwy_test_cli_nw_get_ims_st},
        {"8. Set network IMS state", NULL, nwy_test_cli_nw_set_ims_st},
        {"9. Get default pdn info", NULL, nwy_test_cli_nw_get_def_pdn},
        {"10. Set default pdn info", NULL, nwy_test_cli_nw_set_def_pdn},*/
        {"5. Get radio state", NULL, nwy_test_cli_nw_get_radio_st},
        {"6. Set radio state", NULL, nwy_test_cli_nw_set_radio_st},
        {"7. Get Forbidden netwrok list", NULL, nwy_test_cli_nw_get_fplmn},
        /*{"13. Get Radio signal", NULL, nwy_test_cli_nw_get_radio_sign},*/
        {"8. Get CS reg status", NULL, nwy_test_cli_nw_cs_st},
        {"9. Get PS reg status", NULL, nwy_test_cli_nw_ps_st},
        {"10. Get NETMSG info", NULL, nwy_test_cli_nw_get_netmsg},
        /*{"16. Get LTE reg status", NULL, nwy_test_cli_nw_lte_st},

        {"18. Get EHPLMN info", NULL, nwy_test_cli_nw_get_ehplmn},*/
        {"19. Get signal rssi", NULL, nwy_test_cli_nw_get_signal_rssi},
        /*{"20. Get NETMSG info", NULL, nwy_test_cli_nw_get_netmsg},
        {"21. Get cfgdftpdn info", NULL, nwy_test_cli_nw_get_cfgdftpdn_info},
        {"22. Set cfgdftpdn info", NULL, nwy_test_cli_nw_set_cfgdftpdn_info},*/
        {"23. Get cell info", NULL, nwy_test_cli_nw_get_cellmsg},
        {NULL, NULL, NULL}};

// 4. Voice call
#ifdef NWY_OPEN_TEST_VOICE
static cli_menu_item voice_menu_arry[] =
    {
        {"1. Call start", NULL, nwy_test_cli_voice_call_start},
        {"2. Call end", NULL, nwy_test_cli_voice_call_end},
        {"3. Auto answer", NULL, nwy_test_cli_voice_auto_answ},
        {"4. Set VoLTE voice", NULL, nwy_test_cli_voice_volte_set},
        {"5. Caller id", NULL, nwy_test_cli_voice_caller_id},
        {"6. Call hold", NULL, nwy_test_cli_voice_call_hold},
        {"7. Call unhold", NULL, nwy_test_cli_voice_call_unhold},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const voice_menu_arry = NULL;
#endif
//#endif
//5. SMS
#ifdef NWY_OPEN_TEST_SMS
static cli_menu_item sms_menu_arry[] =
    {
        {"1. Send SMS", NULL, nwy_test_cli_sms_send},
        {"2. Delete SMS", NULL, nwy_test_cli_sms_del},
        {"3. Get SCA", NULL, nwy_test_cli_sms_get_sca},
        {"4. Set SCA", NULL, nwy_test_cli_sms_set_sca},
        {"5. Set storage", NULL, nwy_test_cli_sms_set_storage},
        {"6. Get storage", NULL, nwy_test_cli_sms_get_storage},
        {"7. Set report mode", NULL, nwy_test_cli_sms_set_report_md},
        {"8. Read SMS by index", NULL, nwy_test_cli_sms_read},
        {"9. Read SMS list", NULL, nwy_test_cli_sms_list},
        {"10. Delete SMS by type", NULL, nwy_test_cli_sms_del_type},
        {NULL, NULL, NULL}};
#else
        static cli_menu_item * const sms_menu_arry = NULL;
#endif

static cli_menu_item mobile_menu_arry[] =
    {
        {"1. SIM", sim_menu_arry, NULL},
        {"2. Data connection", data_menu_arry, NULL},
        {"3. Network", nw_menu_arry, NULL},
        {"4. Voice call", voice_menu_arry, NULL},
#ifdef NWY_OPEN_TEST_SMS
        {"5. SMS", sms_menu_arry, NULL},
#endif
        {NULL, NULL, NULL}};
#if 0
#if 0

/*******************************************************************************
                        2. Driver & Kernel
********************************************************************************/
//1 Peripherals
//1.1 UART
static cli_menu_item uart_menu_arry[] =
    {
        {"1. Init", NULL, nwy_test_cli_uart_init},
        {"2. Set baudrate", NULL, nwy_test_cli_uart_set_baud},
        {"3. Get baudrate", NULL, nwy_test_cli_uart_get_baud},
        {"4. Set para", NULL, nwy_test_cli_uart_set_para},
        {"5. Get para", NULL, nwy_test_cli_uart_get_para},
        {"6. Set recv frame timeout", NULL, nwy_test_cli_uart_set_tout},
        {"7. Send data", NULL, nwy_test_cli_uart_send},
        {"8. Register recv callback", NULL, nwy_test_cli_uart_reg_rx_cb},
        {"9. Register tx callback", NULL, nwy_test_cli_uart_reg_tx_cb},
        {"10. Deinit", NULL, nwy_test_cli_uart_deinit},
        {NULL, NULL, NULL}};
#endif
#endif

//1.2 I2C
#ifdef NWY_OPEN_TEST_I2C
static cli_menu_item i2c_menu_arry[] =
    {
        {"1. Init", NULL, nwy_test_cli_i2c_init},
        {"2. Read(reg/mem)", NULL, nwy_test_cli_i2c_read},
        {"3. Write(reg/mem)", NULL, nwy_test_cli_i2c_write},
        {"4. Deinit", NULL, nwy_test_cli_i2c_deinit},
        #if 0
        {"4. Put raw byte", NULL, nwy_test_cli_i2c_put_raw},
        {"5. Get raw byte", NULL, nwy_test_cli_i2c_get_raw},
        #endif
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const i2c_menu_arry = NULL;
#endif

//1.3 SPI
#ifdef NWY_OPEN_TEST_SPI
static cli_menu_item spi_menu_arry[] =
    {
        {"1. Init", NULL, nwy_test_cli_spi_init},
        {"2. Transfer", NULL, nwy_test_cli_spi_trans},
        {"3. Deinit", NULL, nwy_test_cli_spi_deinit},
        {"4. General spi mount", NULL, nwy_test_cli_spi_flash_mount},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const spi_menu_arry = NULL;
#endif
//1.4 GPIO
static cli_menu_item gpio_menu_arry[] =
    {
        {"1. Set value", NULL, nwy_test_cli_gpio_set_val},
        {"2. Get value", NULL, nwy_test_cli_gpio_get_val},
        {"3. Set direction", NULL, nwy_test_cli_gpio_set_dirt},
        {"4. Get direction", NULL, nwy_test_cli_gpio_get_dirt},
        {"5. Config irq", NULL, nwy_test_cli_gpio_config_irq},
        {"6. Enable irq", NULL, nwy_test_cli_gpio_enable_irq},
        {"7. Disable irq", NULL, nwy_test_cli_gpio_disable_irq},
        {"8. Close", NULL, nwy_test_cli_gpio_close},
        {NULL, NULL, NULL}};
//1.5 ADC (no sub menu)
#ifndef NWY_OPEN_TEST_ADC
#define nwy_test_cli_adc_read NULL
#endif
//1.6 PM
static cli_menu_item pm_menu_arry[] =
    {
        #if 0
        {"1. Set power save mode", NULL, nwy_test_cli_pm_save_md},
        {"2. Get power state", NULL, nwy_test_cli_pm_get_pwr_st},
        #endif
        {"3. Power off", NULL, nwy_test_cli_pm_pwr_off},
        #if 0
        {"4. Set dtr mode", NULL, nwy_test_cli_pm_set_dtr},
        {"5. Control powerkey", NULL, nwy_test_cli_pm_pwr_key},
        {"6. Switch sub power", NULL, nwy_test_cli_pm_switch_sub_pwr},
        {"7. Set sub power level", NULL, nwy_test_cli_pm_set_sub_pwr},
        {"8. Set auto poweroff threshold", NULL, nwy_test_cli_pm_set_auto_off},
        {"9. Regsiter charger notice callback", NULL, nwy_test_cli_pm_reg_charger_cb},
        #endif
        {"10. Set power key long press time(ms) to shtudown systom", NULL, nwy_test_cli_pm_pwr_off},
        {NULL, NULL, NULL}};
#if 0
//1.7 Keypad
#ifdef NWY_OPEN_TEST_KEYPAD
static cli_menu_item keypad_menu_arry[] =
    {
        {"1. Register callback", NULL, nwy_test_cli_keypad_reg_cb},
        {"2. Set debouce time", NULL, nwy_test_cli_keypad_set_debouce},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const keypad_menu_arry = NULL;
#endif

//1.8 PWM
#ifdef NWY_OPEN_TEST_PWM
static cli_menu_item pwm_menu_arry[] =
    {
        {"1. Init", NULL, nwy_test_cli_pwm_init},
        {"2. Start", NULL, nwy_test_cli_pwm_start},
        {"3. Stop", NULL, nwy_test_cli_pwm_stop},
        {"4. Deinit", NULL, nwy_test_cli_pwm_deinit},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const pwm_menu_arry = NULL;
#endif

#endif
//1.9 RTC (no sub menu)
#ifndef NWY_OPEN_TEST_RTC
#define nwy_test_cli_rtc_read NULL
#endif
static cli_menu_item peripherals_menu_arry[] =
    {
        #if 0
        {"1. UART", uart_menu_arry, NULL},
        #endif
        {"2. I2C", i2c_menu_arry, NULL},
        {"3. SPI", spi_menu_arry, NULL},
        {"4. GPIO", gpio_menu_arry, NULL},
        {"5. ADC", NULL, nwy_test_cli_adc_read},
        {"6. PM", pm_menu_arry, NULL},
        #if 0
        {"7. Keypad", keypad_menu_arry, NULL},
        {"8. PWM", pwm_menu_arry, NULL},
        #endif
        {"9. RTC", NULL, nwy_test_cli_rtc_read},
        {NULL, NULL, NULL}};

//2. LCD
#ifdef NWY_OPEN_TEST_LCD
static cli_menu_item lcd_menu_arry[] =
    {
        {"1. Open lcd", NULL, nwy_test_cli_lcd_open},
        {"2. Close lcd", NULL, nwy_test_cli_lcd_close},
        {"3. Set backlight level", NULL, nwy_test_cli_lcd_set_bl_level},
        {NULL, NULL, NULL}
    };
#endif

#ifdef NWY_OPEN_TEST_CAMERA
static cli_menu_item camera_menu_arry[] =
    {
        {"1. Open camera", NULL, nwy_test_cli_camera_open},
        {"2. Close camera", NULL, nwy_test_cli_camera_close},
        {"3. get preview", NULL, nwy_test_cli_camera_get_preview},
        {"4. camera capture", NULL, nwy_test_cli_camera_capture},
        {NULL, NULL, NULL}
    };
#endif
//3. SD
#ifdef NWY_OPEN_TEST_SD
static cli_menu_item sd_menu_arry[] =
    {
        {"1 Mount", NULL, nwy_test_cli_sd_mnt},
        {"2 format sd", NULL, nwy_test_cli_sd_mkfs},
        {"3 Get status", NULL, nwy_test_cli_sd_get_st},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const sd_menu_arry = NULL;
#endif

#if 0
//4. Flash
#ifdef NWY_OPEN_TEST_FLASH
static cli_menu_item flash_menu_arry[] =
    {
        {"1 Open", NULL, nwy_test_cli_flash_open},
        {"2 Erase", NULL, nwy_test_cli_flash_erase},
        {"3 Write", NULL, nwy_test_cli_flash_write},
        {"4 Read", NULL, nwy_test_cli_flash_read},
        {NULL, NULL, NULL}};
#else
        static cli_menu_item * const flash_menu_arry = NULL;
#endif

//5. TTS
#ifdef FEATURE_NWY_OPEN_IVTTS
static cli_menu_item tts_menu_arry[] =
    {
        {"1. Input content", NULL, nwy_test_cli_tts_input},
        {"2. Start play", NULL, nwy_test_cli_tts_play_start},
        {"3. Stop play", NULL, nwy_test_cli_tts_play_stop},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const tts_menu_arry = NULL;
#endif
#endif
//6. fota
#ifdef NWY_OPEN_TEST_FOTA_NS
static cli_menu_item * const fota_menu_arry = NULL;
#else
static cli_menu_item fota_menu_arry[] =
    {
        #if 0
        {"1. Base version update", NULL, nwy_test_cli_fota_base_ver},
        #endif
        {"1. APP update", NULL, nwy_test_cli_fota_app_ver},
        {NULL, NULL, NULL}};
#endif

//7. Audio
#ifdef NWY_OPEN_TEST_AUDIO
static cli_menu_item audio_menu_arry[] =
    {
        {"1. Record Start", NULL, nwy_test_cli_audio_rec_start},
        {"2. Record Stop",  NULL, nwy_test_cli_audio_rec_stop},
        {"3. Record Test",  NULL, nwy_test_cli_audio_rec_selftest},
        {"4. Play   Start", NULL, nwy_test_cli_audio_play_start},
        {"5. Set Spk Vol",  NULL, nwy_test_cli_audio_set_speaker_vol},
        {"6. Get Spk Vol",  NULL, nwy_test_cli_audio_get_speaker_vol},
        {"7. Open/Close DTMF Detect",  NULL, nwy_test_cli_audio_dtmf_detect},
        {"8. Get DTMF Status",  NULL, nwy_test_cli_audio_dtmf_get_status},
        {"9. Play file", NULL, nwy_test_cli_audio_play_start_file},
        {"10. Record Start file", NULL, nwy_test_cli_audio_record_start_file},
        {"11. Record Stop file", NULL, nwy_test_cli_audio_record_stop_file},
        {"12. set caccp param", NULL, nwy_test_cli_audio_caccp_param},
        {"13. set cawtf", NULL, nwy_test_cli_audio_cawtf},
        {"14. Play pcm buffer", NULL, nwy_test_cli_audio_player_play},
        {"15. Set Ouput device", NULL, nwy_test_cli_audio_set_ouput_device},
        {NULL, NULL, NULL}
    };
#endif

//8. fs
static cli_menu_item fs_menu_arry[] =
    {
        {"1. Open file", NULL, nwy_test_cli_fs_open},
        {"2. Write file", NULL, nwy_test_cli_fs_write},
        {"3. Read file", NULL, nwy_test_cli_fs_read},
        {"4. Read file size", NULL, nwy_test_cli_fs_fsize},
        {"5. Seek file", NULL, nwy_test_cli_fs_seek},
        {"6. Sync file", NULL, nwy_test_cli_fs_sync},
        {"7. Get file state", NULL, nwy_test_cli_fs_fstate},
        {"8. Trunc file", NULL, nwy_test_cli_fs_trunc},
        {"9. Close file", NULL, nwy_test_cli_fs_close},
        {"10. Unlink/remove file", NULL, nwy_test_cli_fs_remove},
        {"11. Rename file", NULL, nwy_test_cli_fs_rename},
        {"12. Open dir", NULL, nwy_test_cli_dir_open},
        {"13. Read dir", NULL, nwy_test_cli_dir_read},
        {"14. Tell dir", NULL, nwy_test_cli_dir_tell},
        {"15. Seek dir", NULL, nwy_test_cli_dir_seek},
        {"16. Rewind dir", NULL, nwy_test_cli_dir_rewind},
        {"17. Close dir", NULL, nwy_test_cli_dir_close},
        {"18. Make dir", NULL, nwy_test_cli_dir_mk},
        {"19. Remove dir", NULL, nwy_test_cli_dir_remove},
        {"20. Get free size", NULL, nwy_test_cli_fs_free_size},
        {"21. Init safe file", NULL, nwy_test_cli_safe_fs_init},
        {"22. Read safe file", NULL, nwy_test_cli_safe_fs_read},
        {"23. Write safe file", NULL, nwy_test_cli_safe_fs_write},
        {"24. Get safe file size", NULL, nwy_test_cli_safe_fs_fszie},
        {NULL, NULL, NULL}};

static cli_menu_item driver_menu_arry[] =
    {

        {"1. Peripherals", peripherals_menu_arry, NULL},
        {"2. LCD", lcd_menu_arry, NULL},
        {"3. SD Card", sd_menu_arry, NULL},
        #if 0
        {"4. Flash", flash_menu_arry, NULL},
        {"5. TTS", tts_menu_arry, NULL},
        #endif
        {"4. FOTA", fota_menu_arry, NULL},
#ifdef NWY_OPEN_TEST_AUDIO
        {"5. Audio", audio_menu_arry, NULL},
#endif
        {"6. File system", fs_menu_arry, NULL},
#ifdef NWY_OPEN_TEST_CAMERA
        {"7. Camera ", camera_menu_arry, NULL},
#endif
        {NULL, NULL, NULL}};
/*******************************************************************************
                        3. Short Distance Wireless
********************************************************************************/
#if 0
//1. BLE SERVICE
#ifdef NWY_OPEN_TEST_BLE_SERV
static cli_menu_item ble_service_menu_arry[] =
    {
        {"1. BLE open", NULL, nwy_test_cli_ble_open},
        {"2. BLE adv set", NULL, nwy_test_cli_ble_set_adv},
        {"3. BLE send data", NULL, nwy_test_cli_ble_send},
        {"4. BLE receive data", NULL, nwy_test_cli_ble_recv},
        {"5. BLE update connect", NULL, nwy_test_cli_ble_updata_connt},
        {"6. BLE get status", NULL, nwy_test_cli_ble_get_st},
        {"7. BLE get version", NULL, nwy_test_cli_ble_get_ver},
        {"8. BLE set device name", NULL, nwy_test_cli_ble_set_dev_name},
        {"9. BLE set beacon", NULL, nwy_test_cli_ble_set_beacon},
        {"10. BLE set manufacture", NULL, nwy_test_cli_ble_set_manufacture},
        {"11. BLE set adv server uuid", NULL, nwy_test_cli_ble_set_adv_server_uuid},
        {"12. BLE set service", NULL, nwy_test_cli_ble_set_srv},
        {"13. BLE set characteristic", NULL, nwy_test_cli_ble_set_char},
        {"14. BLE conn status(auto report)", NULL, nwy_test_cli_ble_conn_status_report},
        {"15. BLE conn status(manual get)", NULL, nwy_test_cli_ble_conn_status},
        {"16. BLE mac addr", NULL, nwy_test_cli_ble_mac_addr},
        {"17. BLE add server", NULL, nwy_test_cli_ble_add_server},
        {"18. BLE add charcter", NULL, nwy_test_cli_ble_add_char},
        {"19. BLE add send data", NULL, nwy_test_cli_ble_add_send_data},
        {"20. BLE add recv data", NULL, nwy_test_cli_ble_add_recv_data},
        {"21. BLE read req", NULL, nwy_test_cli_ble_read_req},
        {"22. BLE set read rsp", NULL, nwy_test_cli_ble_read_rsp},
        {"23. BLE disconnet", NULL, nwy_test_cli_ble_disconnect},
        {"24. BLE close", NULL, nwy_test_cli_ble_close},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const ble_service_menu_arry = NULL;
#endif

//2. BLE CLIENT
#ifdef NWY_OPEN_TEST_BLE_CLI
static cli_menu_item ble_client_menu_arry[] =
    {
        {"1. BLE client enable set", NULL, nwy_test_cli_ble_client_set_enable},
        {"2. BLE client scan dev", NULL, nwy_test_cli_ble_client_scan},
        {"3. BLE client connect", NULL, nwy_test_cli_ble_client_connect},
        {"4. BLE client disconnect", NULL, nwy_test_cli_ble_client_disconnect},
        {"5. BLE client discovery srv", NULL, nwy_test_cli_ble_client_discover_srv},
        {"6. BLE client discovery char", NULL, nwy_test_cli_ble_client_discover_char},
        {"7. BLE client send data", NULL, nwy_test_cli_ble_client_send_data},
        {"8. BLE client recv data", NULL, nwy_test_cli_ble_client_recv_data},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const ble_client_menu_arry = NULL;
#endif

//2. WIFI
#ifdef NWY_OPEN_TEST_WIFI
static cli_menu_item wifi_menu_arry[] =
    {
        {"1. get wifi current status", NULL, nwy_cli_test_wifi_get_st},
        {"2. enable wifi", NULL, nwy_cli_test_wifi_enable},
        {"3. set wifi work mode", NULL, nwy_cli_test_wifi_set_work_md},
        {"4. set wifi ap basic params", NULL, nwy_cli_test_wifi_set_ap_para},
        {"5. set wifi ap advance params", NULL, nwy_cli_test_wifi_set_ap_para_adv},
        {"6. get wifi ap connected client info", NULL, nwy_cli_test_wifi_get_clit_info},
        {"7. wifi sta start scan", NULL, nwy_cli_test_wifi_sta_scan},
        {"8. wifi sta get scan results", NULL, nwy_cli_test_wifi_sta_scan_ret},
        {"9. wifi sta connect external hotspot", NULL, nwy_cli_test_wifi_sta_connt},
        {"10. wifi sta disconnect", NULL, nwy_cli_test_wifi_sta_disconnt},
        {"11. get wifi sta connected external hostpot info", NULL, nwy_cli_test_wifi_sta_get_hostpot_info},
        {"12. disable wifi", NULL, nwy_cli_test_wifi_disable},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const wifi_menu_arry = NULL;
#endif

static cli_menu_item sdw_menu_arry[] =
    {
        {"1. BLE service", ble_service_menu_arry, NULL},
        {"2. BLE client", ble_client_menu_arry, NULL},
        {"3. WIFI", wifi_menu_arry, NULL},
        {NULL, NULL, NULL}};
#endif

/*******************************************************************************
                        4. LBS
********************************************************************************/
//1. GNSS
#ifdef NWY_OPEN_TEST_GNSS
static cli_menu_item gnss_menu_arry[] =
    {
    #if 0
        {"1. Open location position", NULL, nwy_test_cli_gnss_open},
        {"2. Set position mode", NULL, nwy_test_cli_gnss_set_position_md},
        {"3. Set output format", NULL, nwy_test_cli_gnss_output_format},
        {"4. Delete aiding data", NULL,  nwy_test_cli_gnss_delete_aiding_data},
        {"5. Close location position", NULL, nwy_test_cli_gnss_close},
        {"6. Location and satellite info", NULL, nwy_test_cli_gnss_nmea_info_parse},
        {"7. Get raw nmea data", NULL, nwy_test_cli_gnss_nmea_data},
        {"8. Set location server", NULL, nwy_test_cli_gnss_set_server},
    #endif
        {"9. Open location base position", NULL, nwy_test_cli_gnss_open_base},
        {"10. Open assisted location", NULL, nwy_test_cli_gnss_open_assisted},
        {"11.Open wifi loc position", NULL, nwy_test_cli_wifi_open_base},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const gnss_menu_arry = NULL;
#endif

static cli_menu_item lbs_menu_arry[] =
    {
        {"1. GNSS", gnss_menu_arry, NULL},
        {NULL, NULL, NULL}};


/*******************************************************************************
                        5. TCP/IP Protocol
********************************************************************************/
//1. TCP Client
static cli_menu_item tcp_cli_menu_arry[] =
    {
        {"1. TCP setup", NULL, nwy_test_cli_tcp_setup},
        {"2. TCP send", NULL, nwy_test_cli_tcp_send},
        {"3. TCP close", NULL, nwy_test_cli_tcp_close},
        {"4. PING test", NULL, nwy_test_cli_ping},
        {NULL, NULL, NULL}};
//2. UDP Client
static cli_menu_item udp_cli_menu_arry[] =
    {
        {"1. UDP setup", NULL, nwy_test_cli_udp_setup},
        {"2. UDP send", NULL, nwy_test_cli_udp_send},
        {"3. UDP close", NULL, nwy_test_cli_udp_close},
        {NULL, NULL, NULL}};
//3. FTP
#ifndef NWY_OPEN_TEST_FTP_NS
        static cli_menu_item ftp_cli_menu_arry[] =
        {
            {"1. Login", NULL, nwy_test_cli_ftp_login},
            {"2. Get file", NULL, nwy_test_cli_ftp_get},
            {"3. Put file", NULL, nwy_test_cli_ftp_put},
            {"4. Get file size", NULL, nwy_test_cli_ftp_fsize},
            {"5. Get file list", NULL, nwy_test_cli_ftp_list},
            {"6. Delete file", NULL, nwy_test_cli_ftp_delet},
            {"7. Logout", NULL, nwy_test_cli_ftp_logout},
            {NULL, NULL, NULL}};
 #else
        static cli_menu_item * const ftp_cli_menu_arry = NULL;
#endif
        //4. Http
#ifndef NWY_OPEN_TEST_HTTP_NS
        static cli_menu_item http_cli_menu_arry[] =
        {
            {"1. Setup", NULL, nwy_test_cli_http_setup},
            {"2. Get", NULL, nwy_test_cli_http_get},
            {"3. Head", NULL, nwy_test_cli_http_head},
            {"4. Post", NULL, nwy_test_cli_http_post},
            {"5. Close", NULL, nwy_test_cli_http_close},
            {"6. Add cert", NULL, nwy_test_cli_https_add_cert},
            {"7. Check cert", NULL, nwy_test_cli_https_check_cert},
            {"8. Del cert", NULL, nwy_test_cli_https_delet_cert},
            {"9. ntp test", NULL, nwy_test_cli_ntp_time},
            {NULL, NULL, NULL}};
#else
        static cli_menu_item * const http_cli_menu_arry = NULL;
#endif

        static cli_menu_item tcpip_menu_arry[] =
        {
            {"1. TCP Client", tcp_cli_menu_arry, NULL},
            {"2. UDP Client", udp_cli_menu_arry, NULL},
            #ifndef NWY_OPEN_TEST_FTP_NS
            {"3. FTP Client", ftp_cli_menu_arry, NULL},
            #endif
            #ifndef NWY_OPEN_TEST_HTTP_NS
            {"3. HTTP/HTTPS Client", http_cli_menu_arry, NULL},
            #endif
            {NULL, NULL, NULL}};


/*******************************************************************************
                        6. IoT Protocol
********************************************************************************/
#if 0
//1. Ali MQTT
#ifdef NWY_OPEN_TEST_ALI_MQTT
static cli_menu_item ali_mqtt_menu_arry[] =
    {
        {"1. Ali-MQTT connect", NULL, nwy_test_cli_alimqtt_connect},
        {"2. Ali-MQTT pub", NULL, nwy_test_cli_alimqtt_pub},
        {"3. Ali-MQTT sub", NULL, nwy_test_cli_alimqtt_sub},
        {"4. Ali-MQTT unsub", NULL, nwy_test_cli_alimqtt_unsub},
        {"5. Ali-MQTT state", NULL, nwy_test_cli_alimqtt_state},
        {"6. Ali-MQTT disconnect", NULL, nwy_test_cli_alimqtt_disconnect},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const ali_mqtt_menu_arry = NULL;
#endif
#endif
//2. Paho MQTT
#if (defined(FEATURE_NWY_PAHO_MQTT_V5)) || (defined(FEATURE_NWY_PAHO_MQTT_V3))
static cli_menu_item paho_mqtt_menu_arry[] =
    {
        {"1. MQTT connect", NULL, nwy_test_cli_mqtt_connect},
        {"2. MQTT pub", NULL, nwy_test_cli_mqtt_pub},
        {"3. MQTT sub", NULL, nwy_test_cli_mqtt_sub},
        {"4. MQTT unsub", NULL, nwy_test_cli_mqtt_unsub},
        {"5. MQTT state", NULL, nwy_test_cli_mqtt_state},
        {"6. MQTT disconnect", NULL, nwy_test_cli_mqtt_disconnect},
#ifdef FEATURE_NWY_N58_OPEN_NIPPON
        {"7. MQTT 10k msg pub", NULL, nwy_test_cli_mqtt_pub_test},
#endif
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const paho_mqtt_menu_arry = NULL;
#endif

static cli_menu_item iot_menu_arry[] =
    {
        //{"1. Ali-MQTT", ali_mqtt_menu_arry, NULL},
        #if (defined(FEATURE_NWY_PAHO_MQTT_V5)) || (defined(FEATURE_NWY_PAHO_MQTT_V3))
        {"1. Paho MQTT", paho_mqtt_menu_arry, NULL},
        #endif
        {NULL, NULL, NULL}};

/*******************************************************************************
                        7. OS & Device
********************************************************************************/
//1. AT
#ifdef NWY_OPEN_TEST_VIRT_AT
static cli_menu_item virtual_at_menu_arry[] =
    {
        {"1. Send AT", NULL, nwy_test_cli_send_virt_at},
        {"2. Regist forward AT", NULL, nwy_test_cli_reg_at_fwd},
        {NULL, NULL, NULL}};
#else
static cli_menu_item * const virtual_at_menu_arry = NULL;

#endif

//2. dev info
static cli_menu_item dev_info_menu_arry[] =
    {
        {"1. Get model", NULL, nwy_test_cli_get_model},
        {"2. Get SW version", NULL, nwy_test_cli_get_sw_ver},
        {"3. Get HW version", NULL, nwy_test_cli_get_hw_ver},
        {"4. Get IMEI", NULL, nwy_test_cli_get_imei},
        {"5. Get heap info", NULL, nwy_test_cli_get_heap_info},
        {"6. Get CPU temperature", NULL, nwy_test_cli_get_cpu_temp},
        {"7. Get boot cause", NULL, nwy_test_cli_get_boot_cause},
        {"8. Get chip ID", NULL, nwy_test_cli_get_chipid},
        {"9. Set APP Version", NULL, nwy_test_cli_set_app_version},

        {NULL, NULL, NULL}};
//3. OS
static cli_menu_item os_api_menu_arry[] =
    {
        {"1 Start timer", NULL, nwy_test_cli_start_timer},
        {"2 Stop timer", NULL, nwy_test_cli_stop_timer},
        {"3 Get sys time", NULL, nwy_test_cli_get_time},
        {"4 Set sys time", NULL, nwy_test_cli_set_time},
        {"5 Semaphore", NULL, nwy_test_cli_set_semp},
        {NULL, NULL, NULL}};

static cli_menu_item dev_os_menu_arry[] =
    {
        {"1. Virtual AT", virtual_at_menu_arry, NULL},

        {"2. Device info", dev_info_menu_arry, NULL},
        {"3. OS API", os_api_menu_arry, NULL},
        {NULL, NULL, NULL}};

/*******************************************************************************
                          MAIN
********************************************************************************/
static cli_menu_item mian_menu_arry[] =
    {
        {"1. Mobile Network", mobile_menu_arry, NULL},
        {"2. Driver & Kernel", driver_menu_arry, NULL},
    #if 0
        {"3. Short Distance Wireless", sdw_menu_arry, NULL},
#endif
        {"3. LBS", lbs_menu_arry, NULL},
        {"4. TCP/IP Protocol", tcpip_menu_arry, NULL},
        {"5. IoT Protocol", iot_menu_arry, NULL},
        {"6. OS & Device", dev_os_menu_arry, NULL},
        {NULL, NULL, NULL}};

static cli_menu_item *menu_stack[TEST_CLI_MENU_STACK_MAX] =
    {mian_menu_arry, NULL, NULL, NULL, NULL};
static int curt_menu_stack_id = 0;

void nwy_test_cli_menu_display()
{
    int i;
    cli_menu_item *disp_menu = menu_stack[curt_menu_stack_id];

    nwy_test_cli_echo("\r\n");
    for (i = 0;; i++)
    {
        if (disp_menu[i].name_str == NULL)
        {
            break;
        }
        nwy_test_cli_echo("%s\r\n", disp_menu[i].name_str);
    }
}

void nwy_test_cli_menu_select(int opt)
{
    cli_menu_item *curt_menu = menu_stack[curt_menu_stack_id];
    int i;

    if (opt < 1)
    {
        nwy_test_cli_echo("\r\nInvalid option\r\n");
        return;
    }

    for (i = 1;; i++)
    {
        if (curt_menu[i - 1].name_str == NULL)
        {
            break;
        }
    }

    if (opt >= i)
    {
        nwy_test_cli_echo("\r\nInvalid option\r\n");
        return;
    }

    opt--;
    if (curt_menu[opt].sub_menu != NULL && curt_menu[opt].sub_menu[0].name_str != NULL)
    {
        curt_menu_stack_id++;
        menu_stack[curt_menu_stack_id] = curt_menu[opt].sub_menu;
    }
    else if (curt_menu[opt].func != NULL)
    {
        curt_menu[opt].func();
    }
    else
    {
        nwy_test_cli_echo("\r\nOption not Supported!\r\n");
    }
}

void nwy_test_cli_menu_back()
{
    if (curt_menu_stack_id != 0)
    {
        menu_stack[curt_menu_stack_id] = NULL;
        curt_menu_stack_id--;
    }
}
