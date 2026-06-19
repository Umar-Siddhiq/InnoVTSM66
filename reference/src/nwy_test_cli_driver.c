#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_adpt.h"
#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"


#if 0
/**************************UART*********************************/
int hd = -1;
static uint8_t uart_mode = NWY_UART_MODE_AT;
extern unsigned int nwy_test_uart_name_tab[NWY_OPEN_MAX_UART_CHANNEL];
void nwy_test_cli_uart_init()
{
    char *opt;
    uint8_t port;
    uint32_t name;
    opt = nwy_test_cli_input_gets("\r\nPlease input the uart id(1-URT1, 2-URT2 or 3-URT3):");
    port = atoi(opt);
    if(port < 1 || port > NWY_OPEN_MAX_UART_CHANNEL)
    {
        nwy_test_cli_echo("\r\n Input UART id is invalid!");
        return;
    }
    name = nwy_test_uart_name_tab[port-1];
    if (name == NWY_OPEN_UART_CHANNEL_NS)
    {
        nwy_test_cli_echo("\r\n Input UART not supported!");
        return;
    }
    nwy_test_cli_echo("\r\nTest port = %d!\r\n", port); //delet

    opt = nwy_test_cli_input_gets("\r\nPlease input the uart mode(0-AT,1-DATA):");
    uart_mode = atoi(opt);
    nwy_test_cli_echo("\r\nTest mode = %d!\r\n", uart_mode); //delet

    hd = nwy_uart_init(name, (nwy_uart_mode_t)uart_mode);
    if (hd < 0)
        nwy_test_cli_echo("\r\nTest uart error!\r\n"); //delet
    nwy_test_cli_echo("\r\nTest uart success!\r\n");   //delet
}

void nwy_test_cli_uart_set_baud()
{
    char *opt;
    uint32_t baud = 0;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    opt = nwy_test_cli_input_gets("\r\nPlease input the uart baud:");
    baud = atoi(opt);
    nwy_test_cli_echo("\r\nTest baud = %d,hd = %d!\r\n", baud, hd); //delet

    if (1200 <= baud && 8000000 >= baud)
    {
        nwy_uart_set_baud(hd, baud);
        nwy_test_cli_echo("\r\nSet uart baud success!\r\n");
    }
    else
        nwy_test_cli_echo("\r\nSet uart baud param error!\r\n");
}

void nwy_test_cli_uart_get_baud()
{
    uint32_t baud = 0;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    nwy_uart_get_baud(hd, &baud);
    nwy_test_cli_echo("\r\nRead uart id = %d, baud = %d!\r\n", hd + 1, baud);
}

void nwy_test_cli_uart_set_para()
{
    nwy_uart_parity_t parity;
    nwy_uart_data_bits_t data_size;
    nwy_uart_stop_bits_t stop_size;
    int flow_ctrl = 0;
    char *opt;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    opt = nwy_test_cli_input_gets("\r\nTest in set the uart param:");

    switch (*opt++)
    {
    case 'p':
        nwy_uart_get_para(hd, &parity, &data_size, &stop_size, &flow_ctrl);
        parity = (nwy_uart_parity_t)atoi(opt);
        if (0 <= parity && 2 >= parity)
        {
            nwy_uart_set_para(hd, parity, data_size, stop_size, flow_ctrl);
            nwy_test_cli_echo("\r\nSwitch parity to:%d\r\n", parity);
        }
        else
            nwy_test_cli_echo("\r\nTest invalid parity:%d!\r\n", parity);
        break;

    case 'd':
        nwy_uart_get_para(hd, &parity, &data_size, &stop_size, &flow_ctrl);
        data_size = (nwy_uart_data_bits_t)atoi(opt);
        if (7 <= data_size && 8 >= data_size)
        {
            nwy_uart_set_para(hd, parity, data_size, stop_size, flow_ctrl);
            nwy_test_cli_echo("\r\nSwitch data_size to:%d\r\n", data_size);
        }
        else
            nwy_test_cli_echo("\r\nInvalid data_size:%d\r\n", data_size);
        break;

    case 's':
        nwy_uart_get_para(hd, &parity, &data_size, &stop_size, &flow_ctrl);
        stop_size = (nwy_uart_stop_bits_t)atoi(opt);
        if (1 <= stop_size && 2 >= stop_size)
        {
            nwy_uart_set_para(hd, parity, data_size, stop_size, flow_ctrl);
            nwy_test_cli_echo("\r\nSwitch stop_size to:%d\r\n", stop_size);
        }
        else
            nwy_test_cli_echo("\r\nInvalid stop_size:%d\r\n", stop_size);
        break;

    case 'f':
        nwy_uart_get_para(hd, &parity, &data_size, &stop_size, &flow_ctrl);
        flow_ctrl = atoi(opt);
        if (0 <= parity && 1 >= parity)
        {
            nwy_uart_set_para(hd, parity, data_size, stop_size, flow_ctrl);
            nwy_test_cli_echo("\r\nSwitch flow_ctrl to:%d\r\n", flow_ctrl);
        }
        else
            nwy_test_cli_echo("\r\nInvalid flow_ctrl:%d\r\n", flow_ctrl);
        break;

    default:
        break;
    }
}

void nwy_test_cli_uart_get_para()
{
    nwy_uart_parity_t parity;
    nwy_uart_data_bits_t data_size;
    nwy_uart_stop_bits_t stop_size;
    int flow_ctrl = 0;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    nwy_uart_get_para(hd, &parity, &data_size, &stop_size, &flow_ctrl);
    nwy_test_cli_echo("\r\nUart parity = %d\r\n", parity);
    nwy_test_cli_echo("\r\nUart data_size = %d\r\n", data_size);
    nwy_test_cli_echo("\r\nUart stop_size = %d\r\n", stop_size);
    nwy_test_cli_echo("\r\nUart flow_ctrl = %d\r\n", flow_ctrl);
}

void nwy_test_cli_uart_set_tout()
{
    int timeout;
    char *opt;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    opt = nwy_test_cli_input_gets("\r\nTest in set the uart receive timeout(default:32ms):");
    timeout = atoi(opt);

    nwy_set_rx_frame_timeout(hd, timeout);
    nwy_test_cli_echo("\r\nSet rx frame timeout = %d\r\n", timeout);
}

