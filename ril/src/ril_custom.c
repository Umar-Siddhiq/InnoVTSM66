/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ril_custom.c 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   The module has been designed for customer to develop new API functions over RIL.
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_stdlib.h"
#include "GPRS.h"

#if defined(__OCPU_RIL_SUPPORT__)

/*****************************************************************
* Function:     Ql_RIL_RcvDataFrmCore 
* 
* Description:
*               This function is used to receive data from the core 
*               system when programing some AT commands that need to
*               response with much data, such as "AT+QHTTPREAD". 
*
*               This function is implemented in ril_custom.c. Developer 
*               don't need to call this function. Under mode, this function
*               will be invoken when data coming automatically.
*
*               The CB_RIL_RcvDataFrmCore is defined for ustomer to define
*               the callback function for each AT command.
* Parameters:
*               [in]ptrData:
*                       Pointer to the data to be received.
*
*               [in]dataLen:
*                       The length to be received.
*
*               [in]reserved:
*                       Not used.
*
* Return:        
*               None.     
*
*****************************************************************/
CB_RIL_RcvDataFrmCore cb_rcvCoreData = NULL;
void Ql_RIL_RcvDataFrmCore(u8* ptrData, u32 dataLen, void* reserved)
{
    if (cb_rcvCoreData != NULL)
    {
        cb_rcvCoreData(ptrData, dataLen, reserved);
    }
}

#define MAX_NEIGHBOUR_CELLS 4

#define min_rssi -120
#define max_rssi -50
#define max_csq  31
#define min_csq  0

static int rssi_to_csq(int rssi) {
    if(rssi > 0)
        rssi = rssi*-1;
    // Ensure that rssi is within the specified range
    rssi = (rssi < min_rssi) ? min_rssi : (rssi > max_rssi) ? max_rssi : rssi;

    // Linear scaling formula
    int csq = (int)(((float)(rssi - min_rssi) / (max_rssi - min_rssi)) * (max_csq - min_csq) + min_csq + 0.5);

    return csq;
}

static s32 ATResponse_QENG_Handler(char* line, u32 len, void* userdata)
{
    GSM_Typedef *gsm = (GSM_Typedef *)userdata;
    char *head = Ql_RIL_FindString(line, len, "+QENG:");

    if (head)
    {
        int mode = -1;
        head += Ql_strlen("+QENG: ");
        Ql_sscanf(head, "%d", &mode);

        if (mode == 0)  // Serving cell
        {
            int mcc, mnc, lac, cellid;
            if (Ql_sscanf(head, "0,%d,%d,%x,%x", &mcc, &mnc, &lac, &cellid) >= 4)
            {
                gsm->MCC = mcc;
                gsm->MNC = mnc;
                Ql_sprintf(gsm->LAC, "%04X", lac);        // UPPERCASE HEX
                Ql_sprintf(gsm->CellID, "%04X", cellid);  // UPPERCASE HEX
            }
            LOGData("RIL_CUSTOM","Serving Cell: MCC=%d, MNC=%d, LAC=%s, CellID=%s",
                gsm->MCC, gsm->MNC, gsm->LAC, gsm->CellID);
        }
        else if (mode == 1)  // Neighbor cells
        {
            int ncell, bcch, dbm, bsic, c1, c2, mcc, mnc, lac, cellid;
            char *cursor = head + Ql_strlen("1,");
            int count = 0;

            while (count < MAX_NEIGHBOUR_CELLS &&
                   Ql_sscanf(cursor, "%d,%d,%d,%d,%d,%d,%d,%d,%x,%x",
                             &ncell, &bcch, &dbm, &bsic, &c1, &c2,
                             &mcc, &mnc, &lac, &cellid) == 10)
            {
                gsm->NeigbourCell[count].mcc = mcc;
                gsm->NeigbourCell[count].mnc = mnc;
                Ql_sprintf(gsm->NeigbourCell[count].LAC, "%04X", lac);      // UPPERCASE HEX
                Ql_sprintf(gsm->NeigbourCell[count].CellID, "%04X", cellid); // UPPERCASE HEX

                Ql_sprintf(gsm->NeigbourCell[count].CellDB, "%d", rssi_to_csq(dbm));     // Just numeric string

                count++;

                // Move cursor to next neighbor cell
                for (int skip = 0; skip < 10 && *cursor; ++skip)
                {
                    char *comma = Ql_strstr(cursor, ",");
                    if (!comma) break;
                    cursor = comma + 1;
                }
                LOGData("RIL_CUSTOM","Neighbor Cell %d: MCC=%d, MNC=%d, LAC=%s, CellID=%s, DB=%s",
                    count + 1, mcc, mnc,
                    gsm->NeigbourCell[count].LAC,
                    gsm->NeigbourCell[count].CellID,
                    gsm->NeigbourCell[count].CellDB);
            }

            gsm->IsNeighbourCells = (count > 0) ? 1 : 0;
        }

        return RIL_ATRSP_CONTINUE;
    }

    if (Ql_RIL_FindLine(line, len, "OK"))
        return RIL_ATRSP_SUCCESS;

    if (Ql_RIL_FindLine(line, len, "ERROR") || Ql_RIL_FindString(line, len, "+CME ERROR:"))
        return RIL_ATRSP_FAILED;

    return RIL_ATRSP_CONTINUE;
}



