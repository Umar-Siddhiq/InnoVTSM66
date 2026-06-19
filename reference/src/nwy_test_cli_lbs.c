#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_adpt.h"
#include "nwy_loc.h"

#ifdef NWY_OPEN_TEST_GNSS
#ifndef NWY_OPEN_TEST_LOC_NS
/**************************GNSS*********************************/
void test_loc_event_cb(NWY_GNSS_OUTPUT_INFO_IND_T * ind_msg)
{
     nwy_test_cli_echo("%s\n",ind_msg->nmea_data);
}

void nwy_test_cli_gnss_open()
{
    int ret = 0;
	char *opt = NULL;
    uint8_t startmode = 1;

    opt = nwy_test_cli_input_gets("\r\nPlease input startmode(1-cold star 0-warm star):");
    startmode = atoi(opt);
    nwy_test_cli_echo("\r\nTest startmode = %d!\r\n", startmode);

	ret = nwy_loc_start_navigation(startmode);

    if (ret == 0)
        nwy_test_cli_echo("\r\nTest loc open success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc open error!\r\n");
}

void nwy_test_cli_gnss_set_position_md()
{
    char *opt = NULL;
    uint8_t nSatType = 0;
    int ret = 0;

    opt = nwy_test_cli_input_gets("\r\nPlease input loc nSatType(1,2,3,4,5,8,9,13,16,17,18,19,20,21,24,25,29):");
    nSatType = atoi(opt);
    nwy_test_cli_echo("\r\nTest nSatType = %d!\r\n", nSatType);

    ret = nwy_loc_set_position_mode(nSatType);
    if (ret == 0)
        nwy_test_cli_echo("\r\nTest loc set position success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc set position error!\r\n");
}

void nwy_test_cli_gnss_output_format()
{
    char *opt = NULL;
	uint16_t nOutputFormat = 0;
	uint32_t nTimeInterval = 0;
    int ret = 0;

    opt = nwy_test_cli_input_gets("\r\nPlease input loc nmea format(0-8):");
    nOutputFormat = atoi(opt);
    opt = nwy_test_cli_input_gets("\r\nPlease input loc nTimeInterval(1-32768):");
    nTimeInterval = atoi(opt);

	nwy_test_cli_echo("\r\nTest nOutputFormat = %d,nTimeInterval = %d!\r\n", nOutputFormat,nTimeInterval);
    ret = nwy_loc_output_format(nOutputFormat,nTimeInterval);
    if (ret == 0)
        nwy_test_cli_echo("\r\nTest loc set output format success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc set output format error!\r\n");
}

void nwy_test_cli_gnss_delete_aiding_data()
{
    int ret = 0;

    ret = nwy_loc_delete_aiding_data();
    if (ret == 0)
        nwy_test_cli_echo("\r\nTest loc delete aiding data success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc delete aiding data error!\r\n");
}

