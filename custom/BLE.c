#include "custom_feature_def.h"

#if VTS_BLE_ENABLE

#include "BLE.h"
#include "Hardware.h"

BLUETOOTH BTMaster={{0}};
uint8_t BTRcvBuffer[BL_RX_BUFFER_LEN] = {0};
uint16_t BTRcvBufferLen = 0;
uint8_t BTRcvSlot=0;

uint8_t BLE_IsInitialized = 0;
uint32_t BLE_LastProcessTime = 0;
#define BLE_PROCESS_INTERVAL 50  // Minimum time between process calls in ms

uint8_t BLE_Init(void);
uint8_t BLE_PowerOff(void);

static void BT_Callback(s32 event, s32 errCode, void* param1, void* param2)
{
    //ST_BT_BasicInfo *pstNewBtdev = NULL;
    ST_BT_BasicInfo *pstconBtdev = NULL;
    s32 ret = RIL_AT_SUCCESS;
    //s32 connid = -1;
    LOGData(TAG_BLE,"BT_Callback: event=%d, errCode=%d\r\n",event,errCode);
    switch(event)
    {
        case MSG_BT_SCAN_IND :

        //     if(URC_BT_SCAN_FINISHED== errCode)
        //     {
        //         LOGData(TAG_BLE,"Scan is over.\r\n");
        //         //LOGData(TAG_BLE,"Pair/Connect if need.\r\n");
        //         RIL_BT_GetDevListInfo();
        //         //here obtain ril layer table list for later use
        //         g_dev_info = RIL_BT_GetDevListPointer();
        //         g_pair_search = TRUE;
        //     }
        //     if(URC_BT_SCAN_FOUND == errCode)
        //     {
        //         pstNewBtdev = (ST_BT_BasicInfo *)param1;
        //         //you can manage the scan device here,or you don't need to manage it ,for ril layer already handle it
        //         LOGData(TAG_BLE,"BTHdl[0x%08x] Addr[%s] Name[%s]\r\n",pstNewBtdev->devHdl,pstNewBtdev->addr,pstNewBtdev->name);
                
        //     }
        //     break;
        case MSG_BT_PAIR_IND :
            if(URC_BT_NEED_PASSKEY == errCode)
            {
                //must ask for pincode;
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                LOGData(TAG_BLE,"Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                LOGData(TAG_BLE,"Pair device addr: %s\r\n",pstconBtdev->addr);
                LOGData(TAG_BLE,"Waiting for pair confirm with pinCode...\r\n");
                RIL_BT_PairConfirm(TRUE,BTMaster.pinCode); // Use default pin code
            }
            else if(URC_BT_NO_NEED_PASSKEY == errCode)
            {               
                //no need pincode
                char pinCode[BT_PIN_LEN] = {0};
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                Ql_strncpy(pinCode,(char*)param2,BT_PIN_LEN);
                pinCode[BT_PIN_LEN-1] = 0; // Ensure null termination
                LOGData(TAG_BLE,"Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                LOGData(TAG_BLE,"Pair device addr: %s\r\n",pstconBtdev->addr);
                LOGData(TAG_BLE,"Pair pin code: %s\r\n",pinCode);
                LOGData(TAG_BLE,"pair confirm automatically\r\n");
                RIL_BT_PairConfirm(TRUE,NULL);
            }  

            break;
        case MSG_BT_PAIR_CNF_IND :
            if(URC_BT_PAIR_CNF_SUCCESS == errCode)
            {
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                LOGData(TAG_BLE,"Paired successful.\r\n");
            }
            else
            {
                LOGData(TAG_BLE,"Paired failed.\r\n");
            }            
            break;

        case MSG_BT_SPP_CONN_IND :
            
            if(URC_BT_CONN_SUCCESS == errCode )
            {
                BLE_AddDevice((ST_BT_BasicInfo *)param1); 
                LOGData(TAG_BLE,"Connect successful.\r\n");
            }
            else
            {
                LOGData(TAG_BLE,"Connect failed.\r\n");
            }
        
            break;

        case MSG_BT_RECV_IND :

            //connid = *(s32 *)param1;
            pstconBtdev = (ST_BT_BasicInfo *)param2;
            LOGData(TAG_BLE,"SPP receive data from BTHdl[0x%08x].\r\n",pstconBtdev->devHdl);
            BLE_SetPending(pstconBtdev, 1); // Set device as pending for processing
            break; 

        case MSG_BT_PAIR_REQ:

            if(URC_BT_NEED_PASSKEY == errCode)
            {
                //must ask for pincode;
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                LOGData(TAG_BLE,"Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                LOGData(TAG_BLE,"Pair device addr: %s\r\n",pstconBtdev->addr);
                LOGData(TAG_BLE,"Waiting for pair confirm with pinCode...\r\n");
                RIL_BT_PairConfirm(TRUE,BTMaster.pinCode); // Use default pin code
            }

            if(URC_BT_NO_NEED_PASSKEY == errCode)
            {
                //no need pincode
                char pinCode[BT_PIN_LEN] = {0};
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                Ql_strncpy(pinCode,(char*)param2,BT_PIN_LEN);
                pinCode[BT_PIN_LEN-1] = 0; // Ensure null termination
                LOGData(TAG_BLE,"Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                LOGData(TAG_BLE,"Pair device addr: %s\r\n",pstconBtdev->addr);
                LOGData(TAG_BLE,"Pair pin code: %s\r\n",pinCode);
                LOGData(TAG_BLE,"pair confirm automatically\r\n");
                RIL_BT_PairConfirm(TRUE,NULL);
            }  

            break;

        case  MSG_BT_CONN_REQ :
            
            pstconBtdev = (ST_BT_BasicInfo*)param1;
            LOGData(TAG_BLE,"Get a connect req\r\n");
            LOGData(TAG_BLE,"BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
            LOGData(TAG_BLE,"Addr: %s\r\n",pstconBtdev->addr);
            LOGData(TAG_BLE,"Name: %s\r\n",pstconBtdev->name);

            LOGData(TAG_BLE,"Waiting connect accept.\r\n");    
            if(BTMaster.ConnectionsCount < BT_MAXBTDEV_CNT)
            {
                // Accept the connection request
                ret = RIL_BT_ConnAccept(TRUE, 1);
                if(ret == RIL_AT_SUCCESS)
                {
                    LOGData(TAG_BLE,"Connection accepted.\r\n");
                }
                else
                {
                    LOGData(TAG_BLE,"Failed to accept connection, ret: %d\r\n", ret);
                }
            }
            else
            {
                LOGData(TAG_BLE,"Max connections reached, cannot accept new connection.\r\n");
            }
            break;

        case  MSG_BT_DISCONN_IND :

            pstconBtdev = (ST_BT_BasicInfo*)param1;
            if(URC_BT_DISCONNECT_PASSIVE == errCode || URC_BT_DISCONNECT_POSITIVE == errCode)
            {
                // Handle disconnection
                BLE_RemoveDevice(pstconBtdev); // Remove device from the list
                LOGData(TAG_BLE,"Disconnect ok!\r\n");
            }

             
          break;
 
             
        default :
            break;
    }
}
uint8_t BLE_Init(void)
{
    int ret = -1;
    s32 pw = 0;
    const char *vendorId = (VTSData.VendorID[0] != '\0') ? VTSData.VendorID : DEFAULT_VENDOR;
    const char *imei = (NetWork.IMEI[0] != '\0') ? NetWork.IMEI : "NOIMEI";

    // Initialize name
    /* OLD CODE: Ql_sprintf(BTMaster.name, "APM-%s", NetWork.IMEI); */
    Ql_memset(BTMaster.name, 0, sizeof(BTMaster.name));
    Ql_snprintf(BTMaster.name, sizeof(BTMaster.name) - 1, "%s-%s", vendorId, imei);
    BTMaster.name[sizeof(BTMaster.name) - 1] = '\0';
    LOGData(TAG_BLE, "Bluetooth name set from device VendorID: %s", BTMaster.name);

    // Check power state
    ret = RIL_BT_GetPwrState(&pw);
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to get Bluetooth power state, ret: %d", ret);
        return 0;
    }

    // Power cycle only if necessary
    if (pw == 1) {
        ret = RIL_BT_Switch(0);
        if (ret != RIL_AT_SUCCESS) {
            LOGData(TAG_BLE, "Failed to power off Bluetooth, ret: %d", ret);
            return 0;
        }
        Ql_Sleep(100); // Reduced sleep time
    }

    // Power on and initialize
    ret = RIL_BT_Switch(1);
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to power on Bluetooth, ret: %d", ret);
        return 0;
    }

    ret = RIL_BT_Initialize(BT_Callback);
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to initialize Bluetooth, ret: %d", ret);
        return 0;
    }

    Ql_Sleep(100); // Reduced sleep time

    // Set up basic configuration
    Ql_strcpy(BTMaster.pinCode, BL_DEF_PIN);
    BTMaster.pinCode[6] = '\0';

    // Verify power state
    ret = RIL_BT_GetPwrState(&pw);
    BTMaster.IsPoweredOn = (ret == RIL_AT_SUCCESS && pw == 1) ? 1 : 0;
    if (!BTMaster.IsPoweredOn) {
        LOGData(TAG_BLE, "Bluetooth Power on failed");
        return 0;
    }

    // Set name and get address
    ret = RIL_BT_SetName(BTMaster.name, Ql_strlen(BTMaster.name));
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to set Bluetooth name, ret: %d", ret);
        return 0;
    }

    ret = RIL_BT_GetLocalAddr(BTMaster.address, BT_ADDR_LEN);
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to get Bluetooth address, ret: %d", ret);
        return 0;
    }

    BTMaster.IsVisible = 0;
    LOGData(TAG_BLE, "Bluetooth initialized successfully");
    return 1;
}

