# Build and Flash Helpers

## GMTWatchFace

Use this script to compile and flash `examples/LVGL/GMTWatchFace` quickly:

```powershell
pwsh -File scripts/build_flash_gmtwatchface.ps1 -Port COM4
```

Compile only:

```powershell
pwsh -File scripts/build_flash_gmtwatchface.ps1 -CompileOnly
```

Optional overrides:

- `-ArduinoCliPath "C:\Program Files\Arduino CLI\arduino-cli.exe"`
- `-Fqbn esp32:esp32:twatch`
- `-Esp32CoreVersion 2.0.17`
