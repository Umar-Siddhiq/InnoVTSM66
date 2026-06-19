#ifndef NTP_CUSTOM_H
#define  NTP_CUSTOM_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include "project.h"



#define NTP_RCV_SIZE 100
// Constants
#define NTP_PORT 123
#define NTP_PACKET_SIZE 48
#define NTP_SERVER "time.google.com"  // Replace with a reliable NTP server
#define NTP_EPOCH_OFFSET 2208988800UL // Seconds between 1900 and 1970


#endif