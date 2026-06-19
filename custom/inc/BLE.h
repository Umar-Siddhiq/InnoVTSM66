#ifndef BLE_H
#define BLE_H

#include "VTS.h"
#include "ril_bluetooth.h"
#include "GPRS.h"
#include "Systic.h"

#define BT_MAXBTDEV_CNT  (5)
#define BL_RX_BUFFER_LEN (512)
#define BL_PAYLOAD_SIZE (512) // Maximum payload size for Bluetooth data transfer
#define BL_DEF_PIN        "010203"
#define BL_INVALID_SLOT (0xFF)
#define BL_VISIBLE_TIMEOUT_SEC (20) // 20 seconds for visible timeout

typedef enum BLDeviceState {
    BL_DEVICE_DISCONNECTED = 0,
    BL_DEVICE_CONNECTED,
    BL_DEVICE_PENDING_RCV,
    BL_DEVICE_PENDING_DECODE
}BLDeviceState;

typedef struct BTDevice {
    BLDeviceState state; // Device state
    char name[BT_NAME_LEN];
    char address[BT_ADDR_LEN];
    BT_DEV_HDL handle;
    uint8_t *pData; // Pointer to data buffer
    u32 dataLen; // Length of data buffer
} BTDevice;

typedef struct BLUETOOTH{
    char name[BT_NAME_LEN];
    char address[BT_ADDR_LEN];
    uint8_t IsPoweredOn;
    uint8_t IsVisible;
    uint8_t ConnectionsCount;
    uint8_t IsBonded;
    BTDevice devices[BT_MAXBTDEV_CNT];
    char pinCode[BT_PIN_LEN];
} BLUETOOTH;

#define BTCONFIG_SIZE   sizeof(BLUETOOTH)


uint8_t BLE_Init(void);
uint8_t BLE_PowerOff(void);
uint8_t BLE_SetVisible(uint8_t visible);
uint8_t BLE_AddDevice(ST_BT_BasicInfo *dev);
uint8_t BLE_RemoveDevice(ST_BT_BasicInfo *dev);
uint8_t BLE_SetPending(ST_BT_BasicInfo* dev, uint8_t pending);
uint8_t BLE_CheckPendingDecode(void);
uint8_t BLE_ReadDevice(uint8_t slot);
uint8_t BLE_SendData(uint8_t slot, const char* data, uint16_t len);
void BLE_SendReply(uint8_t *data, int len);
uint8_t BLE_CheckPendingRcv(void);
void BLE_ClearPendingDecode(uint8_t slot);
void BLE_DecodeComplete(void);

#ifdef BLE_THREAD_ENABLE
void BLEThreadEntry(s32 taskId);
void ble_thread_init(u32 taskId);
#else
uint8_t ProcessBLE(void);
#endif

extern BLUETOOTH BTMaster;
extern uint8_t BTRcvBuffer[];
extern uint16_t BTRcvBufferLen;
extern uint8_t BTRcvSlot;


#endif //BLE_H