uint8_t BLE_PowerOff(void)
{
    int ret = -1;
    s32 pw = 0;

    // Check if already powered off
    ret = RIL_BT_GetPwrState(&pw);
    if (ret != RIL_AT_SUCCESS || pw == 0) {
        BTMaster.IsPoweredOn = 0;
        BLE_IsInitialized = 0;
        LOGData(TAG_BLE, "Bluetooth already powered off");
        return 1;
    }

    // Disconnect all devices first
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state > BL_DEVICE_DISCONNECTED)
        {
            RIL_BT_Disconnect(BTMaster.devices[i].handle);
            BTMaster.devices[i].state = BL_DEVICE_DISCONNECTED;
            if(BTMaster.devices[i].pData != NULL) {
                Ql_MEM_Free(BTMaster.devices[i].pData);
                BTMaster.devices[i].pData = NULL;
            }
        }
    }
    BTMaster.ConnectionsCount = 0;

    // Power off Bluetooth
    ret = RIL_BT_Switch(0);
    if (ret != RIL_AT_SUCCESS) {
        LOGData(TAG_BLE, "Failed to power off Bluetooth, ret: %d", ret);
        return 0;
    }

    BTMaster.IsPoweredOn = 0;
    BTMaster.IsVisible = 0;
    BLE_IsInitialized = 0;
    LOGData(TAG_BLE, "Bluetooth powered off successfully");
    return 1;
}

