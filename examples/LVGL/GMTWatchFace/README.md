# GMTWatchFace

Analog GMT watch face for TTGO T-Watch with:

- Hour, minute, second hands
- Red GMT hand with arrow tip
- Numeric date window at 430
- Day of Week at 730
- 24-hour chapter ring
- Large 12 / 9 / 6 / 3 markers

## Quick config

1. Open `config.h` and uncomment your watch hardware macro.
2. In `GMTWatchFace.ino`, adjust:
   - `LOCAL_TO_UTC_OFFSET_HOURS`: local-to-UTC offset used when True UTC mode is enabled

## On-watch settings

- Long-press anywhere (~0.8s) to open settings overlay.
- Tap left side to decrease GMT offset.
- Tap right side to increase GMT offset.
- Tap center to toggle mode (`TRUE UTC` / `OFFSET`).
- Tap top area to exit settings.

## Notes

- Date is shown from RTC local date (`dt.day`).
- GMT hand defaults to `TRUE UTC` mode.
- Seconds hand is ticking (1Hz).

## Power Optimizations

This firmware implements comprehensive power management for extended battery life:

### Implemented Optimizations

1. **CPU Frequency**: 20MHz base (proven stable by BatmanDial), scales to 160MHz for WiFi operations
2. **ADC Management**: Only essential battery monitoring enabled; all unused ADCs disabled (~100-200µA savings)
3. **ADC Sampling Rate**: Reduced from 200Hz to 25Hz (~50-100µA savings)
4. **Wireless Radios**: WiFi and Bluetooth completely disabled except during charging sync (~20-40mA savings)
5. **Temperature Sensor**: Disabled when not needed (~10µA savings)
6. **Charging Optimization**: 300mA charging current for reduced heat and improved efficiency
7. **Audio Rail**: LDO3 power rail disabled (not used by watchface)
8. **LVGL Refresh**: Set to 30ms (33Hz) in lv_conf.h for power/responsiveness balance
9. **Flash Power Down**: Enabled during light sleep for additional savings
10. **Touch Controller**: Automatically enters monitor mode during sleep (~24µA vs ~1-2mA active)
11. **Motion Sensor**: Accelerometer disabled during sleep (~165µA savings)
12. **Dynamic Frequency Scaling**: CPU adapts to workload (WiFi/UI/sleep)
13. **Context-Aware Brightness**: Adjusts for time of day and battery level

### Expected Power Consumption

- **Active (screen on)**: ~35-45mA (vs baseline ~65mA) - **30% reduction**
- **Light sleep**: ~2-3mA (vs baseline ~4mA) - **40% reduction**
- **Overall battery life improvement**: **40-60% in typical usage**

### Power Management Features

- Auto-dim after 15 seconds of inactivity
- Sleep mode after 30 seconds (screen off, ~2-3mA)
- WiFi sync only when charging (preserves battery during normal use)
- Hourly RTC sync for time accuracy
- Smart sensor management based on watch state
- Context-aware brightness (night mode, low battery reduction)