void nwy_test_cli_gnss_nmea_info_parse()
{
    int ret = 0;
	char *opt = NULL;
    uint8_t nmode = 0;
    char data_title[32];
    char data[100];
	uint8_t n = 0;
    NWY_GNSS_READ_INFO_CNF_T *gnssData = (NWY_GNSS_READ_INFO_CNF_T *)malloc(sizeof(NWY_GNSS_READ_INFO_CNF_T));
	memset(gnssData,0,sizeof(NWY_GNSS_READ_INFO_CNF_T));

	opt = nwy_test_cli_input_gets("\r\nPlease input nmode(0-All Information 1-Positioning Information):");
	nmode = atoi(opt);

    nwy_test_cli_echo("\r\nTest nSatType = %d!\r\n", nmode);
	ret = nwy_loc_nmea_Info_parse(gnssData);
    if (ret == 0)
		nwy_test_cli_echo("\r\nTest loc nmea info parse success!\r\n");
	else
		nwy_test_cli_echo("\r\nTest loc nmea info parse error!\r\n");

	if(ret == 0)
	{
		char *data_title_temp = data_title;
		sprintf(data_title_temp, "GNSS Positioning Information:");
		nwy_test_cli_echo("\r\n %s\r\n", data_title);
		if (nmode == 0)
		{
			NWY_GNSS_INFO_DATA_T *gnssInfo = &(gnssData->gnss_data_info);
			char *data_temp1 = data;
			sprintf(data_temp1, "Longitude:%f, Latitude:%f, Altiude:%f", gnssInfo->longitude, gnssInfo->latitude, gnssInfo->altitude);
			nwy_test_cli_echo("\r\n %s\r\n", data);

			memset(data, 0, 100);
			NwyGnssTimeStamp *utc_time = &(gnssData->gnss_data_info.utc_time);
			char *data_temp2 = data;
			sprintf(data_temp2, "UTC Time: %04d/%02d/%02d  %02d:%02d:%02d", utc_time->wYear, utc_time->wMonth, utc_time->wDay, utc_time->wHour, utc_time->wMinute, utc_time->wSecond);
			nwy_test_cli_echo("\r\n %s\r\n", data);

			memset(data, 0, 100);
			char *data_temp3 = data;
			sprintf(data_temp3, "TTFF:%lu, SatelliteNum:%d, Speed:%f, Course:%f, HorizontalAccuracy:%f", gnssInfo->ttff_time, gnssInfo->satellite_num, gnssInfo->speed, gnssInfo->course, gnssInfo->m_nHorizontalAccuracy);
			nwy_test_cli_echo("\r\n %s\r\n", data);

			memset(data_title, 0, 32);
			char *satellite_title = data_title;
			sprintf(satellite_title, "Satellite Information:");
			nwy_test_cli_echo("\r\n %s\r\n", data_title);

			char *data_temp4 = data;
			NWY_GNSS_SATELLITE_INFO_T *satellite_info = gnssData->satellite_list_info.satellite_info;
			for (uint8_t n = 0; n < gnssInfo->satellite_num; n++)
			{
				sprintf(data_temp4, "PRN:%d, CN0:%d, Elevation:%d, Azimuth:%d, Used:%d", satellite_info[n].m_nSatelliteIdentifier, satellite_info[n].m_cn0, satellite_info[n].m_nElevation, satellite_info[n].m_nAzimuth, satellite_info[n].m_IsUsed);
				nwy_test_cli_echo("\r\n %s\r\n", data);
				memset(data, 0, 100);
			}
		}
		else
		{
			NWY_GNSS_INFO_DATA_T *gnssInfo = &(gnssData->gnss_data_info);
			char *data_temp1 = data;
			sprintf(data_temp1, "Longitude:%f, Latitude:%f, Altiude:%f", gnssInfo->longitude, gnssInfo->latitude, gnssInfo->altitude);
			nwy_test_cli_echo("\r\n %s\r\n", data);

			memset(data, 0, 100);
			NwyGnssTimeStamp *utc_time = &(gnssData->gnss_data_info.utc_time);
			char *data_temp2 = data;
			sprintf(data_temp2, "UTC Time: %04d/%02d/%02d  %02d:%02d:%02d", utc_time->wYear, utc_time->wMonth, utc_time->wDay, utc_time->wHour, utc_time->wMinute, utc_time->wSecond);
			nwy_test_cli_echo("\r\n %s\r\n", data);
		}
	}
	else
	{
		nwy_test_cli_echo("\r\nTest loc nmea info parse error!\r\n");
	}

    free(gnssData);
}


void nwy_test_cli_gnss_nmea_data()
{
    int ret = 0;
	ret = nwy_loc_nmea_data_ind( test_loc_event_cb);
	if (ret == 0)
		nwy_test_cli_echo("\r\nTest loc get nmea success!\r\n");
	else
		nwy_test_cli_echo("\r\nTest loc get nmea error!\r\n");
}

