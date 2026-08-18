// ============================================================
// File: LSM6DSOX.c
// Description: Source file for LSM6DSOX IMU sensor interface using SPI
// Author: Vyshak
// Date: 30-Oct-2025
// ============================================================

#include "LSM6DSOX.h"
#include "Hardware.h"
#include "NuMicro.h"
#include "W25Qxx.h"
#include <math.h>



// ====== Public Functions ======
void LSM6DSOX_Init(void)
{
		SPI_Open(SPI0, SPI_MASTER, SPI_MODE_0, 8, SPI_CLK_FREQ);
    // Configure CS pin PF4 as output
    GPIO_SetMode(LSM6DSOX_CS_PORT, LSM6DSOX_CS_PIN, GPIO_MODE_OUTPUT);
    
    // Set CS high initially (inactive)
    LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;
    
    // Read WHO_AM_I to verify device
    LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;  // CS low
    SPI_FLASH_SendByte(0x8F);  // Read WHO_AM_I (0x0F | 0x80)
    uint8_t id = SPI_FLASH_SendByte(0x00);  // Read response
    LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;   // CS high
    
    // TODO: Initialize SPI interface, reset sensor, configure control registers
		if(id == 0x6A) 
		{
			// Device identified, perform full initialization
			
			// 1. Software reset (CTRL3_C - 0x12)
			LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;
			SPI_FLASH_SendByte(0x12);  // Write to CTRL3_C
			SPI_FLASH_SendByte(0x01);  // SW_RESET = 1
			LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;
			
			// Small delay for reset to complete
			for(volatile int i = 0; i < 1000; i++);
			
			// 2. Configure accelerometer (CTRL1_XL - 0x10)
			LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;
			SPI_FLASH_SendByte(0x10);  // Write to CTRL1_XL
			SPI_FLASH_SendByte(0x50);  // 416Hz, ±8g, LP filter disabled
			LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;
			
			// 3. Configure CTRL3_C for proper operation (0x12)
			LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;
			SPI_FLASH_SendByte(0x12);  // Write to CTRL3_C
			SPI_FLASH_SendByte(0x44);  // BDU=1, IF_INC=1 (block data update, auto increment)
			LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;
			
			// 4. Read 20 samples and average them
			float x_sum = 0, y_sum = 0, z_sum = 0;
			const int num_samples = 20;
			
			for(int sample = 0; sample < num_samples; sample++) 
			{
					LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;  // CS low
					SPI_FLASH_SendByte(0xA8);  // Read from OUTX_L_A (0x28) with auto-increment
					
					uint8_t accel_data[6];
					for(int i = 0; i < 6; i++) {
							accel_data[i] = SPI_FLASH_SendByte(0x00);
					}
					LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;   // CS high
					
					// Convert raw data to 16-bit values
					int16_t x_raw = (int16_t)((accel_data[1] << 8) | accel_data[0]);
					int16_t y_raw = (int16_t)((accel_data[3] << 8) | accel_data[2]);
					int16_t z_raw = (int16_t)((accel_data[5] << 8) | accel_data[4]);
					
					// Convert to g-forces and accumulate
					x_sum += (float)x_raw / 4096.0f;
					y_sum += (float)y_raw / 4096.0f;
					z_sum += (float)z_raw / 4096.0f;
					
					// Small delay between samples
					for(volatile int i = 0; i < 100; i++);
			}
				
				// Calculate averages
				float x_g = x_sum / num_samples;
				float y_g = y_sum / num_samples;
				float z_g = z_sum / num_samples;
				
				// Calculate tilt angle
				float tilt_angle = acos(fabs(z_g)) * 180.0 / M_PI;
				
				if (tilt_angle > 30.0) 
				{
						// Tilt greater than 30 degrees - add your code here
				}
		 }
}
void LSM6DSOX_MonitorTilt(void)
{
    float x_sum = 0, y_sum = 0, z_sum = 0;
    const int num_samples = 10; // Reduced for faster response
    
    for(int sample = 0; sample < num_samples; sample++) 
    {
        LSM6DSOX_CS_PORT->DOUT &= ~LSM6DSOX_CS_PIN;  // CS low
        SPI_FLASH_SendByte(0xA8);  // Read from OUTX_L_A (0x28) with auto-increment
        
        uint8_t accel_data[6];
        for(int i = 0; i < 6; i++) {
            accel_data[i] = SPI_FLASH_SendByte(0x00);
        }
        LSM6DSOX_CS_PORT->DOUT |= LSM6DSOX_CS_PIN;   // CS high
        
        // Convert raw data to 16-bit values
        int16_t x_raw = (int16_t)((accel_data[1] << 8) | accel_data[0]);
        int16_t y_raw = (int16_t)((accel_data[3] << 8) | accel_data[2]);
        int16_t z_raw = (int16_t)((accel_data[5] << 8) | accel_data[4]);
        
        // Convert to g-forces and accumulate
        x_sum += (float)x_raw / 4096.0f;
        y_sum += (float)y_raw / 4096.0f;
        z_sum += (float)z_raw / 4096.0f;
        
        // Small delay between samples
        for(volatile int i = 0; i < 50; i++);
    }
    
    // Calculate averages
    float x_g = x_sum / num_samples;
    float y_g = y_sum / num_samples;
    float z_g = z_sum / num_samples;
    
    // Calculate tilt angle
    float tilt_angle = atan2(sqrt(x_g*x_g + y_g*y_g), fabs(z_g)) * 180.0 / M_PI;
    
    if (tilt_angle > 30.0) 
		{
        PeriPheralVal.IsTilt=1;
		}
		else
		{
				PeriPheralVal.IsTilt=0;
		}
}