void EnableQENG(void)
{
    char *cmd = "AT+QENG=1,1";  // Enable non URC mode
    s32 ret = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), NULL, NULL,0);

    if (ret == RIL_AT_SUCCESS)
        LOGData("RIL_CUSTOM","QENG enabled.");
    else
         LOGData("RIL_CUSTOM","Failed to enable QENG. ret=%d", ret);
}

s32 RIL_GetQENGInfo(GSM_Typedef *gsm)
{
    char *cmd = "AT+QENG?";

    s32 ret = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QENG_Handler, gsm, 0);
    return ret;
}

// ... existing code ...

// QSTK response handlers
static s32 ATResponse_QSTK_Handler(char* line, u32 len, void* userdata)
{
    bool *enabled = (bool *)userdata;
    char *head = Ql_RIL_FindString(line, len, "+QSTK:");

    if (head)
    {
        int status = 0;
        head += Ql_strlen("+QSTK: ");
        Ql_sscanf(head, "%d", &status);
        *enabled = (status == 1);
        return RIL_ATRSP_CONTINUE;
    }

    if (Ql_RIL_FindLine(line, len, "OK"))
        return RIL_ATRSP_SUCCESS;

    if (Ql_RIL_FindLine(line, len, "ERROR"))
        return RIL_ATRSP_FAILED;

    return RIL_ATRSP_CONTINUE;
}

static s32 ATResponse_QSTKTR_Handler(char* line, u32 len, void* userdata)
{
    if (Ql_RIL_FindLine(line, len, "OK"))
        return RIL_ATRSP_SUCCESS;

    if (Ql_RIL_FindLine(line, len, "ERROR"))
        return RIL_ATRSP_FAILED;

    return RIL_ATRSP_CONTINUE;
}

static s32 ATResponse_QSTKENV_Handler(char* line, u32 len, void* userdata)
{
    if (Ql_RIL_FindLine(line, len, "OK"))
        return RIL_ATRSP_SUCCESS;

    if (Ql_RIL_FindLine(line, len, "ERROR"))
        return RIL_ATRSP_FAILED;

    return RIL_ATRSP_CONTINUE;
}
uint16_t STKTimeout=5000;
/**
 * Enable or disable STK functionality
 * 
 * @param enable    True to enable, false to disable
 * @return         RIL_AT_SUCCESS on success, error code on failure
 */
s32 RIL_QSTKSet(bool enable)
{
    char cmd[20];
    Ql_sprintf(cmd, "AT+QSTK=%d", enable ? 1 : 0);
    return Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QSTK_Handler, NULL, STKTimeout);
}
/**
 * Get current STK status
 * 
 * @param enabled   Pointer to store the STK status
 * @return         RIL_AT_SUCCESS on success, error code on failure
 */
s32 RIL_QSTKGet(bool *enabled)
{
    char *cmd = "AT+QSTK?";
    return Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QSTK_Handler, enabled, STKTimeout);
}

/**
 * Send terminal response to SIM
 * 
 * @param response  Terminal response string in hex format
 * @return         RIL_AT_SUCCESS on success, error code on failure
 */
s32 RIL_QSTKTerminalResponse(const char* response)
{
    char cmd[256];
    Ql_sprintf(cmd, "AT+STKTR=\"%s\"", response);
    return Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QSTKTR_Handler, NULL, STKTimeout);
}

/**
 * Send envelope command to SIM
 * 
 * @param command   Envelope command string in hex format
 * @return         RIL_AT_SUCCESS on success, error code on failure
 */
s32 RIL_QSTKEnvelopeCommand(const char* command)
{
    char cmd[256];
    Ql_sprintf(cmd, "AT+STKENV=\"%s\"", command);
    return Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QSTKENV_Handler, NULL, STKTimeout);
}

/**
 * Get STK main menu
 * 
 * @return         RIL_AT_SUCCESS on success, error code on failure
 */
s32 RIL_QSTKGetMainMenu(void)
{
    char *cmd = "AT+STKMENU";
    return Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), ATResponse_QSTK_Handler, NULL, STKTimeout);
}

//
// Customer may add new API functions definition here.
//
//
//
//
//
#endif