void nwy_test_cli_gnss_set_server()
{
#ifdef NWY_OPEN_TEST_AGPS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt = NULL;
    char *AGPS_URL = NULL;
    char *AGPS_USER = NULL;
    char *AGPS_PASS = NULL;
    int port = 0;
    int ret = 0;

    AGPS_URL = nwy_test_cli_input_gets("\r\nPlease input agps server url:");
    opt = nwy_test_cli_input_gets("\r\nPlease input agps server port:");

    port = atoi(opt);
    AGPS_USER = nwy_test_cli_input_gets("\r\nPlease input agps server user:");
    AGPS_PASS = nwy_test_cli_input_gets("\r\nPlease input agps server pass:");

    ret = nwy_loc_set_server(NWY_AGPS_URL, NWY_AGPS_PORT, NWY_AGPS_USER, NWY_AGPS_PASS);
    if (ret > 0)
        nwy_test_cli_echo("\r\nTest loc set server success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc set server error!\r\n");
#endif
}
#endif
#ifndef NWY_OPEN_TEST_BASE_LOC_NS
void nwy_cipgsmloc_cb(nwy_log_cipgsmloc_result_t *text)
{
    nwy_log_cipgsmloc_result_t *param = (nwy_log_cipgsmloc_result_t *)text;

    if (NULL == param)
        return;

    nwy_test_cli_echo("\r\n cipgsmloc info\r\n");
    if (0 == param->result)
    {
        nwy_test_cli_echo("lat %lf \r\n", param->info.data.lat);
        nwy_test_cli_echo("lng %lf \r\n", param->info.data.lng);
        nwy_test_cli_echo("accuracy %lf \r\n", param->info.data.alt);
    }
    else
    {
        nwy_test_cli_echo(" %s\r\n", param->info.errmsg);
    }

    return;
}
#endif

#ifndef NWY_OPEN_TEST_WIFI_LOC_NS
void nwy_wifigsmloc_cb(nwy_log_cipgsmloc_result_t *text)
{
    nwy_log_cipgsmloc_result_t *param = (nwy_log_cipgsmloc_result_t *)text;
    if (NULL == param)
        return;

    if (0 == param->result)
    {
        nwy_test_cli_echo("lat %lf \r\n", param->info.data.lat);
        nwy_test_cli_echo("lng %lf \r\n", param->info.data.lng);
        nwy_test_cli_echo("accuracy %lf \r\n", param->info.data.alt);
    }
    else
        nwy_test_cli_echo(" %s\r\n", param->info.errmsg);

    return;
}
#endif

void nwy_test_cli_gnss_open_base()
{
#ifdef NWY_OPEN_TEST_BASE_LOC_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt = NULL;
    int value = 0;
    int ret = 0;

    opt = nwy_test_cli_input_gets("\r\nPlease open lbs(1-open,0-close):");
    value = atoi(opt);

    if (1 == nwy_test_cli_check_data_connect())
    {
        nwy_pdp_set_status(NWY_PDP_CONNECTED);
    }
    else
    {
        nwy_test_cli_echo("\r\n need module dail at+xiic = 1");
        nwy_test_cli_send_virt_at();
    }

    ret = nwy_loc_cipgsmloc_open(value, nwy_cipgsmloc_cb);
    if (ret > 0)
        nwy_test_cli_echo("\r\nTest loc lbs success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc lbs error!\r\n");
#endif
}

void nwy_test_cli_wifi_open_base()
{
#ifdef NWY_OPEN_TEST_WIFI_LOC_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt = NULL;
    int value = 0;
    int ret = 0;

    opt = nwy_test_cli_input_gets("\r\nPlease open wifi lbs(1-open,0-close):");
    value = atoi(opt);

    ret = nwy_loc_wifigsmloc_open(value, nwy_wifigsmloc_cb);
    if (ret > 0)
        nwy_test_cli_echo("\r\nTest open wifi loc success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest open wifi loc error!\r\n");
#endif
}

void nwy_test_cli_gnss_open_assisted()
{
#ifdef NWY_OPEN_TEST_AGPS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt = NULL;
    int value = 0;
    int ret = 0;

    if (1 == nwy_test_cli_check_data_connect())
    {
        nwy_pdp_set_status(NWY_PDP_CONNECTED);
    }
    else
    {
        nwy_test_cli_echo("\r\n need module dail at+xiic = 1");
        nwy_test_cli_send_virt_at();
    }

    nwy_loc_cipgsmloc_open(1, nwy_cipgsmloc_cb);

    opt = nwy_test_cli_input_gets("\r\nPlease open A-GPS(1-open,0-close):");
    value = atoi(opt);

    ret = nwy_loc_agps_open(value);
    if (ret > 0)
        nwy_test_cli_echo("\r\nTest open A-GPS success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest open A-GPS error!\r\n");
#endif
}

#ifndef NWY_OPEN_TEST_LOC_NS
void nwy_test_cli_gnss_close()
{
    int ret = 0;
    ret = nwy_loc_stop_navigation();

    if (ret == 0)
        nwy_test_cli_echo("\r\nTest loc close success!\r\n");
    else
        nwy_test_cli_echo("\r\nTest loc close error!\r\n");
}
#endif
#endif
/*----------------------------------------*/