void nwy_test_cli_uart_send()
{
    char *opt;

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
	{
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    opt = nwy_test_cli_input_gets("\r\nTest in set the uart send data:");

    nwy_uart_send_data(hd, (uint8 *)opt, strlen(opt));
}

static void nwy_uart_recv_handle(const char *str, unsigned int length)
{
    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    nwy_uart_send_data(hd, (uint8 *)str, length);
    nwy_test_cli_echo("\r\nUart send data length = %d\r\n", length);
}

void nwy_test_cli_uart_reg_rx_cb()
{
    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    nwy_uart_reg_recv_cb(hd, nwy_uart_recv_handle);
}

/*if send completly, the callback func will set RS485 as rx state*/
static void nwy_rs485_direction_switch(int port, int value)
{
    nwy_gpio_set_direction(port, nwy_output);
    nwy_gpio_set_value(port, (nwy_value_t)value);
}
static void nwy_uart_send_complet_handle(int param)
{
    nwy_sleep(10);
    nwy_rs485_direction_switch(RS485_GPIO_PORT, RS485_DIR_RX);
    nwy_test_cli_echo("\r\nUart send complet handle success!\r\n");
}

void nwy_test_cli_uart_reg_tx_cb()
{
    char *pstsnd = "hellors485";

    if(nwy_test_cli_check_uart_mode(uart_mode))
    {
        nwy_test_cli_echo("\r\nUart Current mode is AT Mode!! Please input at by uart port!!\r\n");
        return;
    }

    if (hd < 0)
    {
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    nwy_rs485_direction_switch(RS485_GPIO_PORT, RS485_DIR_RX);

    /*register cb func to uart drv */
    nwy_uart_reg_tx_cb(hd, nwy_uart_send_complet_handle);

    /* for send, set RS485 as tx state */
    nwy_rs485_direction_switch(RS485_GPIO_PORT, RS485_DIR_TX);
    nwy_uart_send_data(hd, (uint8_t *)pstsnd, strlen(pstsnd));
}

void nwy_test_cli_uart_deinit()
{
    int close = 0;
    char *opt = NULL;

    if (hd < 0){
        nwy_test_cli_echo("\r\n Uart port is not inited!!!\r\n");
        return;
    }

    opt = nwy_test_cli_input_gets("\r\nSure to close this uart(0-no, 1-yes):");

    close = atoi(opt);
    if (close)
	{
        nwy_uart_deinit(hd);
        hd = -1;
    }
}
#endif

/**************************I2C*********************************/
#ifdef NWY_OPEN_TEST_I2C
int i2c_bus;
void nwy_test_cli_i2c_init()
{
    char *opt;
    uint8_t port;
    char *name;
    opt = nwy_test_cli_input_gets("\r\nPlease input the I2C id(2-I2C2 or 3-I2C3):");
    port = atoi(opt);

    if (port == 2)
    {
        name = NAME_I2C_BUS_2;
    }
    else if (port == 3)
    {
        name = NAME_I2C_BUS_3;
    }
    else
    {
        nwy_test_cli_echo("\r\n Input I2C id is invalid!");
        return;
    }

    i2c_bus = nwy_i2c_init(name, NWY_I2C_BPS_100K);
    if (NWY_SUCESS > i2c_bus)
    {
        nwy_test_cli_echo("\r\nI2c Error : bus:%s init fail\r\n", name);
        return;
    }
}

#define BMA400_DEV_ADDR 0x14
void nwy_test_cli_i2c_read()
{
    uint8_t rtn, sensor_id;
    rtn = nwy_i2c_read(i2c_bus, BMA400_DEV_ADDR, 0x00, &sensor_id, 1);
    if (NWY_SUCESS == rtn)
        nwy_test_cli_echo("\r\nNWY get sensor id = 0x%x!\r\n", sensor_id);
    else
        nwy_test_cli_echo("\r\nNWY read I2C error!\r\n");
}

void nwy_test_cli_i2c_write()
{
    uint8_t temp = 0x1c;
    uint8_t *data = &temp;
    uint8_t rtn;
    rtn = nwy_i2c_write(i2c_bus, BMA400_DEV_ADDR, 0xf4, data, 1);
    if (NWY_SUCESS == rtn)
        nwy_test_cli_echo("\r\nNWY write I2C success!\r\n");
    else
        nwy_test_cli_echo("\r\nNWY write I2C error!\r\n");
}

void nwy_test_cli_i2c_put_raw()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_i2c_get_raw()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_i2c_deinit()
{
    int close;
    char *opt;
    opt = nwy_test_cli_input_gets("\r\nSure to close this i2c(0-no, 1-yes):");

    close = atoi(opt);
    if (close)
        nwy_i2c_deinit(i2c_bus);
}
#endif

/**************************SPI*********************************/
#ifdef NWY_OPEN_TEST_SPI
int spi_bus = -1;
extern char* nwy_test_spi_name_tab[NWY_OPEN_MAX_SPI_CHANNEL];
void nwy_test_cli_spi_init()
{
    char *opt;
    uint8_t port;
    char *name;
    opt = nwy_test_cli_input_gets("\r\nPlease input the SPI id(1-spi1,2-spi2):");
    port = atoi(opt);
    if(port < 1 || port > NWY_OPEN_MAX_SPI_CHANNEL)
    {
        nwy_test_cli_echo("\r\n Input SPI id is invalid!");
        return;
    }
    name = nwy_test_spi_name_tab[port-1];
    if (name == NULL)
    {
        nwy_test_cli_echo("\r\n Input SPI not supported!");
        return;
    }

    spi_bus = nwy_spi_init(name, SPI_MODE_0, 1000000, 8);
    if (NWY_SUCESS > spi_bus)
    {
        nwy_test_cli_echo("\r\nSPI Error : bus:%s init fail\r\n", name);
        return;
    }
}

void nwy_test_cli_spi_trans()
{
#ifdef NWY_OPEN_TEST_SPI_CFG
    uint8_t OSI_ALIGNED(16) cmd[3] = {0x9F, 0, 0};

    nwy_spi_cs_cfg(spi_bus, SPI_CS_0, true);
    nwy_spi_write(spi_bus, cmd, 1);
    cmd[0] = 0x0;
    nwy_spi_read(spi_bus, cmd, 3);
    nwy_spi_cs_cfg(spi_bus, SPI_CS_0, false);
    nwy_test_cli_echo("\r\nSpi flash read id %02x,%02x,%02x\r\n", cmd[0], cmd[1], cmd[2]);
#else
    uint8 Command[4] = {w25x_jedecDeviceID, 0xFF, 0xFF, 0xFF};
    uint8 FlashId[4] = {0};
    int rtn = nwy_spi_transfer(spi_bus, SPI_CS_0, Command, FlashId, 4);

    if (SPI_EC_SUCESS == rtn)
        nwy_test_cli_echo("\r\nSpi flash read id %02x,%02x,%02x\r\n", FlashId[1], FlashId[2], FlashId[3]);
    else
        nwy_test_cli_echo("\r\nSpi error:transfer fail!\r\n");
#endif
}

void nwy_test_cli_spi_deinit()
{
    int close;
    char *opt;
    opt = nwy_test_cli_input_gets("\r\nSure to close this spi(0-no, 1-yes):");

    close = atoi(opt);
    if (close)
        nwy_spi_deinit(spi_bus);
    spi_bus = -1;
}

typedef struct
{
    int hd;
    nwy_osi_mutex_t *lock;
} nwy_flash_spi_t;
static nwy_flash_spi_t nwy_flash_spi_bus;
static nwy_spi_flash_t nwy_spi1_xm25 =
{
    .name = NWY_MAKE_TAG('X', 'M', '2', '5'),
    .block_size = 4096,
    .block_count = 4096,/* 2048:8M bytes 4096:16M bytes */
};
#define SPI_FLASH_MOUNT_POINT "/flash2"

void nwy_test_cli_spi_flash_mount(void)
{
    #ifdef NWY_OPEN_TEST_GENERAL_FLASH_MOUNT
    nwy_test_cli_echo("\r\n flash test nwy_spi_flash_mount_test start");
    int opt;
    int ret;
    static nwy_osi_mutex_t mutex;
    nwy_block_device_t * block_dev;
    nwy_flash_spi_bus.hd = 0;
    nwy_flash_spi_bus.lock = nwy_create_mutex(&mutex);
    nwy_spi1_xm25.priv = &nwy_flash_spi_bus;
    int mode = nwy_test_cli_input_gets("\r\input option:0-1.8Vflash,1-3Vflash:");
    opt = atoi(mode);
    if (opt == 0)
        nwy_subpower_switch(NWY_POWER_SD, 1, 1800);
    else if(opt == 1)
        nwy_subpower_switch(NWY_POWER_SD, 1, 3000);
    int test = nwy_test_cli_input_gets("\r\input option:1-init spi:");
    opt = atoi(test);
    if (opt)
        block_dev = nwy_vfs_block_device_create(&nwy_spi1_xm25,10000000);
    else 
        nwy_test_cli_echo("\r\n flash test end");
    if(block_dev == NULL)
    {
        nwy_test_cli_echo("\r\n flash test block_dev create fail");
        return;
    }
    while(1)
    {
        int mount = nwy_test_cli_input_gets("\r\input option:0-fs mount,1-fs format,2-fs write test,3-fs read test,4-fs free size,5-exit:");
        opt = atoi(mount);
        if(opt == 0)
        {
            nwy_test_cli_echo("\r\n flash test nwy_vfs_mount start");
            ret = nwy_vfs_mount(SPI_FLASH_MOUNT_POINT, block_dev);
            if(0 > ret)
            {
                nwy_test_cli_echo("\r\n flash test mount fail");
                ret = nwy_vfs_mkfs(block_dev);
                if(0 > ret)
                {
                    nwy_test_cli_echo("\r\n flash test format fail");
                    continue;
                }
                ret = nwy_vfs_mount(SPI_FLASH_MOUNT_POINT, block_dev);
                if(0 > ret)
                {
                    nwy_test_cli_echo("\r\n flash test remount fail");
                    continue;
                }
            }
            nwy_test_cli_echo("\r\n flash test nwy_vfs_mount success");
        }
        else if(opt == 1)
        {
            ret = nwy_vfs_mkfs(block_dev);
            nwy_test_cli_echo("\r\n flash test format :%d", ret);
            continue;
        }
        else if(opt == 2)
        {
            int i = 1;
            int len, size;
            char file_name[64] = {0};
            while(i)
            {
                memset(file_name, 0, sizeof(file_name));
                sprintf(file_name, "%s/%d", SPI_FLASH_MOUNT_POINT, i);
                int fd = nwy_sdk_fopen(file_name, NWY_CREAT | NWY_RDWR | NWY_TRUNC);
                if(fd < 0)
                {
                    nwy_test_cli_echo("\r\nfile open %s fail\r\n", file_name);
                    break;
                }
                else
                {
                    len = size = 0;
                    while(len < (4096 * i))
                    {
                        size = nwy_sdk_fwrite(fd, file_name, strlen(file_name));
                        if(size <= 0)
                        {
                            nwy_test_cli_echo("\r\nfile write %s fail\r\n", file_name);
                            nwy_sdk_fclose(fd);
                            break;
                        }
                        len += size;
                    }
                    nwy_sdk_fclose(fd);
                    if(len > 0)
                        nwy_test_cli_echo("\r\nfile write %s size %d success\r\n", file_name, len);
                    else
                        break;
                }
                i++;
                if(i > 10)
                    return;
                nwy_sleep(100);
            }
        }
        else if(opt == 3)
        {
            int i = 1;
            int len, size;
            char file_name[64] = {0};
            char read_data[64];
            while(i)
            {
                memset(file_name, 0, sizeof(file_name));
                sprintf(file_name, "%s/%d", SPI_FLASH_MOUNT_POINT, i);
                int fd = nwy_sdk_fopen(file_name, NWY_RDONLY);
                if(fd < 0)
                {
                    nwy_test_cli_echo("\r\nfile open %s fail\r\n", file_name);
                    break;
                }
                else
                {
                    len = size = 0;
                    while(len < (4096 * i))
                    {
                        memset(read_data, 0, sizeof(read_data));
                        size = nwy_sdk_fread(fd, read_data, strlen(file_name));
                        if(size <= 0)
                        {
                            nwy_test_cli_echo("\r\nfile read %s fail\r\n", file_name);
                            nwy_sdk_fclose(fd);
                            break;
                        }
                        if(memcmp(read_data, file_name, size))
                        {
                            nwy_test_cli_echo("\r\nfile read %s data compare fail, read %s dest %s\r\n", file_name, read_data, file_name);
                            nwy_sdk_fclose(fd);
                            break;
                        }
                        len += size;
                    }
                    nwy_sdk_fclose(fd);
                    if(len > 0)
                        nwy_test_cli_echo("\r\nfile read %s size %d success\r\n", file_name, len);
                    else
                        break;
                }
                i++;
                nwy_sleep(100);
            }
        }
        else if(opt == 4)
        {
            int free_size = nwy_sdk_vfs_free_size(SPI_FLASH_MOUNT_POINT);
            nwy_test_cli_echo("\r\%s free size:%d\r\n", SPI_FLASH_MOUNT_POINT, free_size);
        }
        else if(opt == 5)
            return;
    }
    nwy_test_cli_echo("\r\n flash test end");

#else
    nwy_test_cli_echo("\r\n flash test not support");
#endif
}
#endif
/**************************GPIO*********************************/
void nwy_test_cli_gpio_set_val()
{
    char *opt;
    uint8_t port, vol;
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO id:");
    port = atoi(opt);
    opt = nwy_test_cli_input_gets("\r\nSet the gpio value(0-low level,1-high level):");
    vol = atoi(opt);

    nwy_gpio_set_value(port, (nwy_value_t)vol);
}

void nwy_test_cli_gpio_get_val()
{
    char *opt;
    uint32_t port, vol;
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO id:");
    port = atoi(opt);

    vol = nwy_gpio_get_value(port);
    nwy_test_cli_echo("\r\nGet the GPIO value = %d\r\n", vol);
}

void nwy_test_cli_gpio_set_dirt()
{
    char *opt;
    uint8_t port, dir;
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO id:");
    port = atoi(opt);
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO dir(0-input,1-output):");
    dir = atoi(opt);

    nwy_gpio_set_direction(port, (nwy_dir_mode_t)dir);
}

void nwy_test_cli_gpio_get_dirt()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void _gpioisropen(int param)
{
    nwy_open_sdk_log("nwy gpio isr ing %d",param);
}

void nwy_test_cli_gpio_config_irq()
{
    char *opt;
    uint8_t port, mode;
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO id:");
    port = atoi(opt);
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO irq mode(0-rising,2-rising&falling,3-high):");
    mode = atoi(opt);

    nwy_close_gpio(port);
    int data = nwy_open_gpio_irq_config(port, (nwy_irq_mode_t)mode, _gpioisropen);

    if (data)
    {
        nwy_test_cli_echo("\r\nGpio isr config success!\r\n");
    }
    else
    {
        nwy_test_cli_echo("\r\nGpio isr config failed!\r\n");
    }
}

void nwy_test_cli_gpio_enable_irq()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_gpio_disable_irq()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_gpio_close()
{
    char *opt;
    uint8_t port;
    opt = nwy_test_cli_input_gets("\r\nSet the GPIO id:");
    port = atoi(opt);

    nwy_close_gpio(port);
}

/**************************ADC*********************************/
#ifdef NWY_OPEN_TEST_ADC
void nwy_test_cli_adc_read()
{
    char *opt;
    uint8_t port, mode;
    uint32_t adc_vol;
    opt = nwy_test_cli_input_gets("\r\nChoose the ADC channel(0-CHANNEL0,1-CHANNEL1,2-VBAT):");
    port = atoi(opt);
    opt = nwy_test_cli_input_gets("\r\nChoose the ADC scale(0-1V250,1-2V444,2-3V233,3-5V000):");
    mode = atoi(opt);

    adc_vol = nwy_adc_get((nwy_adc_t)port, (nwy_adc_aux_scale_t)mode);
    nwy_test_cli_echo("\r\nAdc get value = %d\r\n", adc_vol);
}
#endif

/**************************RTC*********************************/
#ifdef NWY_OPEN_TEST_RTC
void nwy_test_cli_rtc_read()
{
    char *opt;
    int ret = -1;
    uint32_t sec;
    opt = nwy_test_cli_input_gets("\r\nSet the rtc value(sec):");
    sec = atoi(opt);

    ret = nwy_set_rtc_alarm(sec);
    nwy_test_cli_echo("\r\RTC set return = %d\r\n", ret);
}
#endif

#ifdef NWY_OPEN_TEST_LCD
#define LCD_DataWrite_GC9307(Data)  nwy_lcd_bus_write_data(Data)

#define LCD_CtrlWrite_GC9307(Cmd)  nwy_lcd_bus_write_cmd(Cmd)
/**************************************************************************************/
// Description: initialize all LCD with LCDC MCU MODE and LCDC mcu mode
/**************************************************************************************/
#define WIDTH 240
#define HEIGHT 320
static unsigned char display_buf[WIDTH*HEIGHT*2];
void nwy_write_data_buf(void *buf, unsigned int len)
{
    int i;
    unsigned char *data = (unsigned char *)buf;
    for (i = 0; i < len; i++)
        LCD_DataWrite_GC9307(data[i]);
    //nwy_test_cli_echo("nwy_write_data_buf!\n");
}
void nwy_lcd_block_write(unsigned short startx, unsigned short starty, unsigned short endx, unsigned short endy)
{
    uint8_t buf[4];
    LCD_CtrlWrite_GC9307(0x2a);
    buf[0] = startx >> 8;
    buf[1] = startx & 0xFF;
    buf[2] = endx >> 8;
    buf[3] = endx & 0xFF;
    nwy_write_data_buf((uint8_t *)buf, 4);
    LCD_CtrlWrite_GC9307(0x2B);
    buf[0] = starty >> 8;
    buf[1] = starty & 0xFF;
    buf[2] = endy >> 8;
    buf[3] = endy & 0xFF;
    nwy_write_data_buf((uint8_t *)buf, 4);
    LCD_CtrlWrite_GC9307(0x2C);
    //nwy_test_cli_echo("nwy LCD block write!\n");
}
static inline void prvFillBufferWhiteScreen(uint8_t *buffer, unsigned width, unsigned height, unsigned char color)
{
    memset(buffer, color, 2 * width * height * sizeof(uint8_t));
}
static void prvLcdClear(unsigned char color)
{
    prvFillBufferWhiteScreen(display_buf, WIDTH, HEIGHT, color);
    nwy_lcd_block_write(0, 0, WIDTH - 1, HEIGHT - 1);
    nwy_write_data_buf(display_buf, WIDTH * HEIGHT * 2);
}
static void gc9307_init(void)
{
    nwy_test_cli_echo("\r\n gc9307 lcd init!\r\n");
    LCD_CtrlWrite_GC9307(0xfe);
    LCD_CtrlWrite_GC9307(0xef);
    LCD_CtrlWrite_GC9307(0x36);
    LCD_DataWrite_GC9307(0x48);
    LCD_CtrlWrite_GC9307(0x3a);
    LCD_DataWrite_GC9307(0x05);
    LCD_CtrlWrite_GC9307(0x86);
    LCD_DataWrite_GC9307(0x98);
    LCD_CtrlWrite_GC9307(0x89);
    LCD_DataWrite_GC9307(0x13);
    LCD_CtrlWrite_GC9307(0x8b);
    LCD_DataWrite_GC9307(0x80);
    LCD_CtrlWrite_GC9307(0x8d);
    LCD_DataWrite_GC9307(0x33);
    LCD_CtrlWrite_GC9307(0x8e);
    LCD_DataWrite_GC9307(0x0f);
    LCD_CtrlWrite_GC9307(0xe8);
    LCD_DataWrite_GC9307(0x12);
    LCD_DataWrite_GC9307(0x00);
    LCD_CtrlWrite_GC9307(0xec);
    LCD_DataWrite_GC9307(0x13);
    LCD_DataWrite_GC9307(0x02);
    LCD_DataWrite_GC9307(0x88);
    LCD_CtrlWrite_GC9307(0xff);
    LCD_DataWrite_GC9307(0x62);
    LCD_CtrlWrite_GC9307(0x99);
    LCD_DataWrite_GC9307(0x3e);
    LCD_CtrlWrite_GC9307(0x9d);
    LCD_DataWrite_GC9307(0x4b);
    LCD_CtrlWrite_GC9307(0x98);
    LCD_DataWrite_GC9307(0x3e);
    LCD_CtrlWrite_GC9307(0x9c);
    LCD_DataWrite_GC9307(0x4b);
    LCD_CtrlWrite_GC9307(0xc3);
    LCD_DataWrite_GC9307(0x27);
    LCD_CtrlWrite_GC9307(0xc4);
    LCD_DataWrite_GC9307(0x18);
    LCD_CtrlWrite_GC9307(0xc9);
    LCD_DataWrite_GC9307(0x0a);
    LCD_CtrlWrite_GC9307(0xf0);
    LCD_DataWrite_GC9307(0x85);
    LCD_DataWrite_GC9307(0x0a);
    LCD_DataWrite_GC9307(0x09);
    LCD_DataWrite_GC9307(0x08);
    LCD_DataWrite_GC9307(0x04);
    LCD_DataWrite_GC9307(0x30);
    LCD_CtrlWrite_GC9307(0xf2);
    LCD_DataWrite_GC9307(0x85);
    LCD_DataWrite_GC9307(0x0a);
    LCD_DataWrite_GC9307(0x09);
    LCD_DataWrite_GC9307(0x08);
    LCD_DataWrite_GC9307(0x04);
    LCD_DataWrite_GC9307(0x30);
    LCD_CtrlWrite_GC9307(0xf1);
    LCD_DataWrite_GC9307(0x47);
    LCD_DataWrite_GC9307(0x5b);
    LCD_DataWrite_GC9307(0xb0);
    LCD_DataWrite_GC9307(0x3a);
    LCD_DataWrite_GC9307(0x3e);
    LCD_DataWrite_GC9307(0x7f);
    LCD_CtrlWrite_GC9307(0xf3);
    LCD_DataWrite_GC9307(0x47);
    LCD_DataWrite_GC9307(0x5b);
    LCD_DataWrite_GC9307(0xb0);
    LCD_DataWrite_GC9307(0x3a);
    LCD_DataWrite_GC9307(0x3f);
    LCD_DataWrite_GC9307(0x7f);
    LCD_CtrlWrite_GC9307(0x35);
    LCD_DataWrite_GC9307(0x00);
    LCD_CtrlWrite_GC9307(0x44);
    LCD_DataWrite_GC9307(0x00);
    LCD_DataWrite_GC9307(0x0a);
#if 0
    LCD_CtrlWrite_GC9307(0x8a);
    LCD_DataWrite_GC9307(0xff);
    LCD_CtrlWrite_GC9307(0xf7);
    LCD_DataWrite_GC9307(0x20);
    LCD_DataWrite_GC9307(0x3f);
    LCD_DataWrite_GC9307(0x00);
    LCD_DataWrite_GC9307(0x00);
    nwy_sleep(100);
    LCD_CtrlWrite_GC9307(0xf7);
    LCD_DataWrite_GC9307(0x20);
    LCD_DataWrite_GC9307(0x00);
    LCD_DataWrite_GC9307(0x3f);
    LCD_DataWrite_GC9307(0x00);
    nwy_sleep(100);
    LCD_CtrlWrite_GC9307(0xf7);
    LCD_DataWrite_GC9307(0x20);
    LCD_DataWrite_GC9307(0x00);
    LCD_DataWrite_GC9307(0x00);
    LCD_DataWrite_GC9307(0x3f);
    nwy_sleep(100);
#endif
    LCD_CtrlWrite_GC9307(0x11);
    nwy_sleep(120);
    LCD_CtrlWrite_GC9307(0x29);
    LCD_CtrlWrite_GC9307(0x2c);
}
#define LCD_BUS_CLK_FREQ (35000000)
#define LCD_BUS_FREQ_READ (5000000)
static bool lcd_init = false;
void nwy_lcd_init(void)
{
    if (lcd_init)
        return;
    nwy_lcd_bus_config_t lcd_bus_config =
        {
            .cs = NWY_LCD_BUS_CS_0,
            .cs0Polarity = false,
            .cs1Polarity = false,
            .resetb = true,
            .rsPolarity = false,
            .wrPolarity = false,
            .rdPolarity = false,
            .highByte = false,
            .clk = LCD_BUS_CLK_FREQ,
            .spiLineType = NWY_LCD_SPI_LINE_3,
        };
    nwy_lcd_bus_init(&lcd_bus_config);
    nwy_sleep(50);
    gc9307_init();
    nwy_sleep(32);
    nwy_lcd_bus_brightness(6);
#if 1
    prvLcdClear(0x00);
    prvLcdClear(0x11);
    prvLcdClear(0x22);
    prvLcdClear(0x33);
    prvLcdClear(0x44);
    prvLcdClear(0x55);
    prvLcdClear(0x66);
    prvLcdClear(0x77);
    prvLcdClear(0x88);
    prvLcdClear(0x99);
    prvLcdClear(0xAA);
    prvLcdClear(0xBB);
    prvLcdClear(0xCC);
    prvLcdClear(0xDD);
    prvLcdClear(0xEE);
    prvLcdClear(0xFF);
#endif
    lcd_init = true;
}
void nwy_lcd_deinit(void)
{
    nwy_lcd_bus_brightness(0);
    nwy_lcd_bus_deinit();
    lcd_init = false;
}
void nwy_test_cli_lcd_open()
{
    nwy_lcd_init();
    nwy_test_cli_echo("\r\nlcd open success!\r\n");
}
void nwy_test_cli_lcd_close()
{
    nwy_lcd_deinit();
    nwy_test_cli_echo("\r\nlcd close success!\r\n");
}
void nwy_test_cli_lcd_set_bl_level()
{
    char *sptr;
    int level = 0;
    sptr = nwy_test_cli_input_gets("\r\n Please input light level[0~7]:");
    level = atoi(sptr);
    nwy_lcd_bus_brightness(level);
    nwy_test_cli_echo("\r\nlcd backlight set success!\r\n");
}

#if 0
void nwy_test_cli_get_lcd_id()
{
    unsigned char productIds[4];
    unsigned short productId = 0;
    nwy_lcd_bus_config_t lcd_bus_config =
    {
            .cs = NWY_LCD_BUS_CS_0,
            .cs0Polarity = false,
            .cs1Polarity = false,
            .resetb = true,
            .rsPolarity = false,
            .wrPolarity = false,
            .rdPolarity = false,
            .highByte = false,
            .clk = LCD_BUS_FREQ_READ,
            .spiLineType = NWY_LCD_SPI_LINE_3,
    };
    nwy_lcd_bus_init(&lcd_bus_config);
    nwy_sleep(50);
    nwy_lcd_bus_read_datas(0x04, productIds, 4);
    productId = ((unsigned short)(productIds[2]) << 8 & 0xff00) | ((unsigned short)productIds[1] & 0x0ff);
    nwy_test_cli_echo("get productIds[0] is 0x%x \n", productIds[0]);
    nwy_test_cli_echo("get productIds[1] is 0x%x \n", productIds[1]);
    nwy_test_cli_echo("get productIds[2] is 0x%x \n", productIds[2]);
    nwy_test_cli_echo("get productIds[3] is 0x%x \n", productIds[3]);
    nwy_test_cli_echo("get lcd id is 0x%x \n", productId);
    if (productId  != 0x9307)
    {
        nwy_test_cli_echo("get lcd id error\n");
    }
    nwy_sleep(100);
    nwy_lcd_bus_deinit();
    return;
}
#endif
#endif
#ifdef NWY_OPEN_TEST_CAMERA
unsigned short *buff = NULL;
void nwy_test_cli_camera_open()
{
	int ret ;
	nwy_cam_info_t cam_info = 
		{
			.img_width = 640,
			.img_height = 480,
			.img_pixel = NWY_CAM_NPIX_VGA,
			.img_format = NWY_CAM_FORMAT_YUV,
		};
	ret = nwy_camera_open(cam_info);
	if (ret < 0)
	{
		nwy_test_cli_echo("\r\n camera open failed!\r\n");
	}
	else
		nwy_test_cli_echo("\r\n camera open success!\r\n");
}
void nwy_test_cli_camera_close()
{
    nwy_camera_close();
    nwy_test_cli_echo("\r\n camera close success!\r\n");
}
void nwy_test_cli_camera_get_preview()
{
	nwy_camera_get_preview(&buff);
	if (buff == NULL)
	{
		nwy_test_cli_echo("\r\n buff is NULL\r\n");
	}
	nwy_test_cli_echo("\r\n camera preview success!\r\n");
}
void nwy_test_cli_camera_capture()
{
	int ret = -1;
	int save_mode = 0;
	int image_format = 0;
	char image_path[128] = {0};
	char *sptr;
	sptr = nwy_test_cli_input_gets("\r\n choose capture save mode: 0:fs  1:sd");
	save_mode = atoi(sptr);
	image_format = YUV_FORMAT;
	//save_mode = SD_MODE;
	strcpy(image_path, "/nwy/cam.yuv");
	ret = nwy_camera_capture_image(image_format, save_mode, image_path);
	if(0 != ret)
	{
		nwy_test_cli_echo("\r\n test camera capture yuv failed\r\n");
	}
	else
	{
		nwy_test_cli_echo("\r\n test camera capture yuv success\r\n");
	}
}
#endif
#if 0
/**************************PM*********************************/
void nwy_test_cli_pm_save_md()
{
#ifdef NWY_OPEN_TEST_PM_SAVE_NS
      nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt;
    uint8_t mode;
    opt = nwy_test_cli_input_gets("\r\nChoose the powersave mode(0-WAKEUP,1-ENTER SLEEP):");
    mode = atoi(opt);
    nwy_pm_state_set(mode);
#endif
}

void nwy_test_cli_pm_get_pwr_st()
{
#ifdef NWY_OPEN_TEST_PWR_ST_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    int pwr_st = nwy_power_state();
    if (NWY_POWER_NORMAL_STATE == pwr_st)
        nwy_test_cli_echo("\r\nPower state in normal!\r\n");
    else
        nwy_test_cli_echo("\r\nPower state in drop!\r\n");
#endif
}
#endif
void nwy_test_cli_pm_pwr_off()
{
    char *opt;
    uint8_t mode;
    opt = nwy_test_cli_input_gets("\r\nChoose the power off mode(0-quickly,1-normal,2-reset):");
    mode = atoi(opt);
    nwy_power_off(mode);
}
void nwy_test_cli_pm_pwrkey_long_press_time_pwr_off()
{
    char *opt;
    uint8_t mode;
    opt = nwy_test_cli_input_gets("\r\nPlease input long press time(ms):");
    mode = atoi(opt);
    nwy_set_pwr_key_long_press_time(mode);
}
#if 0
void nwy_test_cli_pm_set_dtr()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_pm_pwr_key()
{
#ifdef NWY_OPEN_TEST_PWR_KEY_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt;
    uint8_t mode;
    opt = nwy_test_cli_input_gets("\r\nPlease input status(0-close,1(default)-open:");
    mode = atoi(opt);

    nwy_powerkey_poweroff_ctrl(mode);
    nwy_test_cli_echo("\r\nClose key poweroff test in %d\r\n", mode);
#endif
}

void nwy_test_cli_pm_switch_sub_pwr()
{
#ifdef NWY_OPEN_TEST_PM_SUB_PWR_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt;
    uint8_t power_id, mode;
    opt = nwy_test_cli_input_gets("\r\nPlease input sub power id:");
    power_id = atoi(opt);

    nwy_subpower_switch(power_id, true, false);
    nwy_test_cli_echo("\r\nSet the power %d open!\r\n", power_id);

    opt = nwy_test_cli_input_gets("\r\nPlease sure to close this sub power:");
    mode = atoi(opt);

    if (mode)
    {
        nwy_subpower_switch(power_id, false, false);
        nwy_test_cli_echo("\r\nSet the power %d closed!\r\n", power_id);
    }

    nwy_test_cli_echo("\r\nSet the power %d already open!\r\n", power_id);
#endif
}

void nwy_test_cli_pm_set_sub_pwr()
{
#ifdef NWY_OPEN_TEST_PM_SUB_PWR_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt;
    uint8_t power_id, level;
    opt = nwy_test_cli_input_gets("\r\nPlease input sub power id:");
    power_id = atoi(opt);

    opt = nwy_test_cli_input_gets("\r\nPlease set the sub power level you want(mV):");
    level = atoi(opt);

    nwy_set_pmu_power_level(power_id, level);
    nwy_test_cli_echo("\r\nSet the power %d in %d mV!\r\n", power_id, level);
#endif
}

#ifndef NWY_OPEN_TEST_PM_AUTO_OFF_NS
void nwy_shutdown_cb(NWY_SVR_MSG_SERVICE_E msg, uint32_t param)
{
    if (msg == NWY_WARNING_IND)
        NWY_CLI_LOG("nwy the capacity is low, should charge");
    else if (msg == NWY_SHUTDOWN_IND)
        NWY_CLI_LOG("nwy the capacity is very low and must shutdown");
}
#endif

void nwy_test_cli_pm_set_auto_off()
{
#ifdef NWY_OPEN_TEST_PM_AUTO_OFF_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *opt;
    uint16_t shut_vol, dead_vol, count;
    opt = nwy_test_cli_input_gets("\r\nPlease input shutdown vol:");
    shut_vol = atoi(opt);

    opt = nwy_test_cli_input_gets("\r\nPlease input deadline vol:");
    dead_vol = atoi(opt);

    opt = nwy_test_cli_input_gets("\r\nPlease input count:");
    count = atoi(opt);
    nwy_set_auto_poweroff(shut_vol, dead_vol, count, nwy_shutdown_cb);
#endif
}

#ifndef NWY_OPEN_TEST_PM_CHARGER_NS
void nwy_charging_cb(NWY_SVR_MSG_SERVICE_E msg, uint32_t param)
{
    if (msg == NWY_CHARGE_START_IND)
        NWY_CLI_LOG("nwy start chargering");
    else if (msg == NWY_CHARGE_DISCONNECT)
        NWY_CLI_LOG("nwy disconnect chargering");
    else if (msg == NWY_CHARGE_FINISH)
        NWY_CLI_LOG("nwy charger finished");
}
#endif

void nwy_test_cli_pm_reg_charger_cb()
{
#ifdef NWY_OPEN_TEST_PM_CHARGER_NS
      nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_chargering_instructions(nwy_charging_cb);
    nwy_test_cli_echo("\r\nTest in chager cb!\r\n");
#endif
}

/**************************KEYPAD*********************************/
#ifdef NWY_OPEN_TEST_KEYPAD
static void _openkeypad(nwy_key_t key, nwy_keyState_t evt)
{
    uint8_t status = 0;
    if (evt & key_state_press)
        status = 1;
    if (evt & key_state_release)
        status = 0;

    if (evt == key_state_press)
    {
        nwy_test_cli_echo("\r\nThis key%d is press\r\n", key);
    }
    else
    {
        nwy_test_cli_echo("\r\n key id %d,status %d", key, status);
    }
}

void nwy_test_cli_keypad_reg_cb()
{
    nwy_test_cli_echo("\r\nTest in keypad cb!\r\n");
    reg_nwy_key_cb(_openkeypad);
}

void nwy_test_cli_keypad_set_debouce()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}
#endif
/**************************PWM*********************************/
#ifdef NWY_OPEN_TEST_PWM
nwy_pwm_t *test_p;
void nwy_test_cli_pwm_init()
{
    test_p = nwy_pwm_init(NAME_PWM_1, 100, 40);

    if (test_p == NULL)
    {
        nwy_test_cli_echo("\r\nPWM init failed!\r\n");
    }
    nwy_test_cli_echo("\r\nPWM init success!\r\n");
}

void nwy_test_cli_pwm_start()
{
    nwy_pwm_start(test_p);
    nwy_test_cli_echo("\r\nTest in pwm start!\r\n");
}

void nwy_test_cli_pwm_stop()
{
    nwy_pwm_stop(test_p);
    nwy_test_cli_echo("\r\nTest in pwm stop!\r\n");
}

void nwy_test_cli_pwm_deinit()
{
    nwy_pwm_deinit(test_p);
    nwy_test_cli_echo("\r\nTest in pwm deinit!\r\n");
}
#endif
/**************************LCD*********************************/
#if 0

#define ROW 128
#define COL 128

#define WIDTH 128
#define HEIGHT 128

#define ASC12_FILE_NAME "/ASC12"
#define ASC16_FILE_NAME "/ASC16"
#define HZK12_FILE_NAME "/HZK12"
#define HZK16_FILE_NAME "/HZK16"


static unsigned short display_buf[WIDTH * HEIGHT];

static void GetFontSize(unsigned int fontSize, unsigned int *GBK_W,
                        unsigned int *GBK_H, unsigned int *ASC_W, unsigned int *ASC_H)
{
    if (fontSize == 12)
    {
        if (GBK_W != NULL)
            *GBK_W = 12;
        if (GBK_H != NULL)
            *GBK_H = 12;
        if (ASC_W != NULL)
            *ASC_W = 6;
        if (ASC_H != NULL)
            *ASC_H = 12;
    }
    else if (fontSize == 16)
    {
        if (GBK_W != NULL)
            *GBK_W = 16;
        if (GBK_H != NULL)
            *GBK_H = 16;
        if (ASC_W != NULL)
            *ASC_W = 8;
        if (ASC_H != NULL)
            *ASC_H = 16;
    }
}

static void GetGbkOneBuf(unsigned int fontSize, unsigned char *gbk, unsigned short *gbk_buf, unsigned short TextColor, unsigned short BackColor)
{
    int qh;
    int wh;
    int offset = 0;
    int i, j, k;
    int flag;
    int n = 0;

    unsigned int GBK_W = 0;
    unsigned int GBK_H = 0;
    unsigned int WORD_SIZE = 0;
    char *filename = NULL;

    GetFontSize(fontSize, &GBK_W, &GBK_H, NULL, NULL);

    unsigned char buf[100] = {0};
    unsigned char key[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

    qh = (int)(gbk[0] - 0xa0);
    wh = (int)(gbk[1] - 0xa0);

    if (fontSize == 12)
    {
        filename = HZK12_FILE_NAME;
        WORD_SIZE = 24;
        offset = (int)(94 * (qh - 1) + (wh - 1)) * WORD_SIZE;
    }
    else if (fontSize == 16)
    {
        filename = HZK16_FILE_NAME;
        WORD_SIZE = 32;
        offset = (int)(94 * (qh - 1) + (wh - 1)) * WORD_SIZE;
    }
    else
    {
        NWY_CLI_LOG("unsupport font size:%d", fontSize);
        return;
    }

    int fp = nwy_sdk_fopen(filename, NWY_RDONLY);
    if (0 > fp)
    {
        NWY_CLI_LOG("%s open fail\n", filename);
        return;
    }

    nwy_sdk_fseek(fp, offset, NWY_SEEK_SET);
    int read_len = nwy_sdk_fread(fp, buf, WORD_SIZE);

    if (read_len != WORD_SIZE)
    {
        NWY_CLI_LOG("%s read fail read_len=%d\n", filename, read_len);
    }

    nwy_sdk_fclose(fp);

    int total_word_size = WORD_SIZE / GBK_H;
    for (k = 0; k < GBK_H; k++)
    {
        for (j = 0; j < total_word_size; j++)
        {
            for (i = 0; i < 8; i++)
            {
                if (j * 8 + i >= GBK_W)
                    break;
                flag = buf[k * total_word_size + j] & key[i];
                if (flag)
                {
                    gbk_buf[n++] = TextColor;
                }
                else
                {
                    gbk_buf[n++] = BackColor;
                }
            }
        }
    }

    //LCD_BlockWrite(0,0, COL-1,ROW-1);
}

static void GetOneCharBuf(unsigned int fontSize, unsigned char ord, unsigned short *char_buf, unsigned short TextColor, unsigned short BackColor) // ord:0~95
{
    unsigned char i, j, k;
    unsigned char dat;
    int n = 0;

    unsigned int ASC_W = 0;
    unsigned int ASC_H = 0;
    unsigned int WORD_SIZE = 0;
    char *filename = NULL;
    unsigned char buf[100] = {0};

    GetFontSize(fontSize, NULL, NULL, &ASC_W, &ASC_H);

    if (fontSize == 12)
    {
        filename = ASC12_FILE_NAME;
        WORD_SIZE = 12;
    }
    else if (fontSize == 16)
    {
        filename = ASC16_FILE_NAME;
        WORD_SIZE = 16;
    }
    else
    {
        NWY_CLI_LOG("unsupport font size:%d", fontSize);
        return;
    }

    int fp = nwy_sdk_fopen(filename, NWY_RDONLY);
    if (0 > fp)
    {
        NWY_CLI_LOG("%s open fail\n", filename);
        return;
    }

    nwy_sdk_fseek(fp, ord * WORD_SIZE, NWY_SEEK_SET);
    int read_len = nwy_sdk_fread(fp, buf, WORD_SIZE);
    if (read_len != WORD_SIZE)
    {
        NWY_CLI_LOG("ASC16 read fail read_len=%d\n", read_len);
    }

    nwy_sdk_fclose(fp);

    int total_word_size = WORD_SIZE / ASC_H;

    for (k = 0; k < ASC_H; k++)
    {
        for (j = 0; j < total_word_size; j++)
        {
            dat = buf[k * total_word_size + j];
            for (i = 0; i < 8; i++)
            {
                if (j * 8 + i >= ASC_W)
                    break;
                if ((dat << i) & 0x80)
                {
                    char_buf[n++] = TextColor;
                }
                else
                {
                    char_buf[n++] = BackColor;
                }
            }
        }
    }
}

void nwy_dispstrline(unsigned int fontSize, unsigned char *str, unsigned int Xstart, unsigned int Ystart, unsigned short TextColor, unsigned short BackColor)
{
    int i, j;
    static unsigned short tmp_buf[256];
    int line_len;
    int str_index = 0;
    unsigned int Xend = Xstart;
    unsigned int GBK_W = 0;
    unsigned int GBK_H = 0;
    unsigned int ASC_W = 0;
    unsigned int ASC_H = 0;

    GetFontSize(fontSize, &GBK_W, &GBK_H, &ASC_W, &ASC_H);
    line_len = strlen((char *)str) * ASC_W;

    if (line_len + Xstart > WIDTH)
    {
        NWY_CLI_LOG("ERROR:::: line_len is too lone %d\r\n", line_len);
        return;
    }

    while (!(*str == '\0'))
    {
        if (*str > 0x80)
        {
            GetGbkOneBuf(fontSize, str, tmp_buf, TextColor, BackColor);
            for (i = 0; i < GBK_H; i++)
            {
                for (j = 0; j < GBK_W; j++)
                {
                    display_buf[i * line_len + str_index + j] = tmp_buf[i * GBK_W + j];
                }
            }
            str_index += GBK_W;

            if (Xstart > ((COL)-GBK_W))
            {
                Xstart = (COL)-GBK_W;
            }
            else
            {
                Xend = Xend + GBK_W;
            }

            if (Ystart > ((ROW)-GBK_H))
            {
                break;
            }

            str += 2;
        }
        else
        {
            GetOneCharBuf(fontSize, *str++, tmp_buf, TextColor, BackColor);
            for (i = 0; i < ASC_H; i++)
            {
                for (j = 0; j < ASC_W; j++)
                {
                    display_buf[i * line_len + str_index + j] = tmp_buf[i * ASC_W + j];
                }
            }
            str_index += ASC_W;

            if (Xstart > ((COL)-ASC_W * 2))
            {
                Xstart = (COL)-ASC_W * 2;
            }
            else
            {
                Xend = Xend + ASC_H;
            }

            if (Ystart > ((ROW)-ASC_H))
            {
                break;
            }
        }
    }

    nwy_lcd_block_write(Xstart, Ystart, Xstart + line_len - 1, Ystart + (GBK_H - 1));
    nwy_write_data_buf(display_buf, line_len * GBK_H * 2);
}

/**************************************************************************************/
// Description: initialize all LCD with LCDC MCU MODE and LCDC mcu mode
/**************************************************************************************/

#define BLACK 0x0000
#define NAVY 0x000F
#define DGREEN 0x03E0
#define DCYAN 0x03EF
#define MAROON 0x7800
#define PURPLE 0x780F
#define OLIVE 0x7BE0
#define LGRAY 0xC618
#define DGRAY 0x7BEF
#define BLUE 0x001F
#define GREEN 0x07E0
#define CYAN 0x07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

void nwy_test_cli_lcd_draw_line()
{
    int i;
    nwy_lcd_block_write(0, 96, 127, 96);
    for (i = 0; i < 128; i++)
    {
        display_buffer[i] = OLIVE;
    }
    nwy_write_data_buf(display_buffer, 128 * 2);
    nwy_test_cli_echo("\r\nlcd draw line success!\r\n");
}

/* GB18030 chinese code */
static const char chinese_string[] = {
    0xd3, 0xd0, /* ÓÐ */
    0xb7, 0xbd, /* ·½ */
    0xba, 0xba, /* ºº */
    0xd7, 0xd6, /* ×Ö */
    0xd1, 0xdd, /* ÑÝ */
    0xca, 0xbe, /* Ê¾ */
    '\0'};

#define MK_COLOR(r, g, b) ((((r)&0x1f) << 11) + (((g)&0x3f) << 5) + (((b)&0x1f) << 0))
#define COLOR_WHITE MK_COLOR(0xff, 0xff, 0xff)
#define COLOR_BLACK MK_COLOR(0, 0, 0)

void nwy_test_cli_lcd_draw_chinese()
{
    nwy_lcd_block_write(16, 19, 16 + 16 * 6, 34);
    nwy_dispstrline(16, (unsigned char *)chinese_string, 16, 19, BLACK, MAGENTA);
    nwy_test_cli_echo("\r\nlcd draw chinese success!\r\n");
}

#endif
#endif
/**************************SD*********************************/
void nwy_test_cli_sd_get_st()
{
    nwy_test_cli_echo("\r\nsd state:%d\r\n", nwy_read_sdcart_status());
}
/**************************SD*********************************/
void nwy_test_cli_sd_mnt()
{
    nwy_test_cli_echo("\r\nsd mount:%d\r\n", nwy_sdk_sdcard_mount());
}

void nwy_test_cli_sd_mkfs()
{
    nwy_format_sdcard();
    nwy_test_cli_echo("\r\nsd mkfs success\r\n");
}

/**************************FOTA*********************************/
typedef struct
{
    int need_rev_len;
    int rev_len;
    int proceed;
}NWY_APP_DATA;
uint8 nwy_app_update_flag = 0;
static NWY_APP_DATA nwy_app_info;
static uint8 *nwy_app = NULL;

void nwy_ext_app_info_proc(const uint8 *data, int length)
{
    memcpy((nwy_app + nwy_app_info.rev_len), data, length);
    nwy_app_info.rev_len = nwy_app_info.rev_len + length;
    if((nwy_app_info.rev_len * 10 / nwy_app_info.need_rev_len) != nwy_app_info.proceed)
    {
        nwy_app_info.proceed = nwy_app_info.rev_len * 10 / nwy_app_info.need_rev_len;
        nwy_test_cli_echo("\r\n %d%% nwy_app_info.rev_len = %d length = %d\r\n", nwy_app_info.proceed * 10, nwy_app_info.rev_len, length);
    }
    if (nwy_app_info.rev_len  == nwy_app_info.need_rev_len) {
        nwy_app_update_flag = 0;
    }
    if (nwy_app_info.rev_len > nwy_app_info.need_rev_len) {
        nwy_app_update_flag = 0;
        nwy_test_cli_echo("\r\nAPP info is error,upgrade fail");
    }
}

#include "nwy_fota_api.h"
void nwy_test_cli_fota_app_ver()
{
    int ret = 0xff;
    int timoutcount = 0;
    char* sptr;
    int dfota = 0;

    sptr = nwy_test_cli_input_gets("\r\n 1Please fota type: 0-full packet, 1-dfota, 2-ftp_fota");
    dfota = nwy_app_info.need_rev_len = atoi(sptr);
    if(dfota == 2)
    {
        nwy_test_cli_ftp_fota();
        return ;
    }

    memset(&nwy_app_info, 0, sizeof(nwy_app_info));
    sptr = nwy_test_cli_input_gets("\r\n 1Please input app len: ");
    nwy_app_info.need_rev_len = atoi(sptr);
    if (nwy_app_info.need_rev_len > (256*1024)) {
        nwy_test_cli_echo("\r\n APP is too big,max is 256*1024 bytes");
        return ;
    }

    nwy_app = (uint8 *)malloc(nwy_app_info.need_rev_len);
    if (nwy_app == NULL) {
        nwy_test_cli_echo("\r\n malloc fail");
        return ;
    }
    nwy_sleep(100);
    memset(nwy_app, 0, nwy_app_info.need_rev_len);

    nwy_test_cli_echo("\r\n Input APP(%d):\r\n",nwy_app_info.need_rev_len);
    nwy_app_update_flag = 1;
    while(nwy_app_update_flag) {
        nwy_sleep(1000);
        timoutcount++;
        if (timoutcount >= 100) {
            nwy_test_cli_echo("\r\APP input timeout");
            memset(&nwy_app_info, 0, sizeof(nwy_app_info));
            free(nwy_app);
            return ;
        }
    }
    if (nwy_app_info.rev_len > nwy_app_info.need_rev_len)
    {
        nwy_test_cli_echo("\r\APP info is matching the input length");
        memset(&nwy_app_info, 0, sizeof(nwy_app_info));
        free(nwy_app);
        return ;
    }

    if(dfota)
    {
        ret = nwy_fota_update(nwy_app, nwy_app_info.need_rev_len);
        if (ret != 0) {
            if (ret == NWY_ERROR) {
                nwy_test_cli_echo("\r\nupdate file error");
            } else {
                nwy_test_cli_echo("\r\nupdate error");
            }
        } else {
                nwy_test_cli_echo("update success, device will restart,Please wait \r\n");
                nwy_power_off(2);
        }
    }
    else
    {
        ota_package_t pkt;

        pkt.offset = 0;

        while(pkt.offset < nwy_app_info.need_rev_len)
        {
            if(nwy_app_info.need_rev_len - pkt.offset >= 4096)
                pkt.len = 4096;
            else
                pkt.len = nwy_app_info.need_rev_len - pkt.offset;
            pkt.data = nwy_app + pkt.offset;
            if(nwy_fota_dm(&pkt))
                break;
            pkt.offset += pkt.len;
        }
        if(pkt.offset >= nwy_app_info.need_rev_len)
        if(0 == nwy_package_checksum())
        {
            nwy_test_cli_echo("\r\nsystem will reset to update app");
            nwy_sleep(1000);
            nwy_fota_ua();
        }
        nwy_test_cli_echo("\r\nupdate fail");
    }
    memset(&nwy_app_info, 0, sizeof(nwy_app_info));
    free(nwy_app);
}

#if 0
/**************************FLASH*********************************/
void nwy_test_cli_flash_open()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_flash_erase()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_flash_write()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

void nwy_test_cli_flash_read()
{
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
}

/**************************TTS*********************************/
#ifdef FEATURE_NWY_OPEN_IVTTS
char buff[1024] = {0};
nwy_tts_encode_t encode_type = ENCODE_GBK;
char *hexbuf = "b8b8c4b8d4daa3acb2bbd4b6d3cea3acd3ceb1d8d3d0b7bd";

static void tts_play_callback(void *cb_para, nwy_neoway_result_t result)
{
    switch (result)
    {
    case PLAY_END:
        nwy_test_cli_echo("\r\n tts test down \r\n");
        break;
    default:
        break;
    }
}

void nwy_test_cli_tts_input()
{
    char *sptr;
    memset(buff, 0, 1024);
    sptr = nwy_test_cli_input_gets("\r\nPlease input encode mode(0-gbk,1-utf16le,2-utf16be,3-utf8): ");
    encode_type = (nwy_tts_encode_t)atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\nPlease input content: ");
    strncpy(buff, sptr, strlen(sptr));
}

void nwy_test_cli_tts_play_start()
{
    if (strlen(buff) == 0)
        nwy_tts_playbuf(hexbuf, strlen(hexbuf), ENCODE_GBK, tts_play_callback, NULL);
    else
        nwy_tts_playbuf(buff, strlen(buff), encode_type, tts_play_callback, NULL);
}

void nwy_test_cli_tts_play_stop()
{
    nwy_tts_stop_play();
}
#endif
/**************************FOTA*********************************/
#ifndef NWY_OPEN_TEST_FOTA_NS
nwy_osi_msg_queue_t nwy_download_msg_queue = NULL;
typedef struct
{
    uint32 len;
    void *data;
} nwy_download_queue_msg_t;

static void nwy_cli_recv_callback(unsigned char *data, uint32 length)
{
    nwy_download_queue_msg_t msg;
    if (!data || !length)
        return;
    msg.data = malloc(length);
    if (!msg.data)
        return;
    msg.len = length;
    memcpy(msg.data, data, msg.len);
    if (nwy_download_msg_queue)
        if (NWY_SUCESS != nwy_msg_queue_send(nwy_download_msg_queue, sizeof(msg), &msg, NWY_OSA_SUSPEND))
        {
            free(msg.data);
            NWY_CLI_LOG("put msg que failed drop the data:%d", length);
        }
}

typedef void (*nwy_test_cli_download_callback_t)(unsigned char *data, uint32 length, void *arg);

int nwy_test_cli_download_data(uint32 download_size, uint32 timeout, nwy_test_cli_download_callback_t fn, void *arg)
{
    uint32 size = 0;
    nwy_download_queue_msg_t msg;
    if (!download_size || !fn)
        return size;
    nwy_msg_queue_create(&nwy_download_msg_queue, NULL, sizeof(nwy_download_queue_msg_t), 1024);
    if (!nwy_download_msg_queue)
        return size;
    nwy_test_cli_sio_enter_trans_mode((nwy_sio_trans_cb)nwy_cli_recv_callback);
    while (1)
    {
        memset(&msg, 0, sizeof(msg));
        if (NWY_SUCESS == nwy_msg_queue_recv(nwy_download_msg_queue, sizeof(msg),(uint8 *)&msg, NWY_OSA_SUSPEND))
        {
            if (msg.data && msg.len)
            {
                if ((size + msg.len) > download_size)
                    msg.len = download_size - size;

                fn(msg.data, msg.len, arg);

                free(msg.data);
                size += msg.len;
                if (size >= download_size)
                    break;
            }
        }
        else
            break;
    }

    nwy_test_cli_sio_quit_trans_mode();
    #if 0
    /* clear the rest fifo mem */
    while (1)
    {
        memset(&msg, 0, sizeof(msg));
        if (NWY_SUCESS == nwy_msg_queue_recv(nwy_download_msg_queue, sizeof(msg), (uint8 *)&msg, NWY_OSA_SUSPEND))
        {
            if (msg.data && msg.len)
            {
                free(msg.data);
            }
        }
        else
            break;
    }
    #endif
    nwy_msg_queue_delete(nwy_download_msg_queue);
    nwy_download_msg_queue = NULL;
    return size;
}

void nwy_test_cli_download_sdk_fota_pkt_cb(unsigned char *data, uint32 length, void *arg)
{
    ota_package_t *pkt = arg;
    pkt->data = data;
    pkt->len = length;
    if (!nwy_fota_download_core(pkt))
        pkt->offset += length;
    nwy_test_cli_echo("\r\ndownload pkt size:%d", pkt->offset);
}

void nwy_test_cli_fota_base_ver()
{
    char *sptr;
    int pkt_size;
    sptr = nwy_test_cli_input_gets("\r\n Please input firmware packet size:");
    pkt_size = atoi(sptr);
    if (pkt_size <= 0)
    {
        nwy_test_cli_echo("\r\n Fota Error : invalid packet size:%s", sptr);
        return;
    }
    nwy_test_cli_echo("\r\n Please input firmware:\r\n");
    ota_package_t ota_pkt;
    memset(&ota_pkt, 0, sizeof(ota_pkt));
    ota_pkt.ota_size = pkt_size;
    if (pkt_size <= nwy_test_cli_download_data(pkt_size, 8000, nwy_test_cli_download_sdk_fota_pkt_cb, &ota_pkt))
    {
        nwy_test_cli_echo("\r\n firmware download finish");
        nwy_test_cli_echo("\r\n system will reset for update");
        nwy_sleep(1000);
        nwy_version_core_update(true);
        nwy_test_cli_echo("\r\n firmware wrong");
    }
    nwy_test_cli_echo("\r\n what happened.");
}

void nwy_test_cli_download_app_fota_pkt_cb(unsigned char *data, uint32 length, void *arg)
{
    ota_package_t *pkt = arg;
    pkt->data = data;
    pkt->len = length;

    if (!nwy_fota_dm(pkt))
        pkt->offset += length;
    nwy_test_cli_echo("\r\ndownload pkt size:%d", pkt->offset);
}

void nwy_test_cli_fota_app_ver()
{
    char *sptr;
    int ret, pkt_size;
    sptr = nwy_test_cli_input_gets("\r\n Please input firmware packet size:");
    pkt_size = atoi(sptr);
    if (pkt_size <= 0)
    {
        nwy_test_cli_echo("\r\n Fota Error : invalid packet size:%s", sptr);
        return;
    }
    nwy_test_cli_echo("\r\n Please input firmware:\r\n");
    ota_package_t ota_pkt;
    memset(&ota_pkt, 0, sizeof(ota_pkt));
    ota_pkt.ota_size = pkt_size;
    if (pkt_size <= nwy_test_cli_download_data(pkt_size, 8000, nwy_test_cli_download_app_fota_pkt_cb, &ota_pkt))
    {
        nwy_test_cli_echo("\r\n firmware download finish");
        ret = nwy_package_checksum();
        if (ret < 0)
        {
            nwy_test_cli_echo("\r\nchecksum failed");
            return;
        }
        nwy_test_cli_echo("\r\n system will reset for update");
        nwy_sleep(1000);
        ret = nwy_fota_ua();
        if (ret < 0)
        {
            nwy_test_cli_echo("\r\nupdate failed");
            return;
        }
    }
    nwy_test_cli_echo("\r\n what happened.");
}
#endif
#endif

/**************************AUDIO*********************************/
#ifdef NWY_OPEN_TEST_AUDIO
void nwy_test_cli_audio_rec_start(void)
{
	nwy_test_cli_echo("\r\nAudio Record Start");

	// enable mic
	nwy_audio_set_mic_vol(1);
	nwy_audio_record_start();
}

void nwy_test_cli_audio_rec_stop(void)
{
	nwy_test_cli_echo("\r\nAudio Record Stop");
	nwy_audio_record_stop();
}

void nwy_test_cli_audio_rec_selftest(void)
{
	nwy_test_cli_echo("\r\nAudio Rec selftest");
	nwy_audio_record_selftest();
}

void nwy_test_cli_audio_play_start(void)
{
	nwy_test_cli_echo("\r\nAudio Play Start");
	nwy_audio_play_start();
}

void nwy_test_cli_audio_play_start_file(void)
{
	int ret = -1;
	int save_mode = 0;
	char file_path[128] = {0};
    int play_format =0;
	char *sptr;
	nwy_test_cli_echo("\r\nAudio Play Start File");
	sptr = nwy_test_cli_input_gets("\r\n choose save mode: 0:fs  1:sd 2:spi flash");
	save_mode = atoi(sptr);
	sptr = nwy_test_cli_input_gets("\r\n choose file path:");
	strcpy(file_path, sptr);
	sptr = nwy_test_cli_input_gets("\r\n choose play file format:");
    play_format = atoi(sptr);
	nwy_audio_play_start_file(file_path, save_mode, play_format);
}
void nwy_test_cli_audio_record_start_file(void)
{
	int ret = -1;
	int save_mode = 0;
	char file_path[128] = {0};
	char *sptr;
	nwy_test_cli_echo("\r\nAudio record Start File");
	sptr = nwy_test_cli_input_gets("\r\n choose save mode: 0:fs  1:sd 2:spi flash");
	save_mode = atoi(sptr);
	sptr = nwy_test_cli_input_gets("\r\n choose file path:");
	strcpy(file_path, sptr);
	nwy_audio_set_mic_vol(1);
	nwy_audio_record_start_file(file_path, save_mode);
}
void nwy_test_cli_audio_record_stop_file(void)
{
	nwy_test_cli_echo("\r\nAudio Play stop File");
	nwy_audio_record_stop_file();
}
void nwy_test_cli_audio_play_stop(void)
{
	nwy_test_cli_echo("\r\nAudio Play Stop");
	nwy_audio_play_stop();
}

void nwy_test_cli_audio_set_speaker_vol(void)
{
	char *sptr;
	int vol;

	sptr = nwy_test_cli_input_gets("\r\nPlease input Speaker Vol(0-15):");
	vol = atoi(sptr);
	nwy_audio_set_speaker_vol(vol);
}
void nwy_test_cli_audio_get_speaker_vol(void)
{
	int vol;
	vol = nwy_audio_get_speaker_vol();
    nwy_test_cli_echo("\r\n current vol:%d\r\n", vol);
}

static void DTMF_CallBack(char key, unsigned int persistencetime)
{
	nwy_test_cli_echo("\r\nNWY_DTMF_key:%c\r\n", key);
}

void nwy_test_cli_audio_dtmf_detect(void)
{
	char *sptr;
	int enable;

	nwy_test_cli_echo("\r\nDtmf Detect Option:\r\n0:Disable DTMF Detect\r\n1:Enable DTMF Detect\r\n");
	sptr = nwy_test_cli_input_gets("\r\nPlease input Option(0-1):");
	enable = atoi(sptr);
	if (enable)
	{
		nwy_test_cli_echo("\r\n DTMF Enable!!!\r\n");
		nwy_audio_dtmf_detect(1, DTMF_CallBack);
	}
	else
	{
		nwy_test_cli_echo("\r\n DTMF Disable!!!\r\n");
		nwy_audio_dtmf_detect(0, NULL);
	}
}

void nwy_test_cli_audio_dtmf_get_status(void)
{
	int status;

	nwy_test_cli_echo("\r\n Get DTMF Status:\r\n 0:Idle\r\n 1:Start\r\n 2:Run\r\n 3:Stop\r\n");
	status = nwy_audio_dtmf_get_status();
	nwy_test_cli_echo("\r\n DTMF Status=%d\r\n");
}
void nwy_test_cli_audio_caccp_param(void)
{
    NWY_AUD_ITF_T nITF;
    NWY_AUD_NCTRL_T nCtrl;
    bool is_music;
    char *sptr;
    nwy_test_cli_echo("\r\nAudio caccp param set");
    sptr = nwy_test_cli_input_gets("\r\n choose nITF");
    nITF = atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\n choose nCtrl");
    nCtrl = atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\n choose ismusic");
    is_music = atoi(sptr);
    sptr = nwy_test_cli_input_gets("\r\n choose nparam");
    nwy_set_audio_caccp(nITF, nCtrl, sptr,is_music);
}
void nwy_test_cli_audio_cawtf(void)
{
    bool ret;
    nwy_test_cli_echo("\r\nAudio set cawtf ");
    ret = nwy_set_audio_cawtf();
    if (ret != 0)
        nwy_test_cli_echo("\r\n nwy_set_audio_cawtf failed\r\n");
    else
        nwy_test_cli_echo("\r\n nwy_set_audio_cawtf ok\r\n");
}
void nwy_test_cli_audio_player_play(void)
{
    bool ret;
    int fd = -1;
    char *sptr;
    int file_len = 0;
    int tmp_len = 0;
    char *play_buff =NULL;
    int malloc_len = 64*1000;
    int read_len = 0;
    int sample_rate = 0;
    nwy_test_cli_echo("\r\nAudio test player play");

    play_buff = malloc(malloc_len);
    if (play_buff == NULL)
    {
        nwy_test_cli_echo("\r\n malloc error");
        return;
    }
    memset(play_buff, 0, malloc_len);
    sptr = nwy_test_cli_input_gets("\r\n input play path");
    fd = nwy_sdk_fopen(sptr, NWY_RDONLY);
    if (fd < 0)
    {
        nwy_test_cli_echo("\r\n open file error\r\n");
        return;
    }
    file_len = nwy_sdk_fsize_fd(fd);
    nwy_test_cli_echo("\r\n file_len: %d\r\n",file_len);
    sptr = nwy_test_cli_input_gets("\r\n sample rate");
    sample_rate = atoi(sptr);
    while (tmp_len < file_len)
    {
        read_len = nwy_sdk_fread(fd, play_buff,malloc_len);
        nwy_test_cli_echo("\r\n read_len:%d, malloc_len:%d\r\n",read_len, malloc_len);
        ret = nwy_audio_player_play(play_buff, read_len, sample_rate);
        if(ret != 0)
        {
            nwy_test_cli_echo("\r\n nwy_audio_player_play error\r\n");
            return;
        }
        tmp_len += read_len;
        memset(play_buff, 0, malloc_len);
    }
    nwy_sdk_fclose(fd);
    free(play_buff);
    nwy_test_cli_echo("\r\n paly over\r\n");
}
void nwy_test_cli_audio_set_ouput_device(void)
{
    char *sptr;
    int output_device;
    nwy_test_cli_echo("\r\n audio_set_ouput_device");
    sptr = nwy_test_cli_input_gets("\r\n input output device");
    output_device = atoi(sptr);
    nwy_change_output_channel(output_device);
}
#endif

/**************************FS*********************************/
#define NWY_FILE_NAME_MAX 64
static int nwy_test_fs_fd = -1;
static char nwy_test_file_name[NWY_FILE_NAME_MAX + 1] = {0};
static void nwy_fs_close(int fd)
{
    nwy_sdk_fclose(fd);
    nwy_test_fs_fd = -1;
}
void nwy_test_cli_fs_open(void)
{
    char *sptr;
    memset(nwy_test_file_name, 0, sizeof(nwy_test_file_name));
    sptr = nwy_test_cli_input_gets("\r\nPlease input filename(len <= %d): ", NWY_FILE_NAME_MAX);
    if (strlen(sptr) > NWY_FILE_NAME_MAX)
    {
        nwy_test_cli_echo("\r\nfile name can't beyond %d", NWY_FILE_NAME_MAX);
        return;
    }
    strcpy(nwy_test_file_name, sptr);
    nwy_test_fs_fd = nwy_sdk_fopen(nwy_test_file_name, NWY_CREAT | NWY_RDWR);
    if (nwy_test_fs_fd == NWY_FS_PATH_ERR) {
        nwy_test_cli_echo("\r\nfile name can't with path,only support current directory\r\n");
    } else if (nwy_test_fs_fd < 0) {
        nwy_test_cli_echo("\r\nfile %s open error:%d\r\n", nwy_test_file_name, nwy_test_fs_fd);
    } else {
        nwy_test_cli_echo("\r\nfile %s open success:%d\r\n", nwy_test_file_name, nwy_test_fs_fd);
    }
}

void nwy_test_cli_fs_write(void)
{
    char *sptr;
    int avail_space = 0;

    nwy_sdk_sys_avail_space_get(&avail_space);
    nwy_test_cli_echo("\r\nfile system avail_space:%d\r\n", avail_space);

    sptr = nwy_test_cli_input_gets("\r\nPlease input file write data(len <= 2000): ");
    int len = strlen(sptr);
    if (len > 2000)
    {
        nwy_test_cli_echo("\r\nfile write data can't beyond 2000");
        return;
    }
    int rtn = nwy_sdk_fwrite(nwy_test_fs_fd, sptr, len);
    if (rtn != len)
        nwy_test_cli_echo("\r\nfile %s write error:%d\r\n", nwy_test_file_name, rtn);
    else
        nwy_test_cli_echo("\r\nfile %s write success:%d\r\n", nwy_test_file_name, rtn);
}

void nwy_test_cli_fs_read()
{
    int size, rtn;
    char buffer[128];
    nwy_test_cli_echo("\r\n");
    size = rtn = 0;
    nwy_sdk_fseek(nwy_test_fs_fd, 0, NWY_SEEK_SET);

    while (1)
    {
        rtn = nwy_sdk_fread(nwy_test_fs_fd, buffer, sizeof(buffer));
        NWY_CLI_LOG("read data size:%d", rtn);
        if (rtn > 0)
            nwy_test_cli_output(buffer, rtn);
        else
            break;
        size += rtn;
    }
    if (size)
        nwy_test_cli_echo("\r\nfile %s read success:%d\r\n", nwy_test_file_name, size);
}

void nwy_test_cli_fs_fsize()
{
    nwy_test_cli_echo("\r\nfile %s size:%d\r\n", nwy_test_file_name, nwy_sdk_fsize_fd(nwy_test_fs_fd));
}

void nwy_test_cli_fs_seek()
{
    char *sptr;
    char buffer[128] = {0};
    int size = 0;
    sptr = nwy_test_cli_input_gets("\r\nPlease input file seek offset: ");
    int offset = atoi(sptr);
    int rtn = nwy_sdk_fseek(nwy_test_fs_fd, offset, NWY_SEEK_SET);
    if (rtn != offset)
        nwy_test_cli_echo("\r\nfile %s seek error:%d\r\n", nwy_test_file_name, rtn);
    else
        nwy_test_cli_echo("\r\nfile %s seek success:%d\r\n", nwy_test_file_name, rtn);
    while (1)
    {
        rtn = nwy_sdk_fread(nwy_test_fs_fd, buffer, sizeof(buffer));
        NWY_CLI_LOG("read data size:%d", rtn);
        if (rtn > 0)
            nwy_test_cli_output(buffer, rtn);
        else
            break;
        size += rtn;
    }
    if (size)
        nwy_test_cli_echo("\r\nfile %s read success:%d\r\n", nwy_test_file_name, size);
}

void nwy_test_cli_fs_sync()
{
#ifdef NWY_OPEN_TEST_FS_ADV_NS
      nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_test_cli_echo("\r\nfile %s sync:%d\r\n", nwy_test_file_name, nwy_sdk_fsync(nwy_test_fs_fd));
#endif
}

void nwy_test_cli_fs_fstate()
{
#ifdef NWY_OPEN_TEST_FS_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    struct stat st;
    int rtn = nwy_sdk_get_stat_fd(nwy_test_fs_fd, &st);
    nwy_test_cli_echo("\r\nfile %s stat:%d st_size:%d\r\n", nwy_test_file_name, rtn, st.st_size);
#endif
}

void nwy_test_cli_fs_trunc()
{
#ifdef NWY_OPEN_TEST_FS_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input file trunc size: ");
    int size = atoi(sptr);
    int rtn = nwy_sdk_ftrunc_fd(nwy_test_fs_fd, size);
    if (rtn != size)
        nwy_test_cli_echo("\r\nfile %s trunc error:%d\r\n", nwy_test_file_name, rtn);
    else
        nwy_test_cli_echo("\r\nfile %s trunc success:%d\r\n", nwy_test_file_name, rtn);
#endif
}

void nwy_test_cli_fs_close()
{
    nwy_test_cli_echo("\r\nfile %s close:%d\r\n", nwy_test_file_name, nwy_sdk_fclose(nwy_test_fs_fd));
    nwy_test_fs_fd = -1;
}

void nwy_test_cli_fs_remove()
{
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input file name: ");
    int rtn = nwy_sdk_file_unlink(sptr);
    if (rtn != 0)
        nwy_test_cli_echo("\r\nfile %s remove error:%d\r\n", sptr, rtn);
    else
        nwy_test_cli_echo("\r\nfile %s remove success:%d\r\n", sptr, rtn);
}

void nwy_test_cli_fs_rename()
{
    char *sptr;
    char old[64], new[64];
    memset(old, 0, sizeof(old));
    memset(new, 0, sizeof(new));
    sptr = nwy_test_cli_input_gets("\r\nPlease input file old name: ");
    strncpy(old, sptr, sizeof(old));
    sptr = nwy_test_cli_input_gets("\r\nPlease input file new name: ");
    strncpy(new, sptr, sizeof(new));
    int rtn = nwy_sdk_frename(old, new);
    if (rtn != 0)
        nwy_test_cli_echo("\r\nfile %s rename error:%d\r\n", old, rtn);
    else
        nwy_test_cli_echo("\r\nfile %s rename success:%d\r\n", new, rtn);
}

#define NWY_DIR_NAME_MAX 64
#ifndef NWY_OPEN_TEST_DIR_ADV_NS
static nwy_dir *nwy_test_fs_dir = NULL;
static char nwy_test_dir_name[NWY_DIR_NAME_MAX + 1] = {0};
#endif
void nwy_test_cli_dir_open()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    memset(nwy_test_dir_name, 0, sizeof(nwy_test_dir_name));
    sptr = nwy_test_cli_input_gets("\r\nPlease input dir name(len <= %d): ", NWY_DIR_NAME_MAX);
    if (strlen(sptr) > NWY_DIR_NAME_MAX)
    {
        nwy_test_cli_echo("\r\ndir name can't beyond %d", NWY_DIR_NAME_MAX);
        return;
    }
    strcpy(nwy_test_dir_name, sptr);
    nwy_test_fs_dir = nwy_sdk_vfs_opendir(nwy_test_dir_name);
    if (nwy_test_fs_dir == NULL)
        nwy_test_cli_echo("\r\ndir %s open error:%d\r\n", nwy_test_dir_name, nwy_test_fs_dir);
    else
        nwy_test_cli_echo("\r\ndir %s open success:%d\r\n", nwy_test_dir_name, nwy_test_fs_dir);
#endif
}

void nwy_test_cli_dir_read()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char rsp[256 + 84];
    nwy_dirent *ent;
    struct stat st;
    size_t name_len = strlen(nwy_test_dir_name);
    bool trail_slash = (name_len > 0 && nwy_test_dir_name[name_len - 1] == '/');
    //nwy_sdk_vfs_seekdir(nwy_test_fs_dir, 0);
    while ((ent = nwy_sdk_vfs_readdir(nwy_test_fs_dir)) != NULL)
    {
        if (ent->d_type == NWY_DT_REG)
        {
            // borrow rsp for full_path
            snprintf(rsp, sizeof(rsp),"%s/%s", nwy_test_dir_name, ent->d_name);
            nwy_sleep(50);
            if (nwy_sdk_get_stat_path(rsp, &st) != 0)
                continue;

            if (trail_slash)
                snprintf(rsp, sizeof(rsp), "dir read \"%s%s\",%ld", nwy_test_dir_name, ent->d_name, st.st_size);
            else
                snprintf(rsp, sizeof(rsp), "dir read \"%s/%s\",%ld", nwy_test_dir_name, ent->d_name, st.st_size);
            nwy_test_cli_echo("\r\n%s", rsp);
        }
        else if (ent->d_type == NWY_DT_DIR)
        {
            if (trail_slash)
                snprintf(rsp, sizeof(rsp), "dir read \"%s%s\"", nwy_test_dir_name, ent->d_name);
            else
                snprintf(rsp, sizeof(rsp), "dir read \"%s/%s\"", nwy_test_dir_name, ent->d_name);
            nwy_test_cli_echo("\r\n%s", rsp);
        }
    }
    nwy_test_cli_echo("\r\n");
#endif
}

