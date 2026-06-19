#include "nwy_test_cli_utils.h"
#if (defined(FEATURE_NWY_PAHO_MQTT_V5)) || (defined(FEATURE_NWY_PAHO_MQTT_V3))
#include "MQTTClient.h"
#include "nwy_mqtt.h"
#endif
#ifdef FEATURE_NWY_ALI_MQTT
#include "mqtt_api.h"
#endif

/**************************ALI MQTT*********************************/
char *sptr;

#ifdef FEATURE_NWY_ALI_MQTT

static void *pclient = NULL;

#define MQTT_PRODUCT_KEY "a1cPP8Xe4Ax"
#define MQTT_DEVICE_NAME "TEST_T8"
#define MQTT_DEVICE_SECRET "dP1UMS0gKlNsvBrPvEfiChBmpYnz2lNI"
#define MSG_LEN_MAX (1024)

static int mqtt_sub_flag = -1;
static int mqtt_pub_flag = -1;
static int mqtt_unsub_flag = -1;
typedef struct
{
    char topic_name[128];
    iotx_mqtt_event_handle_func_fpt topic_handle_func;
}topic_list_t;
static topic_list_t topic_list[5] = {0};
int check_topic_exit(char *topic_name)
{
    int i = 0;
    for (i = 0; i < 5; i++)
    {
        if(strlen(topic_list[i].topic_name) && strstr(topic_list[i].topic_name, topic_name))
            return 1;
    }
    return 0;
}
int update_topic_list(char *topic_name, int type)
{
    int i = 0;
    for (i = 0; i < 5; i++)
    {
        if(type == 1)
        {
            if(strlen(topic_list[i].topic_name) == 0)
            {
                strncpy(topic_list[i].topic_name, topic_name, strlen(topic_name));
                return 1;
            }
        }
        else
        {
            if(strstr(topic_list[i].topic_name, topic_name))
            {
                memset(topic_list[i].topic_name, 0, sizeof(topic_list[i].topic_name));
                return 1;
            }
        }
    }
    nwy_test_cli_echo("update failed\r\n");
    return 0;
}


static void message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt ptopic_info = (iotx_mqtt_topic_info_pt)msg->msg;
    char topic_name[256] = {0};
    strncpy(topic_name, ptopic_info->ptopic, ptopic_info->topic_len);
    nwy_test_cli_echo("\r\ntopic_len:%d, Topic:%s, payload_len:%d, Payload:%s\r\n",
                      ptopic_info->topic_len, topic_name,
                      ptopic_info->payload_len, ptopic_info->payload);
}

