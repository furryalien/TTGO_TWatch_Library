#include "config.h"
#include <math.h>
#include <Preferences.h>
#include <string.h>

TTGOClass *watch = nullptr;
PCF8563_Class *rtc = nullptr;
TFT_eSPI *tft = nullptr;
TFT_eSprite *canvas = nullptr;
Preferences prefs;

static const int16_t SCREEN_W = 240;
static const int16_t SCREEN_H = 240;
static const int16_t CX = SCREEN_W / 2;
static const int16_t CY = SCREEN_H / 2;

// True UTC by default. If enabled, GMT hand uses local time + LOCAL_TO_UTC_OFFSET_HOURS.
// If disabled, GMT hand uses local time + gmtOffsetHours (user-adjustable on watch).
static const bool DEFAULT_TRUE_UTC = true;
static const int8_t LOCAL_TO_UTC_OFFSET_HOURS = 0;

static bool gmtModeTrueUtc = DEFAULT_TRUE_UTC;
static int8_t gmtOffsetHours = 0;

static bool settingsOpen = false;
static bool touchDown = false;
static uint32_t touchDownStart = 0;
static int16_t lastTouchX = 0;
static int16_t lastTouchY = 0;

static const uint16_t LONG_PRESS_MS = 800;
static const float BATTERY_CAPACITY_MAH = 350.0f;
static const uint32_t DEFAULT_FULL_RUNTIME_SECONDS = 24UL * 60UL * 60UL;

static int clampPercent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static uint32_t estimateBatterySecondsRemaining(int pct, bool charging, float chargeCurrentMa, float dischargeCurrentMa)
{
    float ratio = (float)clampPercent(pct) / 100.0f;
    float seconds = ratio * (float)DEFAULT_FULL_RUNTIME_SECONDS;

    if (charging) {
        if (chargeCurrentMa > 1.0f) {
            seconds = ((100.0f - (float)pct) / 100.0f) * BATTERY_CAPACITY_MAH / chargeCurrentMa * 3600.0f;
        } else {
            seconds = (1.0f - ratio) * (float)DEFAULT_FULL_RUNTIME_SECONDS;
        }
    } else {
        if (dischargeCurrentMa > 1.0f) {
            seconds = ratio * BATTERY_CAPACITY_MAH / dischargeCurrentMa * 3600.0f;
        }
    }

    if (seconds < 0.0f) {
        seconds = 0.0f;
    }
    if (seconds > 359999.0f) {
        seconds = 359999.0f;
    }
    return (uint32_t)seconds;
}