void nwy_test_cli_dir_tell()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_test_cli_echo("\r\ndir %s tell:%d\r\n", nwy_test_dir_name, nwy_sdk_vfs_telldir(nwy_test_fs_dir));
#endif
}

void nwy_test_cli_dir_seek()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input dir seek offset: ");
    int offset = atoi(sptr);
    nwy_sdk_vfs_seekdir(nwy_test_fs_dir, offset);
    nwy_test_cli_echo("\r\ndir %s seek success:%d\r\n", nwy_test_dir_name, offset);
#endif
}

void nwy_test_cli_dir_rewind()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_sdk_vfs_rewinddir(nwy_test_fs_dir);
    nwy_test_cli_echo("\r\ndir %s rewind success\r\n", nwy_test_dir_name);
#endif
}

void nwy_test_cli_dir_close()
{
#ifdef NWY_OPEN_TEST_DIR_ADV_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_test_cli_echo("\r\ndir %s close:%d\r\n", nwy_test_dir_name, nwy_sdk_vfs_closedir(nwy_test_fs_dir));
    nwy_test_fs_dir = NULL;
#endif
}

void nwy_test_cli_dir_mk()
{
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input dir name(len <= %d): ", NWY_DIR_NAME_MAX);
    if (strlen(sptr) > NWY_DIR_NAME_MAX)
    {
        nwy_test_cli_echo("\r\ndir name can't beyond %d", NWY_DIR_NAME_MAX);
        return;
    }
    nwy_test_cli_echo("\r\ndir %s mk:%d\r\n", sptr, nwy_sdk_vfs_mkdir(sptr));
}

