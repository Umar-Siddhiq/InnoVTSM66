# OG delivery acceptance

Run `python tests/og_delivery_regression.py` and `cmd /c Make.bat new_release`.
The host test extracts production functions and mocks transport/storage. It
does not verify physical flash, radio delivery, or SMS receipt.

On a test unit running this build, with history enabled and valid device time:

1. Send the firmware-version GET command that produced the reported packet.
   Expect `OA,12,L`, the complete source host:port, and the firmware value.
   Repeat on the secondary command endpoint. Logical SCK_2 maps to socket[2]
   and configured IP3; socket[1]/IP2 is the emergency endpoint.
2. Disconnect the primary PVT endpoint for several reporting intervals, then
   reconnect. Expect stored PVT packets longer than 256 bytes to replay as
   `NR,2,H` with a valid XOR checksum. Stored alert codes stay unchanged,
   while their live/history flag changes to H.
3. Keep primary PVT connected, disconnect only emergency IP2, and trigger SOS.
   Expect EPB storage and SMS fallback to configured Mob0. Reconnect emergency
   IP2; expect stored EPB replay with SP and a newly calculated valid checksum.
4. Repeat with both endpoints offline, and with IP2 set to NA (emergency routing
   then uses the primary endpoint). Restore connectivity and confirm replay.
5. Start SOS with emergency TCP connected, then disconnect it. Expect fallback
   at the next SOS reporting interval. Cause an SMS submission failure, restore
   SMS service, and confirm retry; after modem acceptance, no repeated fallback
   SMS is submitted during the same SOS activation. Modem acceptance does not
   prove handset delivery. Trigger another SOS and verify fallback is rearmed.
6. Make a command source unavailable while its reply is pending. Periodic PVT
   and history processing must continue; they must not consume the OTA response.
   Reconnection sends OA/12; after the bounded timeout the OA frame goes through
   primary delivery/history fallback. Verify endpoint SET/restart behavior too.

Record raw packets, storage counts, TCP results and modem SMS submission results.
Do not erase history to run these checks. Previously deleted packets cannot be
recovered by this firmware fix. The shared history store remains last-in-first-out;
a PVT at the top waits for the primary endpoint before older EPB records replay.