uint8_t BLE_SetVisible(uint8_t on)
{
    s32 ret = -1;
    if(on != 1)
    {
        BTMaster.IsVisible = 0; // Update visibility state
        ret = RIL_BT_SetVisble(BT_INVISIBLE, BL_VISIBLE_TIMEOUT_SEC); // Set Bluetooth invisible
        if(ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_BLE,"Failed to set Bluetooth invisible, ret: %d", ret);
            return 0;
        }
        return 1;

    }
    ret = RIL_BT_SetVisble(BT_VISIBLE_FOREVER, BL_VISIBLE_TIMEOUT_SEC); // Set Bluetooth visible forever
    if(ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_BLE,"Failed to set Bluetooth visibility, ret: %d", ret);
        return 0;
    }
    LOGData(TAG_BLE,"Bluetooth visibility set successfully.");
    BTMaster.IsVisible = 1; // Update visibility state
    return 1;
}

uint8_t BLE_AddDevice(ST_BT_BasicInfo *dev)
{
    if(dev == NULL)
    {
        LOGData(TAG_BLE,"Invalid device information.");
        return 0;
    }
    if(BTMaster.ConnectionsCount >= BT_MAXBTDEV_CNT)
    {
        LOGData(TAG_BLE,"Device list is full, cannot add more devices.");
        return 0;
    }
    uint8_t slot=BL_INVALID_SLOT;
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state == BL_DEVICE_DISCONNECTED)
        {
            slot = i;
        }
        if(BTMaster.devices[i].handle == dev->devHdl && BTMaster.devices[i].state > BL_DEVICE_DISCONNECTED)
        {
            LOGData(TAG_BLE,"Device already exists in the list.");
            return 0; // Device already exists
        }
    }
    if(slot == BL_INVALID_SLOT)
    {
        LOGData(TAG_BLE,"No available slot to add new device.");
        return 0; // No available slot
    }
    BTMaster.devices[slot].state = BL_DEVICE_CONNECTED; // Mark device as connected
    Ql_strncpy(BTMaster.devices[slot].name, dev->name, BT_NAME_LEN);
    Ql_strncpy(BTMaster.devices[slot].address, dev->addr, BT_ADDR_LEN);
    BTMaster.devices[slot].handle = dev->devHdl;
    BTMaster.ConnectionsCount++; // Increment connection count
    LOGData(TAG_BLE,"Device added successfully, Name: %s, Handle:%lu, Total Connections: %d",
            BTMaster.devices[slot].name,BTMaster.devices[slot].handle, BTMaster.ConnectionsCount);
    return 1;
}

