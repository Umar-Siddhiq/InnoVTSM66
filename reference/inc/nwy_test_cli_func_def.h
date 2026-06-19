#ifndef __NWY_TEST_CLI_FUNC_DEF_H__
#define __NWY_TEST_CLI_FUNC_DEF_H__

/**************************SIM*********************************/
void nwy_test_cli_get_sim_status(void);
void nwy_test_cli_verify_pin(void);
void nwy_test_cli_get_pin_mode(void);
void nwy_test_cli_set_pin_mode(void);
void nwy_test_cli_change_pin(void);
void nwy_test_cli_verify_puk(void);
void nwy_test_cli_get_imsi(void);
void nwy_test_cli_get_iccid(void);
void nwy_test_cli_get_msisdn(void);
void nwy_test_cli_set_msisdn(void);
void nwy_test_cli_get_pin_puk_times(void);
void nwy_test_cli_get_sim_slot(void);
void nwy_test_cli_set_sim_slot(void);
void nwy_test_cli_set_csim(void);
/**************************DATA*********************************/
void nwy_test_cli_data_create(void);
void nwy_test_cli_get_profile(void);
void nwy_test_cli_set_profile(void);
void nwy_test_cli_data_start(void);
void nwy_test_cli_data_info(void);
void nwy_test_cli_data_stop(void);
void nwy_test_cli_data_release(void);
void nwy_test_cli_data_get_flowcalc(void);

/**************************NW*********************************/
void nwy_test_cli_nw_get_mode(void);
void nwy_test_cli_nw_set_mode(void);
void nwy_test_cli_nw_get_fplmn(void);
void nwy_test_cli_nw_manual_scan(void);
void nwy_test_cli_nw_band_lock(void);
void nwy_test_cli_nw_freq_lock(void);
void nwy_test_cli_nw_get_ims_st(void);
void nwy_test_cli_nw_set_ims_st(void);
void nwy_test_cli_nw_get_def_pdn(void);
void nwy_test_cli_nw_set_def_pdn(void);
void nwy_test_cli_nw_get_radio_st(void);
void nwy_test_cli_nw_set_radio_st(void);
void nwy_test_cli_nw_get_radio_sign(void);
void nwy_test_cli_nw_cs_st(void);
void nwy_test_cli_nw_ps_st(void);
void nwy_test_cli_nw_lte_st(void);
void nwy_test_cli_nw_operator_info(void);
void nwy_test_cli_nw_get_ehplmn(void);
void nwy_test_cli_nw_get_signal_rssi(void);
void nwy_test_cli_nw_get_netmsg(void);
void nwy_test_cli_nw_get_cfgdftpdn_info(void);
void nwy_test_cli_nw_set_cfgdftpdn_info(void);
void nwy_test_cli_nw_get_cellmsg(void);

/**************************VOICE*********************************/
void nwy_test_cli_voice_call_start(void);
void nwy_test_cli_voice_call_end(void);
void nwy_test_cli_voice_auto_answ(void);
void nwy_test_cli_voice_volte_set(void);
void nwy_test_cli_voice_caller_id(void);
void nwy_test_cli_voice_call_hold(void);
void nwy_test_cli_voice_call_unhold(void);

/**************************SMS*********************************/
void nwy_test_cli_sms_send(void);
void nwy_test_cli_sms_del(void);
void nwy_test_cli_sms_get_sca(void);
void nwy_test_cli_sms_set_sca(void);
void nwy_test_cli_sms_set_storage(void);
void nwy_test_cli_sms_get_storage(void);
void nwy_test_cli_sms_set_report_md(void);
void nwy_test_cli_sms_read(void);
void nwy_test_cli_sms_list(void);
void nwy_test_cli_sms_del_type(void);

