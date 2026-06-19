#include "DHT11.h"

DHT11DataTypedef DHT11;
int tm, rt;

void DHT11_Init()
{
    DHT11_PIN_MODE_OUTPUT;
    DHT11_PIN_SET(1);
}

int DHT11_read(DHT11DataTypedef *dht)
{
    uint8_t data[5] = {0};
    DHT11_PIN_MODE_OUTPUT;
    DHT11_PIN_SET(0);
    nwy_sleep(18);
    DHT11_PIN_SET(1);
    nwy_usleep(40);
    DHT11_PIN_MODE_INPUT;
    tm = 0;
    while(!DHT11_PIN_GET)
    {
        if(++tm > 100)
            return -1;
        
        nwy_usleep(1);
    }
    tm = 0;
    while(DHT11_PIN_GET)
    {
        if(++tm > 100)
            return -2;
        
        nwy_usleep(1);
    }
    // Sensor OK
    for(int j = 0; j < 5; ++j) {
		for(int i = 0; i < 8; ++i) {
			
			//LOW for 50us
            tm = 0;
			while(!DHT11_PIN_GET)
            {
                if(++tm > 100)
                    return -3;
        
                nwy_usleep(1);
            }

			//HIGH for 26-28us = 0 / 70us = 1
            tm = 0;
			while(DHT11_PIN_GET)
            {
                if(++tm > 100)
                    return -4;
        
                nwy_usleep(1);
            }

			
			//shift 0
			data[j] = data[j] << 1;
			
			//if > 30us it's 1
			if(tm > 40)
				data[j] = data[j]+1;
		}
	}

	dht->temp = data[2];
	dht->humidity = data[0];
	DHT11_PIN_MODE_OUTPUT;
    return 0;
}

void DHT11ThreadEntry(void *param)
{   
    int ret;
    nwy_sleep(5000);
    while(VTSData.SensorSetting.IP2Mode != IP2_MODE_DHT11)
       nwy_sleep(5000); 
    DHT11_Init();
    while(1)
    {
        ret = DHT11_read(&DHT11);
        if(ret != 0)
        {
            DHT11.Status=0;
            nwy_dbg_log("\r\nDHT11 read err %i",ret);
        }
        else
            DHT11.Status=1;
        nwy_sleep(3000);
    }
    nwy_exit_thread(nwy_dht11_thread);
}