void nwy_test_cli_dir_remove()
{
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input dir name(len <= %d): ", NWY_DIR_NAME_MAX);
    if (strlen(sptr) > NWY_DIR_NAME_MAX)
    {
        nwy_test_cli_echo("\r\ndir name can't beyond %d", NWY_DIR_NAME_MAX);
        return;
    }
    //nwy_sdk_vfs_rmdir(sptr);
    nwy_test_cli_echo("\r\ndir %s remove:%d\r\n", sptr, nwy_sdk_vfs_rmdir_recursive(sptr));
}

void nwy_test_cli_fs_free_size()
{
#ifdef NWY_OPEN_TEST_FS_FREE_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    nwy_test_cli_echo("\r\nfs free size:%d\r\n", nwy_sdk_vfs_ls());
#endif
}

void nwy_test_cli_safe_fs_init()
{
#ifdef NWY_OPEN_TEST_SAFE_FS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input filename(len <= %d): ", NWY_FILE_NAME_MAX);
    if (strlen(sptr) > NWY_FILE_NAME_MAX)
    {
        nwy_test_cli_echo("\r\nfile name can't beyond %d", NWY_FILE_NAME_MAX);
        return;
    }
    nwy_test_cli_echo("\r\nsfile %s init:%d\r\n", sptr, nwy_sdk_sfile_init(sptr));
#endif
}