/**************************UART*********************************/
void nwy_test_cli_uart_init(void);
void nwy_test_cli_uart_set_baud(void);
void nwy_test_cli_uart_get_baud(void);
void nwy_test_cli_uart_set_para(void);
void nwy_test_cli_uart_get_para(void);
void nwy_test_cli_uart_set_tout(void);
void nwy_test_cli_uart_send(void);
void nwy_test_cli_uart_reg_rx_cb(void);
void nwy_test_cli_uart_reg_tx_cb(void);
void nwy_test_cli_uart_deinit(void);

/**************************I2C*********************************/
void nwy_test_cli_i2c_init(void);
void nwy_test_cli_i2c_read(void);
void nwy_test_cli_i2c_write(void);
void nwy_test_cli_i2c_put_raw(void);
void nwy_test_cli_i2c_get_raw(void);
void nwy_test_cli_i2c_deinit(void);

/**************************SPI*********************************/
void nwy_test_cli_spi_init(void);
void nwy_test_cli_spi_trans(void);
void nwy_test_cli_spi_deinit(void);
void nwy_test_cli_spi_flash_mount(void);

/**************************GPIO*********************************/
void nwy_test_cli_gpio_set_val(void);
void nwy_test_cli_gpio_get_val(void);
void nwy_test_cli_gpio_set_dirt(void);
void nwy_test_cli_gpio_get_dirt(void);
void nwy_test_cli_gpio_config_irq(void);
void nwy_test_cli_gpio_enable_irq(void);
void nwy_test_cli_gpio_disable_irq(void);
void nwy_test_cli_gpio_close(void);

/**************************ADC*********************************/
void nwy_test_cli_adc_read(void);

/**************************PM*********************************/
void nwy_test_cli_pm_save_md(void);
void nwy_test_cli_pm_get_pwr_st(void);
void nwy_test_cli_pm_pwr_off(void);
void nwy_test_cli_pm_pwrkey_long_press_time_pwr_off();
void nwy_test_cli_pm_set_dtr(void);
void nwy_test_cli_pm_pwr_key(void);
void nwy_test_cli_pm_switch_sub_pwr(void);
void nwy_test_cli_pm_set_sub_pwr(void);
void nwy_test_cli_pm_set_auto_off(void);
void nwy_test_cli_pm_reg_charger_cb(void);

/**************************KEYPAD*********************************/
void nwy_test_cli_keypad_reg_cb(void);
void nwy_test_cli_keypad_set_debouce(void);

/**************************PWM*********************************/
void nwy_test_cli_pwm_init(void);
void nwy_test_cli_pwm_start(void);
void nwy_test_cli_pwm_stop(void);
void nwy_test_cli_pwm_deinit(void);

/**************************RTC*********************************/
void nwy_test_cli_rtc_read(void);

/**************************LCD*********************************/
void nwy_test_cli_lcd_open(void);
void nwy_test_cli_lcd_close(void);
void nwy_test_cli_lcd_set_bl_level(void);

/**************************CAMERA*********************************/
void nwy_test_cli_camera_open(void);
void nwy_test_cli_camera_close(void);
void nwy_test_cli_camera_get_preview(void);
void nwy_test_cli_camera_capture(void);
/**************************SD*********************************/
void nwy_test_cli_sd_get_st(void);
void nwy_test_cli_sd_mnt(void);
void nwy_test_cli_sd_unmnt(void);
void nwy_test_cli_sd_mkfs(void);

/**************************FLASH*********************************/
void nwy_test_cli_flash_open(void);
void nwy_test_cli_flash_erase(void);
void nwy_test_cli_flash_write(void);
void nwy_test_cli_flash_read(void);

/**************************TTS*********************************/
void nwy_test_cli_tts_input(void);
void nwy_test_cli_tts_play_start(void);
void nwy_test_cli_tts_play_stop(void);

/**************************FOTA*********************************/
void nwy_test_cli_fota_base_ver(void);
void nwy_test_cli_fota_app_ver(void);

