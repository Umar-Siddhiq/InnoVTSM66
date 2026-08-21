# Strict Protocol and Specification Compliance

Use this workflow for standards, customer protocols, AIS-140 variants, binary/ASCII packet formats, and amended specifications.

## Build the effective specification first

If a protocol has amendments/revisions, start from the baseline and apply each amendment in chronological order. Interpret add/substitute/replace/delete operations before auditing code. Do not treat the newest amendment as a standalone full specification unless it explicitly is one.

## Identify the exact implementation path

Find the selected protocol variant/configuration and trace only the packet builders, state handlers, parsers, and transport paths actually reachable from that variant. Do not infer compliance from similarly named unused code.

## Reconstruct actual output

Follow `sprintf`/`snprintf`, concatenation, buffer offsets, conditionals, binary writes, checksum functions, and terminators to reconstruct the actual wire packet. Count fields/bytes; do not trust comments like "field 55".

## Compare field by field

For each field/byte record expected value/format/order/source vs actual implementation. Verify datatype, decimal precision, allowed enumeration, optional/mandatory rules, start/end markers, checksum/CRC scope and algorithm, and runtime source.

Presence is not compliance. A field can exist but be in the wrong position, format, state, branch, or source.

## Runtime requirements

When a specification requires polling, timers, storage, retransmission, fallback, OTA retrieval, alerts, or periodic behavior, trace the runtime state machine and persistence path in addition to packet generation.

## Status labels

Use: PASS, FAIL, PARTIAL, NOT IMPLEMENTED, NOT REACHABLE, NEEDS RUNTIME VERIFICATION, NOT APPLICABLE.

A PASS requires specification evidence plus source-path evidence. If either is incomplete, use NEEDS VERIFICATION rather than PASS.

During an initial audit, do not modify code unless the user explicitly asked for fixes as part of the same task.
