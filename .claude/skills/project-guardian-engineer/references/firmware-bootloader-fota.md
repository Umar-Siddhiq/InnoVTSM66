# Bootloader and FOTA/OTA Rules

Treat bootloader/update changes as high risk.

Verify:
- image header/magic/version
- image size and flash bounds
- partition addresses and overlap
- CRC/hash/signature validation
- download integrity and resume logic
- boot selection flags
- rollback/fallback path
- interrupted update recovery
- watchdog/power-loss behavior during update
- bootloader/application compatibility

Never solve update failures by bypassing integrity checks unless the user explicitly requests a controlled diagnostic and understands the risk.

Before changing flash layout, map the existing memory regions and linker configuration. Require explicit approval before mass erase, fuse/option-byte changes, or bootloader overwrite.
