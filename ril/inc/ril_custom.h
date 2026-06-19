#ifndef RIL_CUSTOM_H
#define RIL_CUSTOM_H
#include "ril.h"
#include "GPRS.h"

void EnableQENG(void);
s32 RIL_GetQENGInfo(GSM_Typedef *gsm);
// QSTK Functions
s32 RIL_QSTKSet(bool enable);
s32 RIL_QSTKGet(bool *enabled);
s32 RIL_QSTKTerminalResponse(const char* response);
s32 RIL_QSTKEnvelopeCommand(const char* command);
s32 RIL_QSTKGetMainMenu(void);

#endif // RIL_CUSTOM_H