uint8_t BLE_RemoveDevice(ST_BT_BasicInfo *dev)
{
    if(dev == NULL)
    {
        LOGData(TAG_BLE,"Invalid device information.");
        return 0;
    }
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state > BL_DEVICE_DISCONNECTED && 
           BTMaster.devices[i].handle == dev->devHdl)
        {
            BTMaster.devices[i].state = BL_DEVICE_DISCONNECTED; // Mark device as disconnected
            Ql_memset(&BTMaster.devices[i], 0, sizeof(BTDevice)); // Clear device data
            LOGData(TAG_BLE,"Device removed successfully.");
            BTMaster.ConnectionsCount--; // Decrement connection count
            return 1; // Device removed successfully
        }
    }
    LOGData(TAG_BLE,"Device not found in the list.");
    return 0; // Device not found
}

uint8_t BLE_SetPending(ST_BT_BasicInfo* dev, uint8_t pending)
{
    if(dev == NULL)
    {
        LOGData(TAG_BLE,"Invalid device information.");
        return 0;
    }
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state == BL_DEVICE_CONNECTED && 
           BTMaster.devices[i].handle == dev->devHdl)
        {
            BTMaster.devices[i].state = BL_DEVICE_PENDING_RCV; // Set pending state
            LOGData(TAG_BLE,"Device pending state updated successfully.");
            return 1; // Pending state updated successfully
        }
    }
    LOGData(TAG_BLE,"Device not found in the list.");
    return 0; // Device not found
}

uint8_t BLE_CheckPendingRcv(void)
{
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state == BL_DEVICE_PENDING_RCV)
        {
            LOGData(TAG_BLE,"Device pending found, handle: %lu", BTMaster.devices[i].handle);
            BLE_ReadDevice(i);
            return 1; // Pending device found
        }
    }
   // LOGData(TAG_BLE,"No pending devices found.");
    return 0; // No pending devices
}

