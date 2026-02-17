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