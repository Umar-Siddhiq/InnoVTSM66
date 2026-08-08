# Firmware and Embedded Documentation Additions

For firmware issues, capture these fields when relevant:

- MCU/SoC and board revision
- firmware version/build/hash
- compiler/toolchain and optimization level
- RTOS and task names/priorities
- modem/GNSS/module model and firmware version
- interfaces: UART/SPI/I2C/CAN/RS232/RS485/USB/Ethernet
- baud rate, timing, timeout, retry, buffer sizes
- power supply voltage/current conditions
- antenna/RF/network/SIM/operator state
- watchdog/reset reason
- stack/heap observations
- protocol variant and specification version
- packet before/after examples
- checksum/CRC validation
- bench instruments used
- field vs bench reproduction difference

For code changes, note ISR/task context and concurrency implications.

For communication bugs, distinguish:
- physical layer
- driver
- parser
- state machine
- protocol
- network/server

For power or RF problems, clearly separate firmware evidence from hardware evidence.
