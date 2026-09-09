# Critical Project Invariants

Keep only high-signal, proven rules here. User-written content outside the managed block is preserved.

<!-- PROJECT-GUARDIAN:CRITICAL:START -->
- **CRITICAL / CORRECTION** `2026-09-03` - **Reverted Nuvoton M032 12-bit ADC scaling and restored M031 10-bit ADC scaling**: Reverted NUVOTON_M032 macro and 12-bit ADC (/4095.0f) scaling in custom_feature_def.h, MCU.h, and MCU.c. The 12-bit divisor caused 10-bit M031 ADC values (~500 at 12V mains) to be scaled down to 3.68V. Because 3.68V is below the 6.0V threshold in Hardware.c (PeriPheralVal.MainsVolt > 6.0), PeriPheralVal.IsMain was set to 0 (power disconnected) while ignition remained 1, causing  packet to show '...,1,0,3.6,3.5,...' and GETVSTATUS SMS to show 'MV: 3.68'. Restored standard 10-bit scaling (/1023.0f). Guard: Do not define NUVOTON_M032 or use 12-bit ADC scaling for M031 boards; always verify ADC bit depth and MainsVolt threshold (>6.0V) when touching voltage conversion.
<!-- PROJECT-GUARDIAN:CRITICAL:END -->