uint8_t BLE_ReadDevice(uint8_t slot)
{
    u8 btread[BL_RX_BUFFER_LEN] = {0};
    int ret=-1;
    if(slot >= BT_MAXBTDEV_CNT)
    {
        LOGData(TAG_BLE,"Invalid slot index: %d", slot);
        return 0; // Invalid slot index
    }
    if(BTMaster.devices[slot].state == BL_DEVICE_DISCONNECTED)
    {
        LOGData(TAG_BLE,"No device connected at slot: %d", slot);
        return 0; // No device connected
    }
    if(BTMaster.devices[slot].state != BL_DEVICE_PENDING_RCV)
    {
        LOGData(TAG_BLE,"Device at slot: %d is not pending rcv, cannot read.", slot);
        return 0; // Device is not pending
    }
    ret = RIL_BT_SPP_Read(BTMaster.devices[slot].handle, 
                        btread, 
                        BL_RX_BUFFER_LEN, 
                        &BTMaster.devices[slot].dataLen);

    if(ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_BLE,"Failed to read from device, ret: %d", ret);
        return 0; // Read failed
    }
    if(BTMaster.devices[slot].dataLen == 0)
    {
        LOGData(TAG_BLE,"No data to read from device at slot: %d", slot);
        return 0; // No data read
    }
    if(BTMaster.devices[slot].pData != NULL) {
        Ql_MEM_Free(BTMaster.devices[slot].pData);
    }
    BTMaster.devices[slot].pData = (uint8_t *)Ql_MEM_Alloc(BTMaster.devices[slot].dataLen);
    if(BTMaster.devices[slot].pData == NULL)
    {
        LOGData(TAG_BLE,"Memory allocation failed for device data.");
        return 0; // Memory allocation failed
    }
    Ql_memcpy(BTMaster.devices[slot].pData, btread, BTMaster.devices[slot].dataLen);
    LOGData(TAG_BLE,"Device read successful, slot: %d, data length: %d", slot, BTMaster.devices[slot].dataLen);

    BTMaster.devices[slot].state = BL_DEVICE_PENDING_DECODE; // Update device state to pending decode

    
    return 1; // Device read successfully
}

uint8_t BLE_CheckPendingDecode(void)
{
    for(int i = 0; i < BT_MAXBTDEV_CNT; i++)
    {
        if(BTMaster.devices[i].state == BL_DEVICE_PENDING_DECODE)
        {
            LOGData(TAG_BLE,"Device pending decode found, handle: %lu", BTMaster.devices[i].handle);
            memcpy(BTRcvBuffer, BTMaster.devices[i].pData, BTMaster.devices[i].dataLen);
            BTRcvBufferLen = BTMaster.devices[i].dataLen;
            BTRcvSlot=i;
            return 1;
        }
    }
    return 0;
}


void BLE_ClearPendingDecode(uint8_t slot)
{
    if(slot >= BT_MAXBTDEV_CNT) {
        LOGData(TAG_BLE,"Invalid slot index: %d", slot);
        return;
    }
    
    if(BTMaster.devices[slot].pData != NULL) {
        Ql_MEM_Free(BTMaster.devices[slot].pData);
        BTMaster.devices[slot].pData = NULL;
    }
    
    // Change: Set state back to CONNECTED instead of DISCONNECTED
    BTMaster.devices[slot].state = BL_DEVICE_CONNECTED;  // <-- Changed from DISCONNECTED
    
    LOGData(TAG_BLE,"Cleared pending decode, device ready for new data");
}

