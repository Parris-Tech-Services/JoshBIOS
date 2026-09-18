# Power button to Josh OS

This document keeps every layer honest about what it owns.

```text
Power button
    ↓
CPU/platform reset
    ↓
Platform firmware
    ↓
hardware init / memory init / device discovery
    ↓
Josh firmware experience (future coreboot payload)
    ↓
JoshBIOS Stage 1
    ↓
Josh Boot Manager (Stage 2)
    ├─ Boot
    ├─ Diagnostics
    └─ Reboot
    ↓
JoshBootInfo ABI
    ↓
test kernel today
    ↓
canonical Josh OS kernel in the future
    ↓
kernel services / userspace
    ↓
Josh compositor + shell
    ↓
login / session / desktop
```

## Screens in the journey

### Firmware splash / start screen

Future JoshFirmware should own the first branded screen after hardware initialisation. It should be fast and quiet: logo, firmware version, and a visible key hint for setup/recovery.

### Boot menu

The bootloader owns operating-system choice and recovery choice. The current Stage 2 now has a text boot manager with a short automatic timeout plus diagnostics and reboot options.

Future entries should include only real capabilities, for example:

- Josh OS
- Josh OS recovery
- another discovered OS
- firmware setup

Do not add dead menu items for features that do not exist.

### Kernel start screen

The kernel should show progress only for real boot phases and keep serial diagnostics available even when graphics fail.

### Login / desktop

Authentication, session selection and user-facing setup belong to the OS/userspace, not firmware or the bootloader.

## Long-term firmware direction

True power-on firmware is board-specific. JoshFirmware should use coreboot on explicitly supported boards, with recovery hardware available, rather than pretending one ROM image can safely initialise arbitrary PCs.