void nwy_test_cli_safe_fs_read()
{
#ifdef NWY_OPEN_TEST_SAFE_FS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    int rtn, size;
    char fn[NWY_FILE_NAME_MAX];
    sptr = nwy_test_cli_input_gets("\r\nPlease input filename(len <= %d): ", NWY_FILE_NAME_MAX);
    if (strlen(sptr) > NWY_FILE_NAME_MAX)
    {
        nwy_test_cli_echo("\r\nfile name can't beyond %d", NWY_FILE_NAME_MAX);
        return;
    }
    memset(fn, 0, sizeof(fn));
    strncpy(fn, sptr, NWY_FILE_NAME_MAX);
    size = nwy_sdk_sfile_size(fn);
    if (size <= 0)
    {
        nwy_test_cli_echo("\r\nsfile %s read error:%d\r\n", fn, size);
        return;
    }
    char *buffer = malloc(size);
    if (buffer == NULL)
        return;
    rtn = nwy_sdk_sfile_read(fn, buffer, size);
    if (rtn > 0)
    {
        nwy_test_cli_echo("\r\n");
        nwy_test_cli_output(buffer, rtn);
        nwy_test_cli_echo("\r\nfile %s read success:%d\r\n", fn, rtn);
    }
    else
        nwy_test_cli_echo("\r\nfile %s read error:%d\r\n", fn, rtn);
    free(buffer);
#endif
}

