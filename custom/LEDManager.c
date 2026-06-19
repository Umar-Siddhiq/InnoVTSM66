#include "LEDManager.h"
#include "Hardware.h"
#include "VTS.h"
#include "LOG.h"
#include "GPRS.h"
#include "GPS.h"
#include "SOS.h"
#include "TCP.h"

// LED Manager State
LEDManagerTypedef LEDManager = {0};

// LED Pattern table - initialized at runtime in LEDManager_Init()
static LEDPatternTypedef LED_PATTERNS[20];  // Increased from 16 to 20

#define LED_PATTERN_COUNT (sizeof(LED_PATTERNS) / sizeof(LEDPatternTypedef))

// External variables we'll monitor
extern GSM_Typedef GSM;
extern GPS_Typedef GPS;
extern PepheralTypedef PeriPheralVal;
extern SOSTypeDefStruct SOS;
extern SleepConfigTypedef SleepConfig;
extern VTSTypedef VTSData;
extern Providertypedef prfReq;

void LEDManager_Init(void)
{
    Ql_memset(&LEDManager, 0, sizeof(LEDManagerTypedef));
    
    // Initialize LED pattern table at runtime (since GSMLED etc. are variables)
    int idx = 0;
    
    // GSM LED Patterns
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_INIT;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 2;
    LED_PATTERNS[idx].TotalTime = 20;
    LED_PATTERNS[idx].Priority = 0;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_SEARCHING;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 2;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 1;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_REGISTERED;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 5;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 2;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_GPRS_READY;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 9;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 3;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_CONNECTED;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 10;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 4;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_PROFILE_SWITCH;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 3;
    LED_PATTERNS[idx].TotalTime = 6;
    LED_PATTERNS[idx].Priority = 5;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_REG_DENIED;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 1;
    LED_PATTERNS[idx].TotalTime = 2;
    LED_PATTERNS[idx].Priority = 6;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GSM_SLEEP;
    LED_PATTERNS[idx].LEDID = GSMLED;
    LED_PATTERNS[idx].State = 0;
    LED_PATTERNS[idx].ONTime = 1;
    LED_PATTERNS[idx].TotalTime = 15;
    LED_PATTERNS[idx].Priority = 7;
    idx++;
    
    // GPS LED Patterns
    LED_PATTERNS[idx].StateID = LEDSTATE_GPS_SEARCHING;
    LED_PATTERNS[idx].LEDID = GPSLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 3;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 10;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GPS_FIXED;
    LED_PATTERNS[idx].LEDID = GPSLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 10;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 11;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GPS_OFF;
    LED_PATTERNS[idx].LEDID = GPSLED;
    LED_PATTERNS[idx].State = 0;
    LED_PATTERNS[idx].ONTime = 0;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 12;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_GPS_SLEEP;
    LED_PATTERNS[idx].LEDID = GPSLED;
    LED_PATTERNS[idx].State = 0;
    LED_PATTERNS[idx].ONTime = 1;
    LED_PATTERNS[idx].TotalTime = 15;
    LED_PATTERNS[idx].Priority = 9;  // Higher priority than other GPS states
    idx++;
    
    // Battery LED Patterns
    LED_PATTERNS[idx].StateID = LEDSTATE_BATT_LOW;
    LED_PATTERNS[idx].LEDID = BATTERYLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 2;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 20;
    idx++;
    
    LED_PATTERNS[idx].StateID = LEDSTATE_BATT_NORMAL;
    LED_PATTERNS[idx].LEDID = BATTERYLED;
    LED_PATTERNS[idx].State = 0;
    LED_PATTERNS[idx].ONTime = 0;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 21;
    idx++;
    
    // SOS LED Patterns
    LED_PATTERNS[idx].StateID = LEDSTATE_SOS_ACTIVE;
    LED_PATTERNS[idx].LEDID = SOSLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 2;
    LED_PATTERNS[idx].TotalTime = 5;
    LED_PATTERNS[idx].Priority = 30;
    idx++;
    
    #ifdef PROTO_OG // For OG protocol, SOS_OFF is solid ON
    LED_PATTERNS[idx].StateID = LEDSTATE_SOS_OFF;
    LED_PATTERNS[idx].LEDID = SOSLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 10;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 31;
    idx++;
    #else // For other protocols, SOS_OFF is Slow Blink
    LED_PATTERNS[idx].StateID = LEDSTATE_SOS_OFF;
    LED_PATTERNS[idx].LEDID = SOSLED;
    LED_PATTERNS[idx].State = 1;
    LED_PATTERNS[idx].ONTime = 2;
    LED_PATTERNS[idx].TotalTime = 10;
    LED_PATTERNS[idx].Priority = 31;
    #endif
    
    LED_PATTERNS[idx].StateID = LEDSTATE_SOS_SLEEP;
    LED_PATTERNS[idx].LEDID = SOSLED;
    LED_PATTERNS[idx].State = 0;  // Completely off
    LED_PATTERNS[idx].ONTime = 0;
    LED_PATTERNS[idx].TotalTime = 0;
    LED_PATTERNS[idx].Priority = 29;  // Higher priority than SOS_ACTIVE
    idx++;
    
    // Set initial states to OFF/SEARCHING
    LEDManager.GSMState = LEDSTATE_GSM_INIT;
    LEDManager.GPSState = LEDSTATE_GPS_OFF;
    LEDManager.BatteryState = LEDSTATE_BATT_NORMAL;
    LEDManager.SOSState = LEDSTATE_SOS_OFF;
    
    LOGData(TAG_LED, "LED Manager Initialized with %d patterns", idx);
}