/**************************AUDIO*********************************/
void nwy_test_cli_audio_rec_start(void);
void nwy_test_cli_audio_rec_stop(void);
void nwy_test_cli_audio_rec_selftest(void);
void nwy_test_cli_audio_play_start(void);
void nwy_test_cli_audio_play_stop(void);
void nwy_test_cli_audio_set_speaker_vol(void);
void nwy_test_cli_audio_get_speaker_vol(void);
void nwy_test_cli_audio_dtmf_detect(void);
void nwy_test_cli_audio_dtmf_get_status(void);
void nwy_test_cli_audio_play_start_file(void);
void nwy_test_cli_audio_record_start_file(void);
void nwy_test_cli_audio_record_stop_file(void);
void nwy_test_cli_audio_caccp_param(void);
void nwy_test_cli_audio_cawtf(void);
void nwy_test_cli_audio_player_play(void);
void nwy_test_cli_audio_set_ouput_device(void);

/**************************FS*********************************/
void nwy_test_cli_fs_open(void);
void nwy_test_cli_fs_write(void);
void nwy_test_cli_fs_read(void);
void nwy_test_cli_fs_fsize(void);
void nwy_test_cli_fs_seek(void);
void nwy_test_cli_fs_sync(void);
void nwy_test_cli_fs_fstate(void);
void nwy_test_cli_fs_trunc(void);
void nwy_test_cli_fs_close(void);
void nwy_test_cli_fs_remove(void);
void nwy_test_cli_fs_rename(void);
void nwy_test_cli_dir_open(void);
void nwy_test_cli_dir_read(void);
void nwy_test_cli_dir_tell(void);
void nwy_test_cli_dir_seek(void);
void nwy_test_cli_dir_rewind(void);
void nwy_test_cli_dir_close(void);
void nwy_test_cli_dir_mk(void);
void nwy_test_cli_dir_remove(void);
void nwy_test_cli_fs_free_size(void);
void nwy_test_cli_safe_fs_init(void);
void nwy_test_cli_safe_fs_read(void);
void nwy_test_cli_safe_fs_write(void);
void nwy_test_cli_safe_fs_fszie(void);

/**************************BLE*********************************/
void nwy_test_cli_ble_open(void);
void nwy_test_cli_ble_set_adv(void);
void nwy_test_cli_ble_send(void);
void nwy_test_cli_ble_recv(void);
void nwy_test_cli_ble_updata_connt(void);
void nwy_test_cli_ble_get_st(void);
void nwy_test_cli_ble_get_ver(void);
void nwy_test_cli_ble_set_dev_name(void);
void nwy_test_cli_ble_close(void);
void nwy_test_cli_ble_set_beacon(void);
void nwy_test_cli_ble_set_manufacture(void);
void nwy_test_cli_ble_set_srv(void);
void nwy_test_cli_ble_set_char(void);
void nwy_test_cli_ble_conn_status_report(void);
void nwy_test_cli_ble_conn_status(void);
void nwy_test_cli_ble_mac_addr(void);
void nwy_test_cli_ble_add_server(void);
void nwy_test_cli_ble_add_char(void);
void nwy_test_cli_ble_add_send_data(void);
void nwy_test_cli_ble_add_recv_data(void);
void nwy_test_cli_ble_disconnect(void);
void nwy_test_cli_ble_read_req(void);
void nwy_test_cli_ble_set_adv_server_uuid(void);
void nwy_test_cli_ble_read_rsp(void);

/**************************BLE Client*********************************/
void nwy_test_cli_ble_client_set_enable(void);
void nwy_test_cli_ble_client_scan(void);
void nwy_test_cli_ble_client_connect(void);
void nwy_test_cli_ble_client_disconnect(void);
void nwy_test_cli_ble_client_discover_srv(void);
void nwy_test_cli_ble_client_discover_char(void);
void nwy_test_cli_ble_client_send_data(void);
void nwy_test_cli_ble_client_recv_data(void);