static void formatHms(uint32_t totalSeconds, char *out, size_t outLen)
{
    uint32_t hours = totalSeconds / 3600UL;
    uint32_t minutes = (totalSeconds % 3600UL) / 60UL;
    uint32_t seconds = totalSeconds % 60UL;
    snprintf(out, outLen, "%02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
}

static float normalizeAngle(float angle)
{
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

static int16_t pointX(float angleDeg, int16_t radius)
{
    float rad = (angleDeg - 90.0f) * DEG_TO_RAD;
    return (int16_t)(CX + cosf(rad) * radius);
}

static int16_t pointY(float angleDeg, int16_t radius)
{
    float rad = (angleDeg - 90.0f) * DEG_TO_RAD;
    return (int16_t)(CY + sinf(rad) * radius);
}

static void drawHand(float angleDeg, int16_t length, int16_t tail, uint8_t width, uint16_t color)
{
    float rad = (angleDeg - 90.0f) * DEG_TO_RAD;
    float perpendicular = rad + (PI / 2.0f);

    int16_t x1 = (int16_t)(CX - cosf(rad) * tail);
    int16_t y1 = (int16_t)(CY - sinf(rad) * tail);
    int16_t x2 = (int16_t)(CX + cosf(rad) * length);
    int16_t y2 = (int16_t)(CY + sinf(rad) * length);

    int16_t half = width / 2;
    for (int16_t i = -half; i <= half; ++i) {
        int16_t ox = (int16_t)(cosf(perpendicular) * i);
        int16_t oy = (int16_t)(sinf(perpendicular) * i);
        canvas->drawLine(x1 + ox, y1 + oy, x2 + ox, y2 + oy, color);
    }
}

static void drawGmtArrow(float angleDeg)
{
    const int16_t arrowTipRadius = 91;
    const int16_t arrowBaseRadius = 77;
    const int16_t wingHalfWidth = 5;
    const uint16_t arrowColor = TFT_RED;

    drawHand(angleDeg, arrowBaseRadius, 14, 2, arrowColor);

    float rad = (angleDeg - 90.0f) * DEG_TO_RAD;
    float perpendicular = rad + (PI / 2.0f);

    int16_t tipX = (int16_t)(CX + cosf(rad) * arrowTipRadius);
    int16_t tipY = (int16_t)(CY + sinf(rad) * arrowTipRadius);

    int16_t baseX = (int16_t)(CX + cosf(rad) * arrowBaseRadius);
    int16_t baseY = (int16_t)(CY + sinf(rad) * arrowBaseRadius);

    int16_t wing1X = (int16_t)(baseX + cosf(perpendicular) * wingHalfWidth);
    int16_t wing1Y = (int16_t)(baseY + sinf(perpendicular) * wingHalfWidth);
    int16_t wing2X = (int16_t)(baseX - cosf(perpendicular) * wingHalfWidth);
    int16_t wing2Y = (int16_t)(baseY - sinf(perpendicular) * wingHalfWidth);

    canvas->fillTriangle(tipX, tipY, wing1X, wing1Y, wing2X, wing2Y, arrowColor);
}

static int8_t clampOffset(int8_t value)
{
    if (value < -12) {
        return -12;
    }
    if (value > 14) {
        return 14;
    }
    return value;
}

static void saveSettings()
{
    prefs.putBool("trueUtc", gmtModeTrueUtc);
    prefs.putChar("gmtOff", gmtOffsetHours);
}

static void loadSettings()
{
    prefs.begin("gmtface", false);
    gmtModeTrueUtc = prefs.getBool("trueUtc", DEFAULT_TRUE_UTC);
    gmtOffsetHours = clampOffset((int8_t)prefs.getChar("gmtOff", 0));
}

static void syncRtcToBuildTimeOnce()
{
    if (!rtc) {
        return;
    }

    const char *buildStamp = __DATE__ " " __TIME__;
    String syncedBuild = prefs.getString("rtcBuild", "");
    if (syncedBuild != String(buildStamp)) {
        rtc->setDateTime(RTC_Date(__DATE__, __TIME__));
        prefs.putString("rtcBuild", buildStamp);
    }
}

static bool parseAndSetRtcFromSerial(const char *cmd)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool matched =
        (sscanf(cmd, "SET %d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) ||
        (sscanf(cmd, "SET %d %d %d %d %d %d", &year, &month, &day, &hour, &minute, &second) == 6);

    if (!matched) {
        return false;
    }

    bool valid =
        year >= 2000 && year <= 2099 &&
        month >= 1 && month <= 12 &&
        day >= 1 && day <= 31 &&
        hour >= 0 && hour <= 23 &&
        minute >= 0 && minute <= 59 &&
        second >= 0 && second <= 59;

    if (!valid) {
        Serial.println("ERR Invalid date/time range. Use YYYY-MM-DD HH:MM:SS");
        return true;
    }

    rtc->setDateTime((uint16_t)year, (uint8_t)month, (uint8_t)day, (uint8_t)hour, (uint8_t)minute, (uint8_t)second);
    RTC_Date now = rtc->getDateTime();
    Serial.printf("OK RTC set to %04u-%02u-%02u %02u:%02u:%02u\n", now.year, now.month, now.day, now.hour, now.minute, now.second);
    return true;
}

static void handleSerialCommands()
{
    static char buffer[80];
    static uint8_t idx = 0;

    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            buffer[idx] = '\0';
            idx = 0;

            if (buffer[0] == '\0') {
                continue;
            }

            if (strcmp(buffer, "HELP") == 0) {
                Serial.println("Commands:");
                Serial.println("  SET YYYY-MM-DD HH:MM:SS");
                Serial.println("  SET YYYY MM DD HH MM SS");
                Serial.println("  GET");
                continue;
            }

            if (strcmp(buffer, "GET") == 0) {
                RTC_Date now = rtc->getDateTime();
                Serial.printf("RTC %04u-%02u-%02u %02u:%02u:%02u\n", now.year, now.month, now.day, now.hour, now.minute, now.second);
                continue;
            }

            if (!parseAndSetRtcFromSerial(buffer)) {
                Serial.println("ERR Unknown command. Type HELP");
            }
            continue;
        }

        if (idx < sizeof(buffer) - 1) {
            buffer[idx++] = c;
        }
    }
}

static void drawStaticDial()
{
    canvas->fillSprite(TFT_BLACK);

    // Outer case-like ring and inner bezel
    canvas->drawCircle(CX, CY, 118, TFT_DARKGREY);
    canvas->drawCircle(CX, CY, 117, TFT_DARKGREY);
    canvas->drawCircle(CX, CY, 112, TFT_DARKGREY);
    canvas->drawCircle(CX, CY, 111, TFT_DARKGREY);
    canvas->drawCircle(CX, CY, 106, TFT_DARKGREY);
    canvas->drawCircle(CX, CY, 105, TFT_DARKGREY);

    // 24-hour ring (every hour marker)
    for (int hour = 0; hour < 24; ++hour) {
        float angle = hour * 15.0f;
        int16_t outerR = (hour % 2 == 0) ? 104 : 102;
        int16_t innerR = (hour % 2 == 0) ? 99 : 100;
        canvas->drawLine(pointX(angle, innerR), pointY(angle, innerR), pointX(angle, outerR), pointY(angle, outerR), TFT_LIGHTGREY);
    }

    // 24-hour numerals (2-hour steps)
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setTextFont(1);
    char h24buf[3];
    for (int hour = 2; hour <= 24; hour += 2) {
        float angle = (hour % 24) * 15.0f;
        snprintf(h24buf, sizeof(h24buf), "%02d", hour % 24);
        canvas->drawCentreString(h24buf, pointX(angle, 93), pointY(angle, 93), 1);
    }

    // Main hour/minute markers
    for (int idx = 0; idx < 60; ++idx) {
        float angle = idx * 6.0f;
        bool major = (idx % 5 == 0);
        int16_t outerR = 84;
        int16_t innerR = major ? 74 : 79;
        uint16_t color = major ? TFT_WHITE : TFT_DARKGREY;
        canvas->drawLine(pointX(angle, innerR), pointY(angle, innerR), pointX(angle, outerR), pointY(angle, outerR), color);
    }

    // Cardinal thick markers
    for (int i = 0; i < 4; ++i) {
        float angle = i * 90.0f;
        drawHand(angle, 83, -74, 3, TFT_WHITE);
    }

    // 12, 9, 6, 3 numerals
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextFont(4);
    canvas->drawCentreString("12", CX, CY - 58, 4);
    canvas->drawCentreString("9", CX - 66, CY - 9, 4);
    canvas->drawCentreString("6", CX, CY + 58, 4);
    canvas->drawCentreString("3", CX + 66, CY - 9, 4);
}

static void drawDateWindow(uint8_t day)
{
    const int16_t winCX = 169;
    const int16_t winCY = 161;

    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextFont(2);
    char dayBuf[3];
    snprintf(dayBuf, sizeof(dayBuf), "%02u", day);
    canvas->drawCentreString(dayBuf, winCX, winCY, 2);
}

static void drawWeekdayLabel(const RTC_Date &dt)
{
    static const char *days2[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    uint32_t weekday = rtc->getDayOfWeek(dt.day, dt.month, dt.year) % 7;

    const int16_t wx = pointX(225.0f, 62);
    const int16_t wy = pointY(225.0f, 62);

    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextFont(2);
    canvas->drawCentreString(days2[weekday], wx, wy, 2);
}

static void drawPowerIndicator()
{
    const int16_t bx = 205;
    const int16_t by = 10;
    const int16_t bw = 14;
    const int16_t bh = 6;

    int pct = 0;
    bool charging = false;
    float chargeCurrentMa = 0.0f;
    float dischargeCurrentMa = 0.0f;
#ifdef LILYGO_WATCH_HAS_AXP202
    if (watch && watch->power) {
        pct = clampPercent(watch->power->getBattPercentage());
        charging = watch->power->isChargeing();
        chargeCurrentMa = watch->power->getBattChargeCurrent();
        dischargeCurrentMa = watch->power->getBattDischargeCurrent();
    }
#endif

    canvas->drawRect(bx, by, bw, bh, TFT_WHITE);
    canvas->fillRect(bx + bw, by + 2, 2, 3, TFT_WHITE);

    int fillW = (int)((bw - 4) * pct / 100.0f);
    uint16_t fillCol = pct > 20 ? TFT_WHITE : TFT_RED;
    if (fillW > 0) {
        canvas->fillRect(bx + 2, by + 2, fillW, bh - 4, fillCol);
    }

    if (charging) {
        canvas->drawLine(bx + 7, by + 1, bx + 5, by + 3, TFT_YELLOW);
        canvas->drawLine(bx + 5, by + 3, bx + 8, by + 3, TFT_YELLOW);
        canvas->drawLine(bx + 8, by + 3, bx + 6, by + 5, TFT_YELLOW);
    }

    uint32_t remainSeconds = estimateBatterySecondsRemaining(pct, charging, chargeCurrentMa, dischargeCurrentMa);
    char etaBuf[10];
    formatHms(remainSeconds, etaBuf, sizeof(etaBuf));
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(TR_DATUM);
    canvas->setTextFont(1);
    canvas->drawString(etaBuf, SCREEN_W - 1, by + bh + 4, 1);
}

static void drawSettingsOverlay()
{
    if (!settingsOpen) {
        return;
    }

    const int16_t x = 20;
    const int16_t y = 56;
    const int16_t w = 200;
    const int16_t h = 128;

    canvas->fillRoundRect(x, y, w, h, 8, TFT_NAVY);
    canvas->drawRoundRect(x, y, w, h, 8, TFT_WHITE);

    canvas->setTextDatum(TL_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->setTextFont(2);
    canvas->drawString("GMT Settings", x + 10, y + 10, 2);

    canvas->setTextColor(TFT_YELLOW, TFT_NAVY);
    canvas->drawString(gmtModeTrueUtc ? "Mode: TRUE UTC" : "Mode: OFFSET", x + 10, y + 36, 2);

    char offBuf[20];
    snprintf(offBuf, sizeof(offBuf), "Offset: %+d", gmtOffsetHours);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString(offBuf, x + 10, y + 60, 2);

    canvas->setTextColor(TFT_CYAN, TFT_NAVY);
    canvas->drawString("Left:-  Right:+", x + 10, y + 86, 2);
    canvas->drawString("Center:Mode  Top:Exit", x + 10, y + 106, 2);
}

static void drawFace(const RTC_Date &dt)
{
    drawStaticDial();
    drawDateWindow(dt.day);
    drawWeekdayLabel(dt);
    drawPowerIndicator();

    float secondAngle = dt.second * 6.0f;
    float minuteAngle = (dt.minute + dt.second / 60.0f) * 6.0f;
    float hourAngle = ((dt.hour % 12) + dt.minute / 60.0f + dt.second / 3600.0f) * 30.0f;

    int8_t appliedOffset = gmtModeTrueUtc ? LOCAL_TO_UTC_OFFSET_HOURS : gmtOffsetHours;
    int16_t gmtHour = dt.hour + appliedOffset;
    if (gmtHour < 0) {
        gmtHour += 24;
    }
    gmtHour %= 24;
    float gmtAngle = normalizeAngle((gmtHour + dt.minute / 60.0f + dt.second / 3600.0f) * 15.0f);

    // Hands draw order: GMT, hour, minute, second
    drawGmtArrow(gmtAngle);
    drawHand(hourAngle, 51, 11, 4, TFT_WHITE);
    drawHand(minuteAngle, 73, 13, 2, TFT_WHITE);
    drawHand(secondAngle, 87, 16, 1, TFT_LIGHTGREY);

    // Pinion and decorative center stack
    canvas->fillCircle(CX, CY, 5, TFT_WHITE);
    canvas->fillCircle(CX, CY, 3, TFT_BLACK);
    canvas->drawCircle(CX, CY, 6, TFT_RED);

    drawSettingsOverlay();

    canvas->pushSprite(0, 0);
}

static bool handleSettingsTap(int16_t x, int16_t y)
{
    if (!settingsOpen) {
        return false;
    }

    if (y < 48) {
        settingsOpen = false;
        saveSettings();
        return true;
    }

    if (x < 80) {
        gmtOffsetHours = clampOffset(gmtOffsetHours - 1);
        saveSettings();
        return true;
    }

    if (x > 160) {
        gmtOffsetHours = clampOffset(gmtOffsetHours + 1);
        saveSettings();
        return true;
    }

    gmtModeTrueUtc = !gmtModeTrueUtc;
    saveSettings();
    return true;
}

static void pollTouchUI()
{
    int16_t x = 0;
    int16_t y = 0;
    bool touched = watch->getTouch(x, y);

    if (touched && !touchDown) {
        touchDown = true;
        touchDownStart = millis();
    }

    if (touched) {
        lastTouchX = x;
        lastTouchY = y;
    }

    if (touchDown && touched && !settingsOpen) {
        if (millis() - touchDownStart >= LONG_PRESS_MS) {
            settingsOpen = true;
        }
    }

    if (!touched && touchDown) {
        uint32_t heldMs = millis() - touchDownStart;
        if (settingsOpen && heldMs < LONG_PRESS_MS) {
            handleSettingsTap(lastTouchX, lastTouchY);
        }
        touchDown = false;
    }
}

void setup()
{
    Serial.begin(115200);

    watch = TTGOClass::getWatch();
    watch->begin();
    watch->openBL();

    // Keep LVGL clock active because this is an LVGL-configured example folder.
    watch->lvgl_begin();

    // Lower backlight a little to better match dark-dial style.
    watch->bl->adjust(160);

#ifdef LILYGO_WATCH_HAS_AXP202
    watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
#endif

    rtc = watch->rtc;
    tft = watch->tft;

    rtc->check();
    loadSettings();
    syncRtcToBuildTimeOnce();
    Serial.println("Serial RTC commands ready. Type HELP");

    canvas = new TFT_eSprite(tft);
    canvas->setColorDepth(16);
    canvas->createSprite(SCREEN_W, SCREEN_H);
    canvas->fillSprite(TFT_BLACK);

    drawFace(rtc->getDateTime());
}

void loop()
{
    static uint8_t lastSecond = 255;
    static uint32_t lastUiFrame = 0;

    handleSerialCommands();
    pollTouchUI();

    RTC_Date now = rtc->getDateTime();

    bool frameDue = (millis() - lastUiFrame) >= 100;
    if (settingsOpen && frameDue) {
        lastUiFrame = millis();
        drawFace(now);
    }

    if (now.second != lastSecond) {
        lastSecond = now.second;
        drawFace(now);
    }

    delay(20);
}
