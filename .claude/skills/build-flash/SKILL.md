---
name: build-flash
description: Build this ESP-IDF project and flash it to the ESP32-S3 board. Use when the user asks to build, flash, upload, or "run it on the board". Optional argument is the serial port (e.g. COM5).
---

Build and flash the firmware. Port argument: `$ARGUMENTS` (may be empty).

Run every step with the PowerShell tool, prefixing each command with the env activation in the same call:

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1 *> $null; <command>
```

1. If `sdkconfig` does not exist at the project root, run `idf.py set-target esp32s3`.
2. Run `idf.py build` (timeout 600000 — first builds are slow). If it fails, show the first compiler error with `file:line`, fix it if the cause is clear, and rebuild. Stop here if the user only asked to build.
3. Pick the port:
   - If `$ARGUMENTS` is non-empty, use it.
   - Otherwise list ports with `[System.IO.Ports.SerialPort]::GetPortNames()`. Exactly one → use it. None → ask the user to plug the board in (use the USB port labelled "USB" for the built-in USB-Serial/JTAG, or "UART"). Several → ask which one.
4. Run `idf.py -p <PORT> flash`. If it can't connect, tell the user to hold BOOT, tap RESET, release BOOT, then retry once.
5. Report the result. Do NOT run `idf.py monitor` (interactive, will hang) — tell the user to run `idf.py -p <PORT> monitor` themselves to see logs (Ctrl+] to exit).
