#ifndef _DHT11_H
#define _DHT11_H


#include "project.h"
#include "Hardware.h"
#include "nwy_osi_api.h"

#define DHT11_PIN                   IP2_PIN
#define DHT11_PIN_MODE_OUTPUT       nwy_gpio_set_direction(DHT11_PIN,nwy_output)
#define DHT11_PIN_MODE_INPUT        nwy_gpio_set_direction(DHT11_PIN,nwy_input)
#define DHT11_PIN_GET               nwy_gpio_get_value(DHT11_PIN)
#define DHT11_PIN_SET(x)            nwy_gpio_set_value(DHT11_PIN,x)


typedef struct 
{
    uint8_t Status;
    uint8_t temp;
    uint8_t humidity;
}DHT11DataTypedef;

extern DHT11DataTypedef DHT11;

void DHT11ThreadEntry(void *param);


#endif