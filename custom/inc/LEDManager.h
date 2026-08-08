#ifndef _LED_MANAGER_H
#define _LED_MANAGER_H

#include <stdint.h>

// LED State IDs - Each represents a specific system state
typedef enum {
    // GSM States (0-9)
    LEDSTATE_GSM_INIT = 0,
    LEDSTATE_GSM_SEARCHING,
    LEDSTATE_GSM_REGISTERED,
    LEDSTATE_GSM_GPRS_READY,
    LEDSTATE_GSM_CONNECTED,      // Socket connected
    LEDSTATE_GSM_PROFILE_SWITCH, // Profile switching in progress
    LEDSTATE_GSM_REG_DENIED,
    LEDSTATE_GSM_SLEEP,
    
    // GPS States (10-19)
    LEDSTATE_GPS_SEARCHING = 10,
    LEDSTATE_GPS_FIXED,
    LEDSTATE_GPS_OFF,
    LEDSTATE_GPS_SLEEP,
    
    // Battery States (20-29)
    LEDSTATE_BATT_LOW = 20,
    LEDSTATE_BATT_NORMAL,
    LEDSTATE_BATT_FAULT,         // battery not connected / faulty → solid ON

    // SOS States (30-39)
    LEDSTATE_SOS_ACTIVE = 30,
    LEDSTATE_SOS_OFF,
    LEDSTATE_SOS_SLEEP,          // SOS LED completely off during sleep
    
    LED_STATE_MAX
} LEDStateID;

// LED Pattern Definition
typedef struct {
    LEDStateID StateID;      // State identifier
    int LEDID;               // Which LED (GSMLED, GPSLED, etc.)
    uint8_t State;           // 1=Enable, 0=Disable
    uint16_t ONTime;         // ON time in units
    uint16_t TotalTime;      // Total cycle time
    uint8_t Priority;        // Lower number = higher priority (for conflict resolution)
} LEDPatternTypedef;

// LED Manager State Structure
typedef struct {
    LEDStateID GSMState;
    LEDStateID GPSState;
    LEDStateID BatteryState;
    LEDStateID SOSState;
} LEDManagerTypedef;

extern LEDManagerTypedef LEDManager;

// Function Prototypes
void LEDManager_Init(void);
void LEDManager_Process(void);  // Main processing loop - call this periodically
void LEDManager_ForceUpdate(void);
void LEDManager_PrintStatus(void);

#endif // _LED_MANAGER_H