void nwy_test_cli_safe_fs_write()
{
#ifdef NWY_OPEN_TEST_SAFE_FS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    int rtn, len;
    char fn[NWY_FILE_NAME_MAX];
    sptr = nwy_test_cli_input_gets("\r\nPlease input filename(len <= %d): ", NWY_FILE_NAME_MAX);
    if (strlen(sptr) > NWY_FILE_NAME_MAX)
    {
        nwy_test_cli_echo("\r\nfile name can't beyond %d", NWY_FILE_NAME_MAX);
        return;
    }
    memset(fn, 0, sizeof(fn));
    strncpy(fn, sptr, NWY_FILE_NAME_MAX);
    sptr = nwy_test_cli_input_gets("\r\nPlease input sfile write data(len <= 2000): ");
    len = strlen(sptr);
    if (len > 2000)
    {
        nwy_test_cli_echo("\r\nsfile write data can't beyond 2000");
        return;
    }
    NWY_CLI_LOG("write data[%d] to sfile %s: %s", len, fn, sptr);
    rtn = nwy_sdk_sfile_write(fn, sptr, len);
    if (rtn != len)
        nwy_test_cli_echo("\r\nsfile %s write error:%d\r\n", fn, rtn);
    else
        nwy_test_cli_echo("\r\nsfile %s write success:%d\r\n", fn, rtn);
#endif
}