void BLE_DecodeComplete(void)
{
    if (BTRcvSlot >= BT_MAXBTDEV_CNT) {
        return;
    }
    
    // Change: Set state back to CONNECTED instead of DISCONNECTED
    BTMaster.devices[BTRcvSlot].state = BL_DEVICE_CONNECTED;  // <-- Changed from DISCONNECTED
    
    if(BTMaster.devices[BTRcvSlot].pData != NULL) {
        Ql_MEM_Free(BTMaster.devices[BTRcvSlot].pData);
        BTMaster.devices[BTRcvSlot].pData = NULL;
    }
    
    BTRcvBufferLen = 0;
    BTRcvSlot = 0;
    memset(BTRcvBuffer, 0, BL_RX_BUFFER_LEN);
    
    LOGData(TAG_BLE, "Decode complete, device ready for new data");
}


uint8_t BLE_SendData(uint8_t slot, const char* data, uint16_t len)
{
    if(slot >= BT_MAXBTDEV_CNT)
    {
        LOGData(TAG_BLE,"Invalid slot index: %d", slot);
        return 0; // Invalid slot index
    }
    if(BTMaster.devices[slot].state == BL_DEVICE_DISCONNECTED)
    {
        LOGData(TAG_BLE,"No device connected at slot: %d", slot);
        return 0; // No device connected
    }
    if(data == NULL || len == 0)
    {
        LOGData(TAG_BLE,"Invalid data to send.");
        return 0; // Invalid data
    }
    if(len > BL_PAYLOAD_SIZE) {
        LOGData(TAG_BLE,"Data too large to send");
        return 0;
    }
    int ret = RIL_BT_SPP_Send(BTMaster.devices[slot].handle, (u8*)data, len, NULL);
    if(ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_BLE,"Failed to send data, ret: %d", ret);
        return 0; // Send failed
    }
    LOGData(TAG_BLE,"Data sent successfully to device at slot: %d", slot);
    return 1; // Data sent successfully
}

void BLE_SendReply(uint8_t *data, int len)
{
    if(data == NULL || len <= 0 || BTRcvSlot >= BT_MAXBTDEV_CNT)
    {
        LOGData(TAG_BLE,"Invalid data or slot for reply.");
        return; // Invalid data or slot
    }
    if(BTMaster.devices[BTRcvSlot].state != BL_DEVICE_PENDING_DECODE)
    {
        LOGData(TAG_BLE,"Device at slot: %d is not in pending decode state.", BTRcvSlot);
        return; // Device is not in pending decode state
    }
    int ret = BLE_SendData(BTRcvSlot, (const char*)data, len);
    if(ret == 0)
    {
        LOGData(TAG_BLE,"Failed to send reply data.");
    }
}


/**
 * @brief Process BLE operations asynchronously
 * @return 1 if processing occurred, 0 if skipped due to timing
 */
uint8_t ProcessBLE(void)
{
    // Check if sleep mode is enabled - if so, power off BLE
    if (SleepConfig.IsEnabled) {
        if (BLE_IsInitialized && BTMaster.IsPoweredOn) {
            LOGData(TAG_BLE, "Sleep mode enabled, powering off BLE");
            BLE_PowerOff();
        }
        return 0;
    }

    // Check if minimum interval has passed
    uint32_t currentTime = Ql_GetMsSincePwrOn();
    if ((currentTime - BLE_LastProcessTime) < BLE_PROCESS_INTERVAL) {
        return 0;
    }
    BLE_LastProcessTime = currentTime;

    // Initialize BLE if needed
    if (!BLE_IsInitialized) {
        if (BLE_Init()) {
            BLE_IsInitialized = 1;
        } else {
            return 0;
        }
    }

    // Ensure visibility
    if (BTMaster.IsVisible == 0) {
        if (!BLE_SetVisible(1)) {
            return 0;
        }
    }

    // Process pending operations
    if (BTMaster.ConnectionsCount > 0) {
        // Check for pending decode if receive buffer is empty
        if (!BTRcvBufferLen) {
            BLE_CheckPendingDecode();
        }

        // Check for pending receives
        BLE_CheckPendingRcv();
    }

    return 1;
}

