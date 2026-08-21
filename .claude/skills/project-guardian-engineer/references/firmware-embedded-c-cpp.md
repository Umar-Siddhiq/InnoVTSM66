# Embedded C/C++ Rules

## Implementation priorities

Prefer predictable, bounded behavior. Reuse existing architecture and helpers before adding new abstractions. Avoid broad rewrites for localized defects.

## Memory safety checklist

Check every touched path for:
- array bounds and buffer sizes
- null/dangling pointers
- use-after-free and lifetime errors
- incorrect `memcpy`/`memset` lengths
- integer overflow and signed/unsigned conversion
- uninitialized values
- string termination
- unsafe `sprintf`/`strcpy`/`strcat` where bounded alternatives fit
- stack-heavy local arrays or recursion
- unchecked allocation failure and heap fragmentation

Prefer static/fixed-size allocation in long-running constrained firmware unless the existing architecture intentionally uses dynamic memory.

## Timing and concurrency

Check:
- blocking delays in timing-critical paths
- ISR-safe API usage
- shared variable synchronization
- `volatile` only where semantically appropriate; do not treat it as a lock
- atomicity of multi-byte/shared state updates
- race conditions between ISR/task/callback contexts
- timeout arithmetic and timer wraparound
- watchdog servicing and starvation

## Input validation

Treat modem, GNSS, network, CAN, UART, storage, and server data as untrusted. Validate length, delimiters, ranges, checksum/CRC, termination, and indexes before consuming.

## Change style

Implement the smallest correct change. Preserve naming, error handling, state-machine conventions, and compile-time configuration patterns already used by the project.
