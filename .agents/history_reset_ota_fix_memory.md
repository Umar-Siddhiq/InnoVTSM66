# Project Memory: InnoVTSM66 Deep Analysis & Fix Plan

## Issues Analyzed
1. **History Packets Not Received On Server**:
   - `ProcessHistoryPacket()` in `custom/Server.c` validates saved packets using `if (!Ql_strstr(dataBuffer, "$PVT") || Ql_strlen(dataBuffer) < 150)`.
   - In `PROTO_OG`, saved history packets contain `$PER`, `$GPD`, `$CEL`, `$INF`, or `$NR` (length 70-140 bytes).
   - This check fails, causing the firmware to log `Invalid Hitory Packet, deleting...` and delete saved history without sending to server.
   - **Fix**: Update `ProcessHistoryPacket()` in `Server.c` to accept `$PER`, `$GPD`, `$CEL`, `$INF`, `$NR`, `$PVT`, `$EPB` prefixes for `PROTO_OG`.

2. **Reset Command `$868329088549211,549211,,SET,018:1*69` Not Resetting Device**:
   - In `custom/SMS.c` (`ParseStandardAIS140Command()`), `018:1` sets `reset_type = 2`.
   - For TCP socket sources (`SCK1`, `SCK2`), direct reset is skipped to allow sending `OA,12` reply first.
   - In `custom/Server.c`, deferred reset (`AIS140ResetPending`) is only executed if `responseSocket->SocketState == SOCKET_CONNECTED`.
   - If the socket drops or times out (`otaAckWait`), `AIS140ResetPending` is never executed, wedging the reset.
   - **Fix**: Add fallback execution for `AIS140ResetPending` on socket timeout or disconnect.

3. **Server OTA Not Received / Sockets Disconnecting**:
   - Log shows cyclic socket creation failures (`Socket 0 create failed`) due to unclosed socket descriptors after IP reconfiguration (`SETSERVER1`).
   - **Fix**: Add pre-creation socket cleanup in `TCP.c` and handle DNS/bearer reset cleanly.
