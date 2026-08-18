#ifndef __LSM6DSOX_H__
#define __LSM6DSOX_H__

// ============================================================
// File: LSM6DSOX.h
// Description: Header file for LSM6DSOX IMU sensor interface using SPI
// Author: Vyshak
// Date: 30-Oct-2025
// ============================================================

// ====== Includes ======
#include <stdint.h>
#include <stdbool.h>

#define  M_PI  3.14f
// Chip Select Pin Configuration
#define LSM6DSOX_CS_PORT            PF
#define LSM6DSOX_CS_PIN             BIT4  // PF4 for accelerometer CS


// SPI Commands
#define LSM6DSOX_SPI_READ           0x80  // MSB=1 for read
#define LSM6DSOX_SPI_WRITE          0x00  // MSB=0 for write


// Register Addresses (basic)
#define LSM6DSOX_WHO_AM_I_ADDR      0x0F



// ====== Function Prototypes ======
void LSM6DSOX_Init(void);
uint8_t LSM6DSOX_ReadID(void);
void LSM6DSOX_MonitorTilt(void);



#endif /* __LSM6DSOX_H__ */