#ifdef BLE_THREAD_ENABLE
void BLEThreadEntry(s32 taskId)
{
    ble_thread_init(taskId);
    ThreadSleep(3000);
    Ql_memset(&BTMaster, 0, sizeof(BLUETOOTH));
    LOGData(TAG_BLE,"BLE Thread Entry, Task ID: %d", taskId);
    while(1)
    {
        // Check if sleep mode is enabled - if so, power off BLE and wait
        if (SleepConfig.IsEnabled) {
            if (BTMaster.IsPoweredOn) {
                LOGData(TAG_BLE, "Sleep mode enabled, powering off BLE");
                BLE_PowerOff();
            }
            ThreadSleep(1000); // Wait while in sleep mode
            continue;
        }

        if(BTMaster.IsPoweredOn == 0)
        {
            if(!BLE_Init())
            {
                ThreadSleep(2000);
                continue;
            }
        }
        if(BTMaster.IsVisible == 0)
        {
            if(!BLE_SetVisible(1))
            {
                ThreadSleep(2000);
                continue;
            }
        }
        if(BTMaster.ConnectionsCount < 1)
        {
            //LOGData(TAG_BLE,"No Bluetooth devices connected, waiting for connections...");
            ThreadSleep(50); // Wait for connections
            continue;
        }
        if(!BTRcvBufferLen)
            BLE_CheckPendingDecode();

        BLE_CheckPendingRcv();

        ThreadSleep(50); // Sleep to avoid busy waiting
        
    }
}

void ble_thread_init(u32 taskId)
{
    s32 ret;
    OSThread BLE_Thread = {0};
    BLE_Thread.taskId = taskId;
    Ql_strcpy(BLE_Thread.taskName, "BLE Thread");
    BLE_Thread.taskEnable = 1;
    BLE_Thread.taskState = TASK_STATE_NORMAL;
    BLE_Thread.taskPriority = 1;
    ret = InitializeThread(&BLE_Thread);
    if (ret != 1)
    {
        LOGData(TAG_BLE, "Failed to initialize BLE thread");
        return;
    }
    LOGData(TAG_BLE, "BLE thread initialized successfully");
}
#endif

#else  // VTS_BLE_ENABLE

#include "BLE.h"

BLUETOOTH BTMaster={{0}};
uint8_t BTRcvBuffer[BL_RX_BUFFER_LEN] = {0};
uint16_t BTRcvBufferLen = 0;
uint8_t BTRcvSlot = 0;

uint8_t BLE_IsInitialized = 0;
uint32_t BLE_LastProcessTime = 0;

uint8_t BLE_Init(void) { return 0; }
uint8_t BLE_PowerOff(void) { return 0; }
uint8_t BLE_SetVisible(uint8_t visible) { (void)visible; return 0; }
uint8_t BLE_AddDevice(ST_BT_BasicInfo *dev) { (void)dev; return 0; }
uint8_t BLE_RemoveDevice(ST_BT_BasicInfo *dev) { (void)dev; return 0; }
uint8_t BLE_SetPending(ST_BT_BasicInfo* dev, uint8_t pending) { (void)dev; (void)pending; return 0; }
uint8_t BLE_CheckPendingDecode(void) { return 0; }
uint8_t BLE_ReadDevice(uint8_t slot) { (void)slot; return 0; }
uint8_t BLE_SendData(uint8_t slot, const char* data, uint16_t len) { (void)slot; (void)data; (void)len; return 0; }
void BLE_SendReply(uint8_t *data, int len) { (void)data; (void)len; }
uint8_t BLE_CheckPendingRcv(void) { return 0; }
void BLE_ClearPendingDecode(uint8_t slot) { (void)slot; }
void BLE_DecodeComplete(void) { }

#ifdef BLE_THREAD_ENABLE
void BLEThreadEntry(s32 taskId) { (void)taskId; }
void ble_thread_init(u32 taskId) { (void)taskId; }
#else
uint8_t ProcessBLE(void) { return 0; }
#endif

#endif  // VTS_BLE_ENABLE

