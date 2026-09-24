# Project Map

Generated: `2026-09-24T03:22:00+00:00`
Repository: `InnoVTSM66`
Git: branch `OG-FIRMWARE`, HEAD `3ef4e64`

> Evidence-only inventory. Candidate entrypoints are filename-based; verify behavior in source before editing.

## Scale

- Files indexed: **730**
- Test-like files: **6**

## Languages

- C: 443 files
- C/C++ Header: 196 files
- Python: 5 files

## Build and dependency manifests

- None detected.

## Entrypoint candidates

- `QuectelM031-main/BootLoader/Source/main.c`
- `custom/main.c`

## CI and automation

- None detected.

## Current dirty paths

- `AMD3_PROTOCOL_IMPLEMENTATION.md`
- `QuectelM031-main.zip`
- `ai/MEMORY_INDEX.md`
- `custom/File.c`
- `custom/SMS.c`
- `custom/SOS.c`
- `custom/Server.c`
- `custom/Systic.c`
- `custom/inc/SMS.h`
- `custom/inc/Server.h`
- `custom/inc/VTS.h`
- `tests/og_delivery_regression.py`
- `tests/og_prf_signal_regression.py`
- `tests/og_pvt_format_regression.py`
- `tests/og_tamper_test_regression.py`
- `tests/og_wirecut_disable_regression.py`

## Top-level areas

- `QuectelM031-main`: 438 files
- `custom`: 84 files
- `reference`: 54 files
- `ril`: 41 files
- `example`: 40 files
- `.`: 30 files
- `include`: 24 files
- `make`: 7 files
- `tests`: 6 files
- `.vscode`: 3 files
- `libs`: 3 files

## High-churn files

- `custom/inc/VTS.h`: 10 commits in sampled history
- `custom/SMS.c`: 7 commits in sampled history
- `custom/Server.c`: 7 commits in sampled history
- `custom/File.c`: 6 commits in sampled history
- `custom/GPS.c`: 6 commits in sampled history
- `custom/config/custom_feature_def.h`: 6 commits in sampled history
- `custom/FTP.c`: 6 commits in sampled history
- `custom/SOS.c`: 5 commits in sampled history
- `custom/Systic.c`: 5 commits in sampled history
- `custom/inc/File.h`: 5 commits in sampled history
- `custom/inc/Server.h`: 5 commits in sampled history
- `custom/main.c`: 5 commits in sampled history
- `custom/GPRS.c`: 5 commits in sampled history
- `FIRMWARE_DOCUMENTATION.md`: 5 commits in sampled history
- `custom/MCU.c`: 4 commits in sampled history

## Largest source files

- `example/example_audio.c`: 910042 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/CommonTables/arm_common_tables.c`: 861193 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_dct4_init_f32.c`: 788301 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_dct4_init_q31.c`: 418732 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_rfft_init_f32.c`: 362552 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_dct4_init_q15.c`: 273945 bytes (C)
- `QuectelM031-main/Library/CMSIS/Include/arm_math.h`: 245185 bytes (C/C++ Header)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_rfft_init_q31.c`: 216301 bytes (C)
- `QuectelM031-main/Library/Device/Nuvoton/M031/Include/pwm_reg.h`: 189064 bytes (C/C++ Header)
- `custom/Server.c`: 183867 bytes (C)
- `reference/src/Server.c`: 162430 bytes (C)
- `QuectelM031-main/Library/CMSIS/DSP_Lib/Source/TransformFunctions/arm_rfft_init_q15.c`: 142634 bytes (C)
- `QuectelM031-main/Library/Device/Nuvoton/M031/Include/bpwm_reg.h`: 137640 bytes (C/C++ Header)
- `QuectelM031-main/Library/CMSIS/Include/core_cm7.h`: 137148 bytes (C/C++ Header)
- `QuectelM031-main/Library/Device/Nuvoton/M031/Include/sys_reg.h`: 123609 bytes (C/C++ Header)

## Retrieval rule

Use this map to choose a small set of relevant files. Search symbols and regression memory before reading broad directories. Do not infer architecture from filenames alone.