/**************************WIFI*********************************/
void nwy_cli_test_wifi_get_st(void);
void nwy_cli_test_wifi_enable(void);
void nwy_cli_test_wifi_set_work_md(void);
void nwy_cli_test_wifi_set_ap_para(void);
void nwy_cli_test_wifi_set_ap_para_adv(void);
void nwy_cli_test_wifi_get_clit_info(void);
void nwy_cli_test_wifi_sta_scan(void);
void nwy_cli_test_wifi_sta_scan_ret(void);
void nwy_cli_test_wifi_sta_connt(void);
void nwy_cli_test_wifi_sta_disconnt(void);
void nwy_cli_test_wifi_sta_get_hostpot_info(void);
void nwy_cli_test_wifi_disable(void);

/**************************GNSS*********************************/
void nwy_test_cli_gnss_open(void);	
void nwy_test_cli_gnss_set_position_md(void);
void nwy_test_cli_gnss_output_format(void);
void nwy_test_cli_gnss_set_output_fmt(void);
void nwy_test_cli_gnss_delete_aiding_data(void);
void nwy_test_cli_gnss_nmea_info_parse(void);
void nwy_test_cli_gnss_nmea_data(void);
void nwy_test_cli_gnss_set_server(void);
void nwy_test_cli_gnss_open_base(void);
void nwy_test_cli_wifi_open_base(void);
void nwy_test_cli_gnss_open_assisted(void);
void nwy_test_cli_gnss_close(void);

/**************************TCP*********************************/
void nwy_test_cli_tcp_setup(void);
void nwy_test_cli_tcp_send(void);
void nwy_test_cli_tcp_close(void);

/**************************UDP*********************************/
void nwy_test_cli_udp_setup(void);
void nwy_test_cli_udp_send(void);
void nwy_test_cli_udp_close(void);

void nwy_test_cli_ping();
/**************************FTP*********************************/
void nwy_test_cli_ftp_login(void);
void nwy_test_cli_ftp_get(void);
void nwy_test_cli_ftp_put(void);
void nwy_test_cli_ftp_fsize(void);
void nwy_test_cli_ftp_list(void);
void nwy_test_cli_ftp_delet(void);
void nwy_test_cli_ftp_logout(void);

/**************************HTTP*********************************/
void nwy_test_cli_http_setup(void);
void nwy_test_cli_http_get(void);
void nwy_test_cli_http_head(void);
void nwy_test_cli_http_post(void);
void nwy_test_cli_http_close(void);
void nwy_test_cli_https_add_cert(void);
void nwy_test_cli_https_check_cert(void);
void nwy_test_cli_https_delet_cert(void);
void nwy_test_cli_ntp_time();

/**************************ALI MQTT*********************************/
void nwy_test_cli_alimqtt_connect(void);
void nwy_test_cli_alimqtt_pub(void);
void nwy_test_cli_alimqtt_sub(void);
void nwy_test_cli_alimqtt_unsub(void);
void nwy_test_cli_alimqtt_state(void);
void nwy_test_cli_alimqtt_disconnect(void);

/**************************MQTT*********************************/
void nwy_test_cli_mqtt_connect(void);
void nwy_test_cli_mqtt_pub(void);
void nwy_test_cli_mqtt_sub(void);
void nwy_test_cli_mqtt_unsub(void);
void nwy_test_cli_mqtt_state(void);
void nwy_test_cli_mqtt_disconnect(void);
void nwy_test_cli_mqtt_pub_test(void);

/**************************OS & Dev API************************/
void nwy_test_cli_get_model(void);
void nwy_test_cli_get_imei(void);
void nwy_test_cli_get_chipid(void);
void nwy_test_cli_get_boot_cause(void);
void nwy_test_cli_get_sw_ver(void);
void nwy_test_cli_get_hw_ver(void);
void nwy_test_cli_get_heap_info(void);
void nwy_test_cli_get_cpu_temp(void);
void nwy_test_cli_set_app_version(void);

void nwy_test_cli_start_timer(void);
void nwy_test_cli_stop_timer(void);
void nwy_test_cli_get_time(void);
void nwy_test_cli_set_time(void);
void nwy_test_cli_set_semp(void);

void nwy_test_cli_send_virt_at(void);
void nwy_test_cli_reg_at_fwd(void);
#endif