void nwy_test_cli_safe_fs_fszie()
{
#ifdef NWY_OPEN_TEST_SAFE_FS_NS
    nwy_test_cli_echo("\r\nOption not Supported!\r\n");
#else
    char *sptr;
    sptr = nwy_test_cli_input_gets("\r\nPlease input filename(len <= %d): ", NWY_FILE_NAME_MAX);
    if (strlen(sptr) > NWY_FILE_NAME_MAX)
    {
        nwy_test_cli_echo("\r\nfile name can't beyond %d", NWY_FILE_NAME_MAX);
        return;
    }
    nwy_test_cli_echo("\r\nsfile %s size:%d\r\n", sptr, nwy_sdk_sfile_size(sptr));
#endif
}


static unsigned char *fota_buffer = NULL;
static uint32 fota_buffer_len = 0;

//ftp fota
#define FTP_HOST "123.139.59.166"
#define FTP_PORT 10113
#define FTP_FILEPATH "app_fota.bin"
#define FTP_MODE 0  //1:active 0:passv
#define FTP_USERNAME "neoway"
#define FTP_PASSWORD "neoway"
#define FTP_SINGLE_DOWNLOAD_SIZE  150*1024
#define USE_FTPS 0

typedef enum{
        FTP_FOTA_START = NWY_APP_EVENT_ID_BASE + 30,
        FTP_FOTA_CONNECTED,
        FTP_FOTA_DISCONNECTED,
        FTP_FOTA_GET_FILESIZE,
        FTP_FOTA_DOWNLOADING,
        FTP_FOTA_ERROR
}ftp_fota_event;