// Get pattern definition by state ID
static const LEDPatternTypedef* GetPattern(LEDStateID stateID)
{
    for (int i = 0; i < LED_PATTERN_COUNT; i++)
    {
        if (LED_PATTERNS[i].StateID == stateID)
            return &LED_PATTERNS[i];
    }
    return NULL;
}

// Update LED hardware based on current state with priority handling
static void UpdateLEDHardware(void)
{
    // Build array of current states for each LED
    LEDStateID currentStates[4] = {
        LEDManager.GSMState,
        LEDManager.GPSState,
        LEDManager.BatteryState,
        LEDManager.SOSState
    };
    
    // For each physical LED, find the highest priority pattern to display
    int ledIDs[4] = {GSMLED, GPSLED, BATTERYLED, SOSLED};
    
    for (int led = 0; led < 4; led++)
    {
        const LEDPatternTypedef* bestPattern = NULL;
        uint8_t highestPriority = 255;
        
        // Search for highest priority pattern for this LED
        for (int state = 0; state < 4; state++)
        {
            const LEDPatternTypedef* pattern = GetPattern(currentStates[state]);
            if (pattern && pattern->LEDID == ledIDs[led])
            {
                if (pattern->Priority < highestPriority)
                {
                    highestPriority = pattern->Priority;
                    bestPattern = pattern;
                }
            }
        }
        
        // Apply the best pattern found
        if (bestPattern)
        {
            hw_led_state_set(bestPattern->LEDID, 
                           bestPattern->State, 
                           bestPattern->ONTime, 
                           bestPattern->TotalTime);
        }
    }
}

// Determine GSM LED state based on GSM module state
static LEDStateID DetermineGSMState(void)
{
    // Check registration denied first - highest priority error state
    if (GSM.IsRegDenied)
        return LEDSTATE_GSM_REG_DENIED;
    
    // Check profile switching - high priority indicator
    if (prfReq != NONE)
        return LEDSTATE_GSM_PROFILE_SWITCH;
    
    // Check sleep mode
    if (SleepConfig.IsEnabled)
        return LEDSTATE_GSM_SLEEP;
    
    // Check if socket 0 or socket 2 (indices 0 and 2) are connected - highest priority normal state
    if (ServerSocket[0].SocketState == SOCKET_CONNECTED || 
        ServerSocket[2].SocketState == SOCKET_CONNECTED)
        return LEDSTATE_GSM_CONNECTED;
    
    // Check GSM state
    switch (GSM.GSMState)
    {
        case SIM_NOT_DETECTED:
            return LEDSTATE_GSM_SEARCHING;
            
        case SIM_DETECTED:
            return LEDSTATE_GSM_SEARCHING;
            
        case GPRS_INIT:
            return LEDSTATE_GSM_REGISTERED;
            
        case GPRS_ACTIVE:
            return LEDSTATE_GSM_GPRS_READY;
            
        default:
            return LEDSTATE_GSM_INIT;
    }
}

