# RTOS and Peripheral Driver Rules

## RTOS

For FreeRTOS, ThreadX, Zephyr, CMSIS-RTOS, or proprietary schedulers inspect:
- task priority and starvation
- stack size / high-water marks
- mutex/semaphore/queue ownership
- timeout values and tick wraparound
- deadlock and priority inversion
- event/timer callback execution context
- ISR-safe API variants
- critical-section duration

Never call blocking task APIs from ISR context.

## UART

Check baud/parity/stop bits/flow control, DMA/IRQ ownership, RX ring buffer, overflow policy, framing/error flags, parser reentrancy, TX completion, and race conditions between ISR and task consumers.

## SPI

Check mode (CPOL/CPHA), chip-select timing, frequency, frame width, bit order, DMA/cache interactions, and transaction boundaries.

## I2C

Check 7/10-bit address, ACK/NACK handling, bus-busy recovery, clock stretching, timeout behavior, and stuck-bus reset strategy.

## CAN

Check bitrate/sample point, standard vs extended ID, filters, DLC, endianness, error counters, bus-off handling, and retry/recovery policy.

## ADC/GPIO/PWM/timers

Check electrical polarity, pull configuration, debounce, sampling timing, scaling/reference voltage, timer overflow, compare/update timing, and ISR latency.