const uint32 ftp_event_ids[] = {
    [NWY_FTP_EVENT_DNS_ERR] = FTP_FOTA_START,
    [NWY_FTP_EVENT_OPEN_FAIL] = FTP_FOTA_START,
    [NWY_FTP_EVENT_CLOSED] = FTP_FOTA_DISCONNECTED,
    [NWY_FTP_EVENT_LOGOUT] = FTP_FOTA_DISCONNECTED,
    [NWY_FTP_EVENT_DATA_RECVED] = FTP_FOTA_DOWNLOADING,
    [NWY_FTP_EVENT_DATA_GET] = FTP_FOTA_DOWNLOADING,
    [NWY_FTP_EVENT_DATA_OPEND] = FTP_FOTA_DOWNLOADING,
    [NWY_FTP_EVENT_DATA_CLOSED] = FTP_FOTA_DOWNLOADING,
    [NWY_FTP_EVENT_DATA_SETUP_ERROR] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_LOGIN] = FTP_FOTA_CONNECTED,
    [NWY_FTP_EVENT_SIZE] = FTP_FOTA_GET_FILESIZE,
    [NWY_FTP_EVENT_SIZE_ZERO] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_PASS_ERROR] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_FILE_NOT_FOUND] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_FILE_SIZE_ERROR] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_FILE_DELE_SUCCESS] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_DATA_PUT_FINISHED] = FTP_FOTA_ERROR,
    [NWY_FTP_EVENT_UNKOWN] = FTP_FOTA_ERROR
};

static nwy_osi_thread_t nwy_ftp_fota_thread = NULL;


void nwy_cli_ftp_fota_cb(nwy_ftp_result_t *param)
{
    nwy_event_msg_t event;
    event.param1 = 0;
    if (NULL == param)
        return;
    else if(param->event > NWY_FTP_EVENT_UNKOWN)
        return;
    else if (NWY_FTP_EVENT_SIZE == param->event)
    {
        event.param1 = *(uint32 *)param->data;
    }
    else if (NWY_FTP_EVENT_DATA_GET == param->event)
    {
        if (NULL == param->data)
            return;
        if (param->data_len != 0)
        {
            if(fota_buffer_len > FTP_SINGLE_DOWNLOAD_SIZE)
                return;
            memcpy(fota_buffer + fota_buffer_len, (unsigned char *)param->data, param->data_len);
            fota_buffer_len += param->data_len;
        }
        event.param1 = 0;
    }
    else if(NWY_FTP_EVENT_DATA_CLOSED == param->event)
    {
        event.param1 = 1;
    }
    event.param2 = param->event;
    event.id = ftp_event_ids[param->event];
    nwy_send_thread_event(nwy_ftp_fota_thread, &event, NWY_OSA_SUSPEND);
}

void nwy_test_ftp_fota_entry()
{
    int result = 0; 
    uint32 single_get_size = 0;
    nwy_event_msg_t event = {NULL};
    ota_package_t fota_pkt = {NULL};
    uint32 fota_file_size = 0;
    bool ftp_is_connected = false;
    uint32 retry = 0;
    nwy_ftp_login_para_t ftp_param = {
        .channel = 1,
        .host = FTP_HOST,
        .port = FTP_PORT,
        .username = FTP_USERNAME,
        .passwd = FTP_PASSWORD,
        .mode = FTP_MODE,
        .timeout = 60*15
    };
    fota_buffer = NULL;
    fota_buffer_len = 0;
    fota_buffer = (unsigned char *)malloc(FTP_SINGLE_DOWNLOAD_SIZE);
    if(fota_buffer == NULL)
    {
        nwy_test_cli_echo("\r\nmalloc fota buffer fail");
        result = -1;
    }
    memset(fota_buffer, 0x00, FTP_SINGLE_DOWNLOAD_SIZE);
    fota_pkt.offset = 0;

    while(1)
    {
        if(result != 0)
            break;
        if(!nwy_wait_thread_event(nwy_ftp_fota_thread, &event, NWY_OSA_SUSPEND))
            break;
        nwy_test_cli_echo("\r\nnwy_wait_thread_event recive %d", event.id);
        switch(event.id)
        {
            case FTP_FOTA_START:
            {
                nwy_test_cli_echo("\r\nftp fota start");
                result = -1;
                if(retry < 3)
                    result = nwy_ftp_login(&ftp_param, nwy_cli_ftp_fota_cb);
                retry ++;
                break;
            }
            case FTP_FOTA_CONNECTED:
            {
                nwy_test_cli_echo("\r\nftp fota connected");
                ftp_is_connected = true;
                retry = 0;
                result = nwy_ftp_filesize(1, FTP_FILEPATH, 10000);
                break;
            }
            case FTP_FOTA_GET_FILESIZE:
            {
                fota_file_size = event.param1;
                nwy_test_cli_echo("\r\nfile size %d", fota_file_size);
                single_get_size = fota_file_size - fota_pkt.offset;
                single_get_size = single_get_size < FTP_SINGLE_DOWNLOAD_SIZE ? single_get_size : FTP_SINGLE_DOWNLOAD_SIZE;
                result = nwy_ftp_get(1, FTP_FILEPATH, 2, 0, single_get_size);
                break;
            }
            case FTP_FOTA_DOWNLOADING:
            {
                if(fota_buffer_len != single_get_size || event.param1 == 0)
                    break;
                if((fota_buffer_len == single_get_size) && (event.param1 == 1))
                {
                    fota_pkt.data = fota_buffer;
                    fota_pkt.len = fota_buffer_len;
                    nwy_test_cli_echo("\r\nftp fota_pkt.len %d offset %d", fota_pkt.len, fota_pkt.offset);

                    if (nwy_fota_dm(&fota_pkt) == 0)
                    {
                        fota_pkt.offset += fota_buffer_len;
                        nwy_test_cli_echo("\r\nftp fota download %d", fota_pkt.offset*100/fota_file_size);
                        retry = 0;
                    }
                    else
                    {
                        if(++ retry > 3)
                        {
                            result = -1;
                            break;
                        }
                    }
                    if(fota_file_size == fota_pkt.offset)
                    {
                        nwy_test_cli_echo("\r\nftp fota download success");
                        if(0 == nwy_package_checksum())
                        {
                            nwy_test_cli_echo("\r\nsystem will reset to update app");
                            nwy_sleep(1000);
                            nwy_fota_ua();
                            return ;
                        }
                        else
                        {
                            nwy_test_cli_echo("\r\nfota file checksum err");
                            result = -1;
                            break;
                        }
                    }
                }
                fota_buffer_len = 0;
                single_get_size = fota_file_size - fota_pkt.offset;
                single_get_size = single_get_size < FTP_SINGLE_DOWNLOAD_SIZE ? single_get_size : FTP_SINGLE_DOWNLOAD_SIZE;
                result = nwy_ftp_get(1, FTP_FILEPATH, 2, fota_pkt.offset, single_get_size);
                break;
            }
            case FTP_FOTA_DISCONNECTED:
            {
                ftp_is_connected = false;
                result = -1;
                break;
            }
            case FTP_FOTA_ERROR:
            {
                result = -1;
                break;
            }
            default:
                break;
            if(-1 == result)
                break;
        }
    }
    free(fota_buffer);
    nwy_test_cli_echo("\r\nfota fail");
    if(ftp_is_connected)
    {
        if(!nwy_ftp_logout(1, 20000))
        while(1)
        {
            result = nwy_wait_thread_event(nwy_ftp_fota_thread, &event, 10000);
            if(event.id == FTP_FOTA_DISCONNECTED || result == false)
                break;
        }
    }
    nwy_exit_thread_self();
}
        

void nwy_test_cli_ftp_fota()
{
    nwy_event_msg_t event = {NULL};
    event.id = FTP_FOTA_START;
    if(nwy_create_thread(&nwy_ftp_fota_thread, 1024 * 8, NWY_OSI_PRIORITY_NORMAL, "ftp_fota", nwy_test_ftp_fota_entry, NULL, 16))
    {
            nwy_test_cli_echo("\r\n ftp fota thread create fail");
            return;
    }        
    nwy_sleep(3000);
    nwy_send_thread_event(nwy_ftp_fota_thread, &event, NWY_OSA_SUSPEND);
}