// Determine GPS LED state based on GPS module state
static LEDStateID DetermineGPSState(void)
{
    // Check sleep mode first - highest priority
    if (SleepConfig.IsEnabled)
        return LEDSTATE_GPS_SLEEP;
    
    if (GPS.State == 0)
        return LEDSTATE_GPS_OFF;
    
    if (GPS.GPSFix)
        return LEDSTATE_GPS_FIXED;
    else
        return LEDSTATE_GPS_SEARCHING;
}

// Determine Battery LED state based on battery voltage
static LEDStateID DetermineBatteryState(void)
{
    if (PeriPheralVal.BattVolt < VTSData.BattThrs)
        return LEDSTATE_BATT_LOW;
    else
        return LEDSTATE_BATT_NORMAL;
}

// Determine SOS LED state based on SOS status
static LEDStateID DetermineSOSState(void)
{
    // Turn off SOS LED completely if sleep mode is enabled
    if (SleepConfig.IsEnabled)
        return LEDSTATE_SOS_SLEEP;
    
    if (SOS.IsSOS)
        return LEDSTATE_SOS_ACTIVE;
    else
        return LEDSTATE_SOS_OFF;
}

// Main processing function - call this periodically (e.g., every 200ms)
void LEDManager_Process(void)
{
    LEDStateID newGSMState = DetermineGSMState();
    LEDStateID newGPSState = DetermineGPSState();
    LEDStateID newBatteryState = DetermineBatteryState();
    LEDStateID newSOSState = DetermineSOSState();
    
    uint8_t changed = 0;
    
    // Check if any state has changed
    if (LEDManager.GSMState != newGSMState)
    {
        LOGData(TAG_LED, "GSM LED State: %d -> %d", LEDManager.GSMState, newGSMState);
        LEDManager.GSMState = newGSMState;
        changed = 1;
    }
    
    if (LEDManager.GPSState != newGPSState)
    {
        LOGData(TAG_LED, "GPS LED State: %d -> %d", LEDManager.GPSState, newGPSState);
        LEDManager.GPSState = newGPSState;
        changed = 1;
    }
    
    if (LEDManager.BatteryState != newBatteryState)
    {
        LOGData(TAG_LED, "Battery LED State: %d -> %d", LEDManager.BatteryState, newBatteryState);
        LEDManager.BatteryState = newBatteryState;
        changed = 1;
    }
    
    if (LEDManager.SOSState != newSOSState)
    {
        LOGData(TAG_LED, "SOS LED State: %d -> %d", LEDManager.SOSState, newSOSState);
        LEDManager.SOSState = newSOSState;
        changed = 1;
    }
    
    // Only update hardware if something changed
    if (changed)
    {
        UpdateLEDHardware();
    }
}

// Force update all LEDs (useful for recovering from edge cases)
void LEDManager_ForceUpdate(void)
{
    LOGData(TAG_LED, "Force updating all LEDs");
    UpdateLEDHardware();
}

// Get current state summary for debugging
void LEDManager_PrintStatus(void)
{
    LOGData(TAG_LED, "LED Manager Status:");
    LOGData(TAG_LED, "  GSM: %d (Module State: %d)", LEDManager.GSMState, GSM.GSMState);
    LOGData(TAG_LED, "  GPS: %d (Fix: %d, State: %d)", LEDManager.GPSState, GPS.GPSFix, GPS.State);
    LOGData(TAG_LED, "  Battery: %d (Volt: %.2f)", LEDManager.BatteryState, PeriPheralVal.BattVolt);
    LOGData(TAG_LED, "  SOS: %d (Active: %d)", LEDManager.SOSState, SOS.IsSOS);
}