static void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type)
    {
    case IOTX_MQTT_EVENT_UNDEF:
        nwy_test_cli_echo("\r\nundefined event occur.");
        break;

    case IOTX_MQTT_EVENT_DISCONNECT:
        nwy_test_cli_echo("\r\nMQTT disconnect.");
        break;

    case IOTX_MQTT_EVENT_RECONNECT:
        nwy_test_cli_echo("\r\nMQTT reconnect.");
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
        mqtt_sub_flag = 1;
        nwy_test_cli_echo("\r\nsubscribe success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
        mqtt_sub_flag = 0;
        nwy_test_cli_echo("\r\nsubscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
        mqtt_sub_flag = 0;
        nwy_test_cli_echo("\r\nsubscribe nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
        mqtt_unsub_flag = 1;
        nwy_test_cli_echo("\r\nunsubscribe success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
        mqtt_unsub_flag = 0;
        nwy_test_cli_echo("\r\nunsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
        mqtt_unsub_flag = 0;
        nwy_test_cli_echo("\r\nunsubscribe nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
        mqtt_pub_flag = 1;
        nwy_test_cli_echo("\r\npublish success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
        mqtt_pub_flag = 0;
        nwy_test_cli_echo("\r\npublish timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_NACK:
        mqtt_pub_flag = 0;
        nwy_test_cli_echo("\r\npublish nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_RECEIVED:
        nwy_test_cli_echo("\r\ntopic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
        break;

    default:
        nwy_test_cli_echo("\r\nShould NOT arrive here.");
        break;
    }
}

nwy_osi_thread_t task_id = NULL;

void nwy_ali_cycle(void *ctx)
{
    int ret = 0;

    for (;;)
    {
        while (ret == 0)
        {
            ret = IOT_MQTT_Yield(pclient, 300);
            nwy_sleep(100);
            if (ret != 0)
            {
                IOT_MQTT_Destroy(&pclient);
                break;
            }
        }
        nwy_suspend_thread(task_id);
    }
}

nwy_osi_thread_t nwy_at_ali_yeild_task_init(void)
{
    if (task_id == NULL)
    {
        nwy_create_thread(&task_id, NULL, 1024 * 15, NWY_OSI_PRIORITY_NORMAL, "neo_ali_yeild_task", nwy_ali_cycle,
                                    NULL , 4);
    }
    else
    {
        nwy_resume_thread(task_id);
    }

    nwy_test_cli_echo(" CreateThread neo_ali_yeild_task\n");
    return task_id;
}

int nwy_Snprintf(char *str, const int len, const char *fmt, ...)
{
    va_list args;
    int rc;
    va_start(args, fmt);
    rc = vsnprintf(str, len, fmt, args);
    va_end(args);
    return rc;
}

int iotx_midreport_topic(char *topic_name, char *topic_head, char *product_key, char *device_name)
{
    int ret;
    /* reported topic name: "/sys/${productKey}/${deviceName}/thing/status/update" */
    int len = strlen(product_key) + strlen(device_name) + 128;
    ret = nwy_Snprintf(topic_name,
                       len,
                       "%s/sys/%s/%s/thing/status/update",
                       topic_head,
                       product_key,
                       device_name);
    return ret;
}

void nwy_test_cli_alimqtt_connect()
{
    int domain_type = IOTX_CLOUD_DOMAIN_SH;
    char topic[128] = {0};
    static iotx_conn_info_pt pconn_info = NULL;
    static iotx_mqtt_param_t mqtt_params;
    nwy_osi_thread_t id = NULL;
    unsigned keepalive = 120000;
    unsigned int clean = 0;
    unsigned int timeout = 30000;
    int ret = 0;
    int dynamic_register = 0;

    iotx_mqtt_topic_info_t topic_info;

    char msg[MSG_LEN_MAX] = {0};
    nwy_test_cli_echo("\r\nalimqtt test");
    //IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, &dynamic_register);
    IOT_Ioctl(IOTX_IOCTL_SET_REGION, (void *)&domain_type);
    IOT_Ioctl(IOTX_IOCTL_SET_PRODUCT_KEY, (void *)MQTT_PRODUCT_KEY);
    IOT_Ioctl(IOTX_IOCTL_SET_DEVICE_NAME, (void *)MQTT_DEVICE_NAME);
    IOT_Ioctl(IOTX_IOCTL_SET_DEVICE_SECRET, (void *)MQTT_DEVICE_SECRET);
    ret = IOT_SetupConnInfo(MQTT_PRODUCT_KEY, MQTT_DEVICE_NAME, MQTT_DEVICE_SECRET, (void **)&pconn_info);
    if (ret == 0)
    {
        if ((NULL == pconn_info) || (0 == pconn_info->client_id))
        {
            nwy_test_cli_echo("\r\nplease auth fail\n");
            return;
        }
        nwy_test_cli_echo("\r\nIOT_SetupConnInfo SUCCESS\r\n");
        memset(&mqtt_params, 0x0, sizeof(mqtt_params));
        mqtt_params.port = pconn_info->port;
        mqtt_params.host = pconn_info->host_name;
        mqtt_params.client_id = pconn_info->client_id;
        mqtt_params.username = pconn_info->username;
        mqtt_params.password = pconn_info->password;
        mqtt_params.pub_key = pconn_info->pub_key;

        mqtt_params.request_timeout_ms = timeout;
        mqtt_params.clean_session = clean;
        mqtt_params.keepalive_interval_ms = keepalive;
        //mqtt_params.pread_buf = msg_readbuf;
        mqtt_params.read_buf_size = MSG_LEN_MAX;
        //mqtt_params.pwrite_buf = msg_writebuf;
        mqtt_params.write_buf_size = MSG_LEN_MAX;

        mqtt_params.handle_event.h_fp = event_handle;
        mqtt_params.handle_event.pcontext = NULL;
        ret = IOT_MQTT_CheckStateNormal(pclient);
        if (1 == ret)
        {
            nwy_test_cli_echo("\r\nMQTT is connected");
            return;
        }
        pclient = IOT_MQTT_Construct(&mqtt_params);
        if (NULL == pclient)
        {
            nwy_test_cli_echo("\r\nMQTT construct failed\n");
            return;
        }
        else
        {
            nwy_test_cli_echo("\r\nMQTT construct success\n");
            id = nwy_at_ali_yeild_task_init();
            if (NULL == id)
            {
                nwy_test_cli_echo("\r\nMQTT construct failed\n");
                return;
            }
        }
        iotx_midreport_topic(topic, "", MQTT_PRODUCT_KEY, MQTT_DEVICE_NAME);
        nwy_test_cli_echo("\r\ntopic = %s\r\n", topic);
        memset(topic_list, 0, sizeof(topic_list));
        ret = IOT_MQTT_Subscribe(pclient, topic, IOTX_MQTT_QOS1, message_arrive, NULL);
        if (ret < 0)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe error");
            return;
        }
        while (1)
        {
            if (1 == mqtt_sub_flag)
            {
                nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe OK\r\n");
                update_topic_list(topic, 1);
                break;
            }
            else if (0 == mqtt_sub_flag)
            {
                nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe error\r\n");
                break;
            }
            nwy_sleep(100);
        }
        /*
    iotx_midreport_reqid(requestId,
     dev->product_key,
     dev->device_name)
    iotx_midreport_payload(msg, )/*/
        strcpy(msg, "{hello word}");
        topic_info.qos = IOTX_MQTT_QOS1;
        topic_info.payload = (void *)msg;
        topic_info.payload_len = strlen(msg);
        topic_info.retain = 0;
        topic_info.dup = 0;
        ret = IOT_MQTT_Publish(pclient, topic, &topic_info);
        if (ret < 0)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Publish error");
            return;
        }

        while (1)
        {
            if (1 == mqtt_pub_flag)
            {
                nwy_test_cli_echo("\r\nIOT_MQTT_Publish OK");
                break;
            }
            else if (0 == mqtt_pub_flag)
            {
                nwy_test_cli_echo("\r\nIOT_MQTT_Publish error");
                break;
            }
            nwy_sleep(100);
        }
    }
    else
        nwy_test_cli_echo("\r\nMQTT auth failed");
}

void nwy_test_cli_alimqtt_pub()
{
    char msg[MSG_LEN_MAX] = {0};
    int ret = 0;
    iotx_mqtt_topic_info_t topic_info;
    char topic[128] = {0};
    iotx_midreport_topic(topic, "", MQTT_PRODUCT_KEY, MQTT_DEVICE_NAME);
    strcpy(msg, "{\"id\":\"12\",\"params\":{\"temperature\":18,\"data\":\"2020/04/03\"}}");
    topic_info.qos = IOTX_MQTT_QOS1;
    topic_info.payload = (void *)msg;
    topic_info.payload_len = strlen(msg);
    topic_info.retain = 0;
    topic_info.dup = 0;
    ret = IOT_MQTT_Publish(pclient, topic, &topic_info);
    if (ret < 0)
    {
        nwy_test_cli_echo("\r\nIOT_MQTT_Publish error");
        return;
    }

    while (1)
    {
        if (1 == mqtt_pub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Publish OK");
            break;
        }
        else if (0 == mqtt_pub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Publish error");
            break;
        }
        nwy_sleep(100);
    }
}

void nwy_test_cli_alimqtt_sub()
{
    char topic[128] = {0};
    int ret = 0;
    iotx_midreport_topic(topic, "", MQTT_PRODUCT_KEY, MQTT_DEVICE_NAME);
    nwy_test_cli_echo("\r\ntopic = %s", topic);
    if(check_topic_exit(topic))
    {
        nwy_test_cli_echo("topic has been subscibe\r\n");
        return ;
    }
    ret = IOT_MQTT_Subscribe(pclient, topic, IOTX_MQTT_QOS1, message_arrive, NULL);
    if (ret < 0)
    {
        nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe error");
        return;
    }
    mqtt_sub_flag = -1;

    while (1)
    {
        if (1 == mqtt_sub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe OK");
            update_topic_list(topic, 1);
            break;
        }
        else if (0 == mqtt_sub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Subscribe error");
            break;
        }
        nwy_sleep(100);
    }
}

void nwy_test_cli_alimqtt_unsub()
{
    int rc = 0;
    char topic[128] = {0};
    iotx_midreport_topic(topic, "", MQTT_PRODUCT_KEY, MQTT_DEVICE_NAME);
    rc = IOT_MQTT_Unsubscribe(pclient, topic);
    if (rc < 0)
    {
        nwy_test_cli_echo("\r\nIOT_MQTT_Unsubscribe error");
        return;
    }
    while (1)
    {
        if (1 == mqtt_unsub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Unsubscribe OK");
            update_topic_list(topic, 0);
            break;
        }
        else if (0 == mqtt_unsub_flag)
        {
            nwy_test_cli_echo("\r\nIOT_MQTT_Unsubscribe error");
            break;
        }
        nwy_sleep(100);
    }
}

void nwy_test_cli_alimqtt_state()
{
    int ret = 0;
    ret = IOT_MQTT_CheckStateNormal(pclient);
    nwy_test_cli_echo("\r\nMQTT state is %d", ret);
}

void nwy_test_cli_alimqtt_disconnect()
{
    int ret = 0;
    ret = IOT_MQTT_CheckStateNormal(pclient);
    if (ret == 0)
        nwy_test_cli_echo("\r\nMQTT is disconnected ");
    else
    {
        IOT_MQTT_Destroy(&pclient);
        pclient = NULL;
        nwy_test_cli_echo("\r\nMQTT is Destroy");
    }
}
#endif
#ifdef FEATURE_NWY_PAHO_MQTT_V3
/**************************MQTT*********************************/
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
MQTTClient paho_mqtt_client;
unsigned char *g_nwy_paho_readbuf = NULL;
unsigned char *g_nwy_paho_writebuf = NULL;
nwy_osi_thread_t nwy_paho_task_id = NULL;
Network *paho_network = NULL;
#define NWY_EXT_SIO_PER_LEN 1024
char echo_buff[NWY_EXT_SIO_PER_LEN + 1] = {0};
nwy_paho_mqtt_at_param_type paho_mqtt_at_param = {0};

void messageArrived(MessageData *md)
{
    char topic_name[PAHO_TOPIC_LEN_MAX] = {0};
    int i = 0;
    unsigned int remain_len = 0;
    strncpy(topic_name, md->topicName->lenstring.data, md->topicName->lenstring.len);
    nwy_test_cli_echo("\r\n===messageArrived======");
    nwy_test_cli_echo("\r\npayloader len is %d", md->message->payloadlen);
    nwy_test_cli_echo("\r\n%s:\r\n", topic_name);
    remain_len = md->message->payloadlen;

    if (md->message->payloadlen > NWY_EXT_SIO_PER_LEN)
    {
        for (i = 0; i < ((md->message->payloadlen / NWY_EXT_SIO_PER_LEN) + 1); i++)
        {
            memset(echo_buff, 0, sizeof(echo_buff));
            strncpy(echo_buff, md->message->payload + i * NWY_EXT_SIO_PER_LEN,
                    remain_len > NWY_EXT_SIO_PER_LEN ? NWY_EXT_SIO_PER_LEN : remain_len);
            remain_len = md->message->payloadlen - (i + 1) * NWY_EXT_SIO_PER_LEN;
            nwy_test_cli_echo(echo_buff);
        }
    }
    else
    {
        memset(echo_buff, 0, sizeof(echo_buff));
        strncpy(echo_buff, md->message->payload, md->message->payloadlen);
        nwy_test_cli_echo(echo_buff);
    }
}

void nwy_paho_cycle(void *ctx)
{
    while (1)
    {
        while (MQTTIsConnected(&paho_mqtt_client))
        {
            MQTTYield(&paho_mqtt_client, 500);
            nwy_sleep(200);
        }
        nwy_test_cli_echo("\r\nMQTT disconnect ,Out paho cycle");
        nwy_suspend_thread(nwy_paho_task_id);
    }
    nwy_sleep(200);
}
nwy_osi_thread_t nwy_paho_yeild_task_init(void)
{

    if (nwy_paho_task_id == NULL)
    {
        nwy_create_thread(&nwy_paho_task_id, 1024 * 2,NWY_OSI_PRIORITY_NORMAL,"neo_paho_yeild_task", nwy_paho_cycle,NULL, 4);
    }
    else
        nwy_resume_thread(nwy_paho_task_id);
    return nwy_paho_task_id;
}
void nwy_test_cli_mqtt_connect()
{
    int rc;
    nwy_test_cli_echo("\r\nnwy_test_cli_mqtt_connect\r\n");
    if (MQTTIsConnected(&paho_mqtt_client) == 1)
    {
        nwy_test_cli_echo("\r\npaho mqtt already connect");
        return;
    }
    memset(&paho_mqtt_at_param, 0, sizeof(nwy_paho_mqtt_at_param_type));
    sptr = nwy_test_cli_input_gets("\r\nPlease input url/ip: ");
    strncpy(paho_mqtt_at_param.host_name, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input port: ");
    paho_mqtt_at_param.port = atoi(sptr);

    sptr = nwy_test_cli_input_gets("\r\nPlease input client_id: ");
    strncpy(paho_mqtt_at_param.clientID, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input usrname: ");
    strncpy(paho_mqtt_at_param.username, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input password: ");
    strncpy(paho_mqtt_at_param.password, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input sslmode(1-ssl,0-no ssl): ");
    if (atoi(sptr) > 1 || atoi(sptr) < 0)
    {
        nwy_test_cli_echo("\r\ninput sslmode error");
        return;
    }
    paho_mqtt_at_param.paho_ssl_tcp_conf.sslmode = atoi(sptr);
    if (g_nwy_paho_writebuf != NULL)
    {
        free(g_nwy_paho_writebuf);
        g_nwy_paho_writebuf = NULL;
    }
    if (NULL == (g_nwy_paho_writebuf = (unsigned char *)malloc(PAHO_PLAYOAD_LEN_MAX)))
    {
        nwy_test_cli_echo("\r\nmalloc buffer g_nwy_paho_writebuf error");
        return;
    }
    if (g_nwy_paho_readbuf != NULL)
    {
        free(g_nwy_paho_readbuf);
        g_nwy_paho_readbuf = NULL;
    }
    if (NULL == (g_nwy_paho_readbuf = (unsigned char *)malloc(PAHO_PLAYOAD_LEN_MAX)))
    {
        nwy_test_cli_echo("\r\nmalloc buffer g_nwy_paho_readbuf error");
        return;
    }
    memset(g_nwy_paho_readbuf, 0, PAHO_PLAYOAD_LEN_MAX);
    memset(g_nwy_paho_writebuf, 0, PAHO_PLAYOAD_LEN_MAX);
    if (paho_network != NULL)
    {
        free(paho_network);
        paho_network = NULL;
    }
    if (NULL == (paho_network = (Network *)malloc(sizeof(Network))))
    {
        nwy_test_cli_echo("\r\nmalloc buffer g_nwy_paho_readbuf error");
        return;
    }
    memset(paho_network, 0, sizeof(Network));
    NetworkInit(paho_network);
    if (paho_mqtt_at_param.paho_ssl_tcp_conf.sslmode == 1)
    {
        sptr = nwy_test_cli_input_gets("\r\nPlease input auth_mode(0/1/2): ");
        paho_mqtt_at_param.paho_ssl_tcp_conf.authmode = atoi(sptr);
        if (paho_mqtt_at_param.paho_ssl_tcp_conf.authmode == 0)
        {
            paho_network->tlsConnectParams.pRootCALocation = NULL;
            paho_network->tlsConnectParams.pDeviceCertLocation = NULL;
            paho_network->tlsConnectParams.pDevicePrivateKeyLocation = NULL;
        }
        else if (paho_mqtt_at_param.paho_ssl_tcp_conf.authmode == 1)
        {
            sptr = nwy_test_cli_input_gets("\r\nPlease input ca: ");
            strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name, sptr, strlen(sptr));
            paho_network->tlsConnectParams.pRootCALocation = paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name;
            paho_network->tlsConnectParams.pDeviceCertLocation = NULL;
            paho_network->tlsConnectParams.pDevicePrivateKeyLocation = NULL;
        }
        else
        {
            sptr = nwy_test_cli_input_gets("\r\nPlease input ca: ");
            strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name, sptr, strlen(sptr));
            paho_network->tlsConnectParams.pRootCALocation = paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name;
            sptr = nwy_test_cli_input_gets("\r\nPlease input clientcert: ");
            strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.clientcert.cert_name, sptr, strlen(sptr));
            sptr = nwy_test_cli_input_gets("\r\nPlease input clientkey: ");
            strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.clientkey.cert_name, sptr, strlen(sptr));
            paho_network->tlsConnectParams.pDeviceCertLocation = paho_mqtt_at_param.paho_ssl_tcp_conf.clientcert.cert_name;
            paho_network->tlsConnectParams.pDevicePrivateKeyLocation = paho_mqtt_at_param.paho_ssl_tcp_conf.clientkey.cert_name;
        }
        paho_network->tlsConnectParams.ServerVerificationFlag = paho_mqtt_at_param.paho_ssl_tcp_conf.authmode;
        paho_network->is_SSL = 1;
        paho_network->tlsConnectParams.timeout_ms = 20000;
        sptr = nwy_test_cli_input_gets("\r\nPlease input sslversion(0-3): ");
        if(atoi(sptr)>3 || atoi(sptr)<0)
        {
          nwy_test_cli_echo("\r\ninput sslversion error");
          return ;
        }
        paho_network->tlsConnectParams.sslversion = atoi(sptr);
    }
    else
        nwy_test_cli_echo("\r\nis no-SSL NetworkConnect");
    sptr = nwy_test_cli_input_gets("\r\nPlease input clean_flag(0/1): ");
    if (atoi(sptr) > 1 || atoi(sptr) < 0)
    {
        nwy_test_cli_echo("\r\ninput clean_flag error");
        return;
    }
    paho_mqtt_at_param.cleanflag = (sptr[0] - 0x30);
    sptr = nwy_test_cli_input_gets("\r\nPlease input keep_alive: ");
    paho_mqtt_at_param.keepalive = atoi(sptr);
    nwy_test_cli_echo("\r\nip:%s, port :%d", paho_mqtt_at_param.host_name, paho_mqtt_at_param.port);
    rc = NetworkConnect(paho_network, paho_mqtt_at_param.host_name, paho_mqtt_at_param.port);
    if (rc < 0)
    {
        nwy_test_cli_echo("\r\nNetworkConnect err rc=%d", rc);
        return;
    }
    nwy_test_cli_echo("\r\nNetworkConnect ok");
    MQTTClientInit(&paho_mqtt_client, paho_network, 30000, g_nwy_paho_writebuf, PAHO_PLAYOAD_LEN_MAX,
                   g_nwy_paho_readbuf, PAHO_PLAYOAD_LEN_MAX);
    MQTTClientInit_defaultMessage(&paho_mqtt_client, messageArrived);
    data.clientID.cstring = paho_mqtt_at_param.clientID;
    if (0 != strlen((char *)paho_mqtt_at_param.username) && 0 != strlen((char *)paho_mqtt_at_param.password))
    {
        data.username.cstring = paho_mqtt_at_param.username;
        data.password.cstring = paho_mqtt_at_param.password;
    }
    data.keepAliveInterval = paho_mqtt_at_param.keepalive;
    data.cleansession = paho_mqtt_at_param.cleanflag;
    if (0 != strlen((char *)paho_mqtt_at_param.willtopic))
    {
        memset(&data.will, 0x0, sizeof(data.will));
        data.willFlag = 1;
        data.will.retained = paho_mqtt_at_param.willretained;
        data.will.qos = paho_mqtt_at_param.willqos;
        if (paho_mqtt_at_param.willmessage_len != 0)
        {
            data.will.topicName.lenstring.data = paho_mqtt_at_param.willtopic;
            data.will.topicName.lenstring.len = strlen((char *)paho_mqtt_at_param.willtopic);
            data.will.message.lenstring.data = paho_mqtt_at_param.willmessage;
            data.will.message.lenstring.len = paho_mqtt_at_param.willmessage_len;
        }
        else
        {
            data.will.topicName.cstring = paho_mqtt_at_param.willtopic;
            data.will.message.cstring = paho_mqtt_at_param.willmessage;
        }
        nwy_test_cli_echo("\r\nMQTT will ready");
    }
    nwy_test_cli_echo("\r\nConnecting MQTT");
    if ((rc = nwy_MQTTConnect(&paho_mqtt_client, &data)))
        nwy_test_cli_echo("\r\nFailed to create client, return code %d", rc);
    else
    {
        nwy_test_cli_echo("\r\nMQTT connect ok");
        nwy_osi_thread_t task_id = nwy_paho_yeild_task_init();
        if (task_id == NULL)
            nwy_test_cli_echo("\r\npaho yeid task create failed ");
        else
            nwy_test_cli_echo("\r\npaho yeid task create ok ");
    }
}

void nwy_test_cli_mqtt_pub()
{
    int rc;
    MQTTMessage pubmsg = {0};
    if (MQTTIsConnected(&paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        sptr = nwy_test_cli_input_gets("\r\nPlease input retained: ");
        paho_mqtt_at_param.retained = atoi(sptr);
        memset(paho_mqtt_at_param.message, 0, sizeof(paho_mqtt_at_param.message));
#ifdef FEATURE_NWY_N58_OPEN_NIPPON
        sptr = nwy_test_cli_input_gets("\r\nPlease input message(<= 2048): ");
        if (strlen(sptr) > NWY_EXT_SIO_RX_MAX)
        {
            nwy_test_cli_echo("\r\nNo more than 2048 bytes at a time ");
            return;
        }
#else
        sptr = nwy_test_cli_input_gets("\r\nPlease input message(<= 512): ");
        if (strlen(sptr) > 512)
        {
            nwy_test_cli_echo("\r\nNo more than 512 bytes at a time ");
            return;
        }
#endif
        strncpy(paho_mqtt_at_param.message, sptr, strlen(sptr));
        nwy_test_cli_echo("\r\nmqttpub param retained = %d, qos = %d, topic = %s, msg = %s",
                          paho_mqtt_at_param.retained,
                          paho_mqtt_at_param.qos,
                          paho_mqtt_at_param.topic,
                          paho_mqtt_at_param.message);
        memset(&pubmsg, 0, sizeof(pubmsg));
        pubmsg.payload = (void *)paho_mqtt_at_param.message;
        pubmsg.payloadlen = strlen(paho_mqtt_at_param.message);
        pubmsg.qos = paho_mqtt_at_param.qos;
        pubmsg.retained = paho_mqtt_at_param.retained;
        pubmsg.dup = 0;
        rc = nwy_MQTTPublish(&paho_mqtt_client, paho_mqtt_at_param.topic, &pubmsg);
        nwy_test_cli_echo("\r\nmqtt publish rc:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT not connect");
}

void nwy_test_cli_mqtt_sub()
{
    int rc;
    if (MQTTIsConnected(&paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        rc = MQTTSubscribe(&paho_mqtt_client,
                           (char *)paho_mqtt_at_param.topic,
                           paho_mqtt_at_param.qos,
                           messageArrived);
        if (rc == SUCCESS)
            nwy_test_cli_echo("\r\nMQTT Sub ok");
        else
            nwy_test_cli_echo("\r\nMQTT Sub error:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT no connect");
}

void nwy_test_cli_mqtt_unsub()
{
    int rc;
    if (MQTTIsConnected(&paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        rc = MQTTUnsubscribe(&paho_mqtt_client, paho_mqtt_at_param.topic);
        if (rc == SUCCESS)
            nwy_test_cli_echo("\r\nMQTT Unsub ok");
        else
            nwy_test_cli_echo("\r\nMQTT Unsub error:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT no connect");
}

void nwy_test_cli_mqtt_state()
{
    if (MQTTIsConnected(&paho_mqtt_client))
        nwy_test_cli_echo("\r\nMQTTconnect");
    else
        nwy_test_cli_echo("\r\nMQTT disconnect");
}

void nwy_test_cli_mqtt_disconnect()
{
    if (MQTTIsConnected(&paho_mqtt_client))
    {
        MQTTDisconnect(&paho_mqtt_client);
        NetworkDisconnect(paho_network);
        if (NULL != g_nwy_paho_writebuf)
        {
            free(g_nwy_paho_writebuf);
            g_nwy_paho_writebuf = NULL;
        }
        if (NULL != g_nwy_paho_readbuf)
        {
            free(g_nwy_paho_readbuf);
            g_nwy_paho_readbuf = NULL;
        }
        if (NULL != paho_network)
        {
            free(paho_network);
            paho_network = NULL;
        }
    }
    nwy_test_cli_echo("\r\nMQTT disconnect ok");
}
#ifdef FEATURE_NWY_N58_OPEN_NIPPON
void nwy_test_cli_mqtt_pub_test(void)
{
    int rc, i;
    MQTTMessage pubmsg = {0};
    if (MQTTIsConnected(&paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        sptr = nwy_test_cli_input_gets("\r\nPlease input retained: ");
        paho_mqtt_at_param.retained = atoi(sptr);
        memset(paho_mqtt_at_param.message, 0, sizeof(paho_mqtt_at_param.message));
        sptr = nwy_test_cli_input_gets("\r\nPlease input 1k str,msg(10k) consists of 10 str");
        if (strlen(sptr) != 1024)
        {
            nwy_test_cli_echo("\r\nmust be 1k message");
            return;
        }
        for (i = 0; i < 10; i++)
            strncpy(paho_mqtt_at_param.message + i * strlen(sptr), sptr, strlen(sptr));
        nwy_test_cli_echo("\r\nmqttpub param retained = %d, qos = %d, topic = %s, strlen is %d",
                          paho_mqtt_at_param.retained,
                          paho_mqtt_at_param.qos,
                          paho_mqtt_at_param.topic,
                          strlen(paho_mqtt_at_param.message));
        memset(&pubmsg, 0, sizeof(pubmsg));
        pubmsg.payload = (void *)paho_mqtt_at_param.message;
        pubmsg.payloadlen = strlen(paho_mqtt_at_param.message);
        pubmsg.qos = paho_mqtt_at_param.qos;
        pubmsg.retained = paho_mqtt_at_param.retained;
        pubmsg.dup = 0;
        rc = nwy_MQTTPublish(&paho_mqtt_client, paho_mqtt_at_param.topic, &pubmsg);
        nwy_test_cli_echo("\r\nmqtt publish rc:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT not connect");
}
#endif
#endif
#ifdef FEATURE_NWY_PAHO_MQTT_V5
MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
MQTTClient paho_mqtt_client;
nwy_osi_thread_t nwy_paho_task_id;
#define NWY_EXT_SIO_PER_LEN 1024
char echo_buff[NWY_EXT_SIO_PER_LEN + 1] = {0};
nwy_paho_mqtt_at_param_type paho_mqtt_at_param = {0};
int messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* m)
{
    char topic_name[PAHO_TOPIC_LEN_MAX] = {0};
    int i = 0;
    unsigned int remain_len = 0;
    nwy_test_cli_echo("\r\n===messageArrived======");
    nwy_test_cli_echo("\r\npayloader len is %d", m->payloadlen);
    nwy_test_cli_echo("\r\n%s:\r\n", topicName);
	nwy_test_cli_echo("message received on topic %s is ",topicName);
    remain_len = m->payloadlen;
    if (m->payloadlen > NWY_EXT_SIO_PER_LEN)
    {
        for (i = 0; i < ((m->payloadlen / NWY_EXT_SIO_PER_LEN) + 1); i++)
        {
            memset(echo_buff, 0, sizeof(echo_buff));
            strncpy(echo_buff, m->payload + i * NWY_EXT_SIO_PER_LEN,
                    remain_len > NWY_EXT_SIO_PER_LEN ? NWY_EXT_SIO_PER_LEN : remain_len);
            remain_len = m->payloadlen - (i + 1) * NWY_EXT_SIO_PER_LEN;
            nwy_test_cli_echo(echo_buff);
        }
    }
    else
    {
        memset(echo_buff, 0, sizeof(echo_buff));
        strncpy(echo_buff, m->payload, m->payloadlen);
        nwy_test_cli_echo(echo_buff);
    }
	return 1;
}


void nwy_paho_cycle(void *ctx)
{
    while (1)
    {
        while (MQTTClient_isConnected(paho_mqtt_client))
        {
            MQTTClient_yield();
            nwy_sleep(500);
        }
        nwy_test_cli_echo("\r\nMQTT disconnect ,Out paho cycle");
        nwy_suspend_thread(nwy_paho_task_id);
    }
    nwy_sleep(200);
}


nwy_osi_thread_t nwy_paho_yeild_task_init(void)
{

    if ( nwy_paho_task_id == NULL)
    {
        nwy_create_thread(&nwy_paho_task_id, NULL, 1024 * 2,NWY_OSI_PRIORITY_NORMAL,"neo_paho_yeild_task", nwy_paho_cycle,
                                                     NULL, 4);
    }
    else
        nwy_resume_thread(nwy_paho_task_id);
    return nwy_paho_task_id;
}
void nwy_test_cli_mqtt_connect()
{
    int rc;
    size_t file_size;
    int authmode =0;
    MQTTClient_init();
    char url[128] = {0};
    nwy_test_cli_echo("\r\nnwy_test_cli_mqtt_connect\r\n");
    if (MQTTClient_isConnected(paho_mqtt_client) == 1)
    {
        nwy_test_cli_echo("\r\npaho mqtt already connect");
        return;
    }
    sptr = nwy_test_cli_input_gets("\r\nPlease input url/ip: ");
    strncpy(paho_mqtt_at_param.host_name, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input port: ");
    paho_mqtt_at_param.port = atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\nPlease input client_id: ");
    strncpy(paho_mqtt_at_param.clientID, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input usrname: ");
    strncpy(paho_mqtt_at_param.username, sptr, strlen(sptr));
    sptr = nwy_test_cli_input_gets("\r\nPlease input password: ");
    strncpy(paho_mqtt_at_param.password, sptr, strlen(sptr));
    opts.MQTTVersion = MQTTVERSION_DEFAULT;
    sptr = nwy_test_cli_input_gets("\r\nPlease input sslmode(1-ssl,0-no ssl): ");
    if (atoi(sptr) > 1 || atoi(sptr) < 0)
    {
        nwy_test_cli_echo("\r\ninput sslmode error");
        return;
    }
    paho_mqtt_at_param.paho_ssl_tcp_conf.sslmode = atoi(sptr);
    if(paho_mqtt_at_param.paho_ssl_tcp_conf.sslmode == 0)
        sprintf(url, "%s:%d", paho_mqtt_at_param.host_name, paho_mqtt_at_param.port);
    else
    {
        sprintf(url, "ssl://%s:%d", paho_mqtt_at_param.host_name, paho_mqtt_at_param.port);
        MQTTClient_SSLOptions sslopts = MQTTClient_SSLOptions_initializer;
        sptr = nwy_test_cli_input_gets("\r\nPlease input auth_mode(0/1/2): ");
        authmode = atoi(sptr);
        if (authmode == 0)
           sslopts.enableServerCertAuth = 0;
        else if (authmode == 1)
        {
           sslopts.enableServerCertAuth = 1;
           sptr = nwy_test_cli_input_gets("\r\nPlease input ca: ");
           strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name, sptr, strlen(sptr));
           sslopts.trustStore = paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name;
        }
        else
        {
           sslopts.enableServerCertAuth = 1;
           sptr = nwy_test_cli_input_gets("\r\nPlease input ca: ");
           strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name, sptr, strlen(sptr));
           if(nwy_mbedtls_load_file(paho_mqtt_at_param.paho_ssl_tcp_conf.cacert.cert_name, 
                    &sslopts.trustStore, &file_size)< 0)
           {
                nwy_test_cli_echo("\r\nload ca error");
                return;
           }
           sptr = nwy_test_cli_input_gets("\r\nPlease input clientcert: ");
           strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.clientcert.cert_name, sptr, strlen(sptr));
           if(nwy_mbedtls_load_file(paho_mqtt_at_param.paho_ssl_tcp_conf.clientcert.cert_name, 
                    &sslopts.keyStore, &file_size)< 0)
           {
                nwy_test_cli_echo("\r\niload keyStore error");
                return;
           }
           sptr = nwy_test_cli_input_gets("\r\nPlease input clientkey: ");
           strncpy(paho_mqtt_at_param.paho_ssl_tcp_conf.clientkey.cert_name, sptr, strlen(sptr));
           if(nwy_mbedtls_load_file(paho_mqtt_at_param.paho_ssl_tcp_conf.clientkey.cert_name, 
                    &sslopts.privateKey, &file_size)< 0)
           {
                nwy_test_cli_echo("\r\niload privateKey error");
                return;
           }
        }
        sptr = nwy_test_cli_input_gets("\r\nPlease input ssl version(0/1/2/3): ");
        if (atoi(sptr) > 3 || atoi(sptr) < 0)
        {
            nwy_test_cli_echo("\r\ninput sslverison error");
            return;
        }
        sslopts.sslVersion = atoi(sptr);
        sslopts.verify = 1;
        opts.ssl = &sslopts;
    }
    sptr = nwy_test_cli_input_gets("\r\nPlease input keep_alive: ");
    opts.keepAliveInterval = atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\nPlease input clean_flag(0/1): ");
    if (atoi(sptr) > 1 || atoi(sptr) < 0)
    {
        nwy_test_cli_echo("\r\ninput clean_flag error");
        return;
    }
    opts.cleansession = atoi(sptr);
    opts.username = paho_mqtt_at_param.username;
    opts.password = paho_mqtt_at_param.password;
    opts.connectTimeout = 10;
    opts.serverURIs = url;
    nwy_test_cli_echo("\r\nconnect url :%s\n", url);
    rc = MQTTClient_create(&paho_mqtt_client, url, paho_mqtt_at_param.clientID, MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
    if(rc != MQTTCLIENT_SUCCESS)
    {
        nwy_test_cli_echo("create mqtt client rc : %d\n", rc);
        return ;
    }
    rc = MQTTClient_setCallbacks(paho_mqtt_client, NULL, NULL, messageArrived, NULL);
    if(rc != MQTTCLIENT_SUCCESS)
    {
        nwy_test_cli_echo("set callbacks rc : %d\n", rc);
        return ;
    }
    nwy_test_cli_echo("\r\nConnecting MQTT");
    rc = MQTTClient_connect(paho_mqtt_client, &opts);
    if (rc!=MQTTCLIENT_SUCCESS)
        nwy_test_cli_echo("\r\nFailed to connect client, return code %d", rc);
    else
    {
        nwy_test_cli_echo("\r\nMQTT connect ok");
        nwy_osi_thread_t task_id = nwy_paho_yeild_task_init();
        if (task_id == NULL)
            nwy_test_cli_echo("\r\npaho yeid task create failed ");
        else
            nwy_test_cli_echo("\r\npaho yeid task create ok ");
    }
}

void nwy_test_cli_mqtt_sub()
{
    int rc;
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        rc = MQTTClient_subscribe(paho_mqtt_client, (char *)paho_mqtt_at_param.topic, paho_mqtt_at_param.qos);
        if (rc < 0)
            nwy_test_cli_echo("\r\nMQTT Sub error:%d", rc);
        else
            nwy_test_cli_echo("\r\nMQTT Sub ok");
    }
    else
        nwy_test_cli_echo("\r\nMQTT no connect");
}

void nwy_test_cli_mqtt_pub()
{
    int rc;
    MQTTClient_deliveryToken last_token;
    MQTTProperties pub_props = MQTTProperties_initializer;
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        sptr = nwy_test_cli_input_gets("\r\nPlease input retained: ");
        paho_mqtt_at_param.retained = atoi(sptr);
        memset(paho_mqtt_at_param.message, 0, sizeof(paho_mqtt_at_param.message));
#ifdef FEATURE_NWY_N58_OPEN_NIPPON
        sptr = nwy_test_cli_input_gets("\r\nPlease input message(<= 2048): ");
        if (strlen(sptr) > NWY_EXT_SIO_RX_MAX)
        {
            nwy_test_cli_echo("\r\nNo more than 2048 bytes at a time ");
            return;
        }
#else
        sptr = nwy_test_cli_input_gets("\r\nPlease input message(<= 512): ");
        if (strlen(sptr) > 512)
        {
            nwy_test_cli_echo("\r\nNo more than 512 bytes at a time ");
            return;
        }
#endif
        strncpy(paho_mqtt_at_param.message, sptr, strlen(sptr));
        nwy_test_cli_echo("\r\nmqttpub param retained = %d, qos = %d, topic = %s, msg = %s",
                          paho_mqtt_at_param.retained,
                          paho_mqtt_at_param.qos,
                          paho_mqtt_at_param.topic,
                          paho_mqtt_at_param.message);
        rc = MQTTClient_publish(paho_mqtt_client, paho_mqtt_at_param.topic, strlen(paho_mqtt_at_param.message),
                paho_mqtt_at_param.message, paho_mqtt_at_param.qos, paho_mqtt_at_param.retained, &last_token);
        nwy_test_cli_echo("\r\nmqtt publish rc:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT not connect");
}

void nwy_test_cli_mqtt_unsub()
{
    int rc;
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        rc = MQTTClient_unsubscribe(paho_mqtt_client, paho_mqtt_at_param.topic);
        if (rc < 0)
            nwy_test_cli_echo("\r\nMQTT unsub error:%d", rc);
        else
            nwy_test_cli_echo("\r\nMQTT unsub ok");


    }
    else
        nwy_test_cli_echo("\r\nMQTT no connect");
}

void nwy_test_cli_mqtt_state()
{
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
        nwy_test_cli_echo("\r\nMQTTconnect");
    else
        nwy_test_cli_echo("\r\nMQTT disconnect");
}

void nwy_test_cli_mqtt_disconnect()
{
    int rc;
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
    {
        MQTTClient_disconnect(paho_mqtt_client, 0);
        MQTTClient_destroy(&paho_mqtt_client);
    }
    nwy_test_cli_echo("\r\nMQTT disconnect ok");
}
#ifdef FEATURE_NWY_N58_OPEN_NIPPON
void nwy_test_cli_mqtt_pub_test(void)
{
    int rc, i;
    MQTTClient_deliveryToken last_token;
    MQTTProperties pub_props = MQTTProperties_initializer;
    MQTTClient_init();
    if (MQTTClient_isConnected(paho_mqtt_client))
    {
        memset(paho_mqtt_at_param.topic, 0, sizeof(paho_mqtt_at_param.topic));
        sptr = nwy_test_cli_input_gets("\r\nPlease input topic: ");
        strncpy(paho_mqtt_at_param.topic, sptr, strlen(sptr));
        sptr = nwy_test_cli_input_gets("\r\nPlease input qos: ");
        paho_mqtt_at_param.qos = atoi(sptr);
        sptr = nwy_test_cli_input_gets("\r\nPlease input retained: ");
        paho_mqtt_at_param.retained = atoi(sptr);
        memset(paho_mqtt_at_param.message, 0, sizeof(paho_mqtt_at_param.message));
        sptr = nwy_test_cli_input_gets("\r\nPlease input 1k str,msg(10k) consists of 10 str");
        if (strlen(sptr) != 1024)
        {
            nwy_test_cli_echo("\r\nmust be 1k message");
            return;
        }
        for (i = 0; i < 10; i++)
            strncpy(paho_mqtt_at_param.message + i * strlen(sptr), sptr, strlen(sptr));
        nwy_test_cli_echo("\r\nmqttpub param retained = %d, qos = %d, topic = %s, strlen is %d",
                          paho_mqtt_at_param.retained,
                          paho_mqtt_at_param.qos,
                          paho_mqtt_at_param.topic,
                          strlen(paho_mqtt_at_param.message));
        rc = MQTTClient_publish(paho_mqtt_client, paho_mqtt_at_param.topic, strlen(paho_mqtt_at_param.message),
                        paho_mqtt_at_param.message, paho_mqtt_at_param.qos, paho_mqtt_at_param.retained, &last_token);
        nwy_test_cli_echo("\r\nmqtt publish rc:%d", rc);
    }
    else
        nwy_test_cli_echo("\r\nMQTT not connect");
}
#endif
#endif
