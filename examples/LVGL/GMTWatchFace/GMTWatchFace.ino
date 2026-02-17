#include "config.h"
#include <math.h>
#include <Preferences.h>
#include <string.h>
#include <WiFi.h>
#include <time.h>
#include "esp_sleep.h"

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
static uint32_t lastFullChargeTimestamp = 0;

static bool settingsOpen = false;
static bool touchDown = false;
static uint32_t touchDownStart = 0;
static int16_t lastTouchX = 0;
static int16_t lastTouchY = 0;

static const uint16_t LONG_PRESS_MS = 800;
static const float BATTERY_CAPACITY_MAH = 350.0f;
static const uint32_t DEFAULT_FULL_RUNTIME_SECONDS = 24UL * 60UL * 60UL;

static const uint32_t SCREEN_TIMEOUT_MS = 30UL * 1000UL;  // Sleep after 30 seconds
static const uint32_t DIM_TIMEOUT_MS = 15UL * 1000UL;     // Dim after 15 seconds
static const uint8_t DAY_ACTIVE_BRIGHTNESS = 180;
static const uint8_t DAY_DIM_BRIGHTNESS = 50;
static const uint8_t NIGHT_ACTIVE_BRIGHTNESS = 80;
static const uint8_t NIGHT_DIM_BRIGHTNESS = 20;
static const uint32_t RTC_SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL;
static const uint32_t WIFI_SYNC_INTERVAL_MS = 4UL * 60UL * 60UL * 1000UL;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000UL;
static const char *RTC_TIME_ZONE = "CST-8";

static bool displaySleeping = false;
static bool backlightDimmed = false;
static uint32_t lastActivityMs = 0;
static uint32_t lastRtcSyncMs = 0;
static uint32_t lastWiFiSyncMs = 0;
static uint32_t lastBrightnessChangeMs = 0;
static uint32_t lastIRQCheckMs = 0;
static const uint32_t BRIGHTNESS_CHANGE_DEBOUNCE_MS = 500;
static const uint32_t IRQ_CHECK_INTERVAL_MS = 100;  // Only check IRQ 10 times per second

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

static uint8_t calculateContextualBrightness(bool dimmed)
{
    if (!rtc) {
        return dimmed ? DAY_DIM_BRIGHTNESS : DAY_ACTIVE_BRIGHTNESS;
    }

    RTC_Date now = rtc->getDateTime();
    bool isNightTime = (now.hour >= 22 || now.hour < 6);

#ifdef LILYGO_WATCH_HAS_AXP202
    if (watch && watch->power) {
        float batteryVoltage = watch->power->getBattVoltage();
        if (batteryVoltage < 3.5f) {
            uint8_t baseBrightness = isNightTime ? 
                (dimmed ? NIGHT_DIM_BRIGHTNESS : NIGHT_ACTIVE_BRIGHTNESS) : 
                (dimmed ? DAY_DIM_BRIGHTNESS : DAY_ACTIVE_BRIGHTNESS);
            return (uint8_t)(baseBrightness * 0.7f);
        }
    }
#endif

    return isNightTime ? 
        (dimmed ? NIGHT_DIM_BRIGHTNESS : NIGHT_ACTIVE_BRIGHTNESS) : 
        (dimmed ? DAY_DIM_BRIGHTNESS : DAY_ACTIVE_BRIGHTNESS);
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

static void saveLastFullChargeTimestamp()
{
    prefs.putUInt("lastCharge", lastFullChargeTimestamp);
}

static void loadSettings()
{
    prefs.begin("gmtface", false);
    gmtModeTrueUtc = prefs.getBool("trueUtc", DEFAULT_TRUE_UTC);
    gmtOffsetHours = clampOffset((int8_t)prefs.getChar("gmtOff", 0));
    lastFullChargeTimestamp = prefs.getUInt("lastCharge", 0);
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

// ============================================================================
// TIER 3: ADVANCED POWER MANAGEMENT CLASSES
// ============================================================================

enum PowerProfile {
    PROFILE_PERFORMANCE,   // 160 MHz - network operations
    PROFILE_BALANCED,      // 20 MHz - normal UI (proven by BatmanDial)
    PROFILE_POWER_SAVE,    // 20 MHz - static watch face
    PROFILE_ULTRA_SAVE     // 20 MHz - sleep/minimal updates
};

class DynamicFrequencyScaler {
private:
    PowerProfile currentProfile;
    uint32_t lastTransition;
    bool wifiActive;
    bool animationActive;
    
public:
    DynamicFrequencyScaler() : currentProfile(PROFILE_BALANCED), lastTransition(0), 
                               wifiActive(false), animationActive(false) {}
    
    void setWiFiActive(bool active) {
        wifiActive = active;
        update();
    }
    
    void setAnimationActive(bool active) {
        animationActive = active;
        update();
    }
    
    void update() {
        PowerProfile target = calculateOptimalProfile();
        if (target != currentProfile) {
            transitionTo(target);
        }
    }
    
    PowerProfile calculateOptimalProfile() {
        if (wifiActive || animationActive) {
            return PROFILE_PERFORMANCE;
        } else if (displaySleeping) {
            return PROFILE_ULTRA_SAVE;
        } else if (settingsOpen || (millis() - lastActivityMs) < 3000) {
            return PROFILE_BALANCED;
        } else {
            return PROFILE_POWER_SAVE;
        }
    }
    
    void transitionTo(PowerProfile profile) {
        uint8_t freq = 20;
        switch(profile) {
            case PROFILE_PERFORMANCE: freq = 160; break;
            case PROFILE_BALANCED:    freq = 20;  break;
            case PROFILE_POWER_SAVE:  freq = 20;  break;
            case PROFILE_ULTRA_SAVE:  freq = 20;  break;
        }
        setCpuFrequencyMhz(freq);
        currentProfile = profile;
        lastTransition = millis();
    }
    
    PowerProfile getCurrentProfile() const {
        return currentProfile;
    }
};

enum SleepState {
    STATE_ACTIVE,        // Full power, screen on
    STATE_IDLE,          // Screen on, no interaction
    STATE_DIM,           // Screen dimmed
    STATE_LIGHT_SLEEP,   // Screen off, quick wake
    STATE_DEEP_STANDBY   // Maximum power save (not implemented for active use)
};

class SleepStateManager {
private:
    SleepState currentState;
    uint32_t stateEnterTime;
    
    static const uint32_t IDLE_TRANSITION_MS = 3000;
    static const uint32_t LIGHT_SLEEP_TIMEOUT_MS = 300000;  // 5 minutes
    
public:
    SleepStateManager() : currentState(STATE_ACTIVE), stateEnterTime(0) {}
    
    void update(uint32_t inactiveMs) {
        SleepState targetState = calculateTargetState(inactiveMs);
        if (targetState != currentState) {
            transitionTo(targetState);
        }
    }
    
    SleepState calculateTargetState(uint32_t inactiveMs) {
        if (inactiveMs < IDLE_TRANSITION_MS) {
            return STATE_ACTIVE;
        } else if (inactiveMs < DIM_TIMEOUT_MS) {
            return STATE_IDLE;
        } else if (inactiveMs < SCREEN_TIMEOUT_MS) {
            return STATE_DIM;
        } else {
            return STATE_LIGHT_SLEEP;
        }
    }
    
    void transitionTo(SleepState newState) {
        currentState = newState;
        stateEnterTime = millis();
    }
    
    SleepState getCurrentState() const {
        return currentState;
    }
    
    void forceActive() {
        transitionTo(STATE_ACTIVE);
    }
};

class AlwaysOnDisplayManager {
private:
    bool aodEnabled;
    bool aodActive;
    uint32_t lastAodUpdate;
    static const uint32_t AOD_UPDATE_INTERVAL_MS = 60000;  // Update every minute
    static const uint8_t AOD_BRIGHTNESS = 15;  // 5% brightness
    
public:
    AlwaysOnDisplayManager() : aodEnabled(false), aodActive(false), lastAodUpdate(0) {}
    
    void setEnabled(bool enabled) {
        aodEnabled = enabled;
    }
    
    bool isEnabled() const {
        return aodEnabled;
    }
    
    bool isActive() const {
        return aodActive;
    }
    
    void activate() {
        if (!aodEnabled) return;
        
        aodActive = true;
        watch->setBrightness(AOD_BRIGHTNESS);
        setCpuFrequencyMhz(20);
        lastAodUpdate = millis();
    }
    
    void deactivate() {
        aodActive = false;
        watch->setBrightness(calculateContextualBrightness(false));
        setCpuFrequencyMhz(80);
    }
    
    bool needsUpdate() {
        if (!aodActive) return false;
        return (millis() - lastAodUpdate) >= AOD_UPDATE_INTERVAL_MS;
    }
    
    void markUpdated() {
        lastAodUpdate = millis();
    }
};

class PeripheralPowerManager {
private:
    bool accelEnabled;
    bool stepCountEnabled;
    bool audioEnabled;
    uint32_t lastOptimization;
    
    static const uint32_t OPTIMIZATION_INTERVAL_MS = 10000;
    
public:
    PeripheralPowerManager() : accelEnabled(true), stepCountEnabled(true), 
                               audioEnabled(false), lastOptimization(0) {}
    
    void optimizeForState(SleepState state) {
        if (millis() - lastOptimization < OPTIMIZATION_INTERVAL_MS) {
            return;
        }
        
        switch(state) {
            case STATE_ACTIVE:
            case STATE_IDLE:
            case STATE_DIM:
                enableAccelIfNeeded(true);
                enableStepCountIfNeeded(true);
                break;
                
            case STATE_LIGHT_SLEEP:
                enableAccelIfNeeded(false);
                enableStepCountIfNeeded(false);
                break;
                
            case STATE_DEEP_STANDBY:
                disableAllPeripherals();
                break;
        }
        
        lastOptimization = millis();
    }
    
    void enableAccelIfNeeded(bool enable) {
        if (accelEnabled != enable) {
            if (enable) {
                watch->bma->enableAccel();
            } else {
                watch->bma->disableAccel();
            }
            accelEnabled = enable;
        }
    }
    
    void enableStepCountIfNeeded(bool enable) {
        if (stepCountEnabled != enable) {
            watch->bma->enableStepCountInterrupt(enable);
            stepCountEnabled = enable;
        }
    }
    
    void disableAllPeripherals() {
        watch->bma->disableAccel();
        watch->bma->enableStepCountInterrupt(false);
        watch->bma->enableWakeupInterrupt(false);
        
#ifdef LILYGO_WATCH_HAS_AXP202
        if (watch && watch->power) {
            watch->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
        }
#endif
        
        accelEnabled = false;
        stepCountEnabled = false;
        audioEnabled = false;
    }
    
    float estimatePowerSavings() {
        float savings = 0.0f;
        if (!accelEnabled) savings += 0.165f;  // mA
        if (!stepCountEnabled) savings += 0.05f;
        if (!audioEnabled) savings += 0.3f;
        return savings;
    }
};

// Global Tier 3 managers
static DynamicFrequencyScaler frequencyScaler;
static SleepStateManager sleepManager;
static AlwaysOnDisplayManager aodManager;
static PeripheralPowerManager peripheralManager;

// ============================================================================
// END TIER 3 CLASSES
// ============================================================================

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

static void periodicRtcSync()
{
    if (millis() - lastRtcSyncMs < RTC_SYNC_INTERVAL_MS) {
        return;
    }
    rtc->syncToSystem();
    lastRtcSyncMs = millis();
}

static bool shouldEnableMotionWake()
{
    // Check watch orientation to determine if motion wake should be enabled
    // Only enable motion wake if watch is in a vertical/wrist position
    // Disable if laying flat on desk to prevent false wakes from vibrations
    
    uint8_t dir = watch->bma->direction();
    
    // Laying flat - display facing up or down
    if (dir == DIRECTION_DISP_UP || dir == DIRECTION_DISP_DOWN) {
        return false;  // Disable motion wake when flat on desk
    }
    
    // Vertical or tilted position - likely on wrist or being worn
    return true;
}

static void smartWiFiSyncChargingOnly()
{
#ifdef LILYGO_WATCH_HAS_AXP202
    if (!watch || !watch->power || !watch->power->isChargeing()) {
        return;
    }
#else
    return;
#endif

    if (millis() - lastWiFiSyncMs < WIFI_SYNC_INTERVAL_MS) {
        return;
    }

    bool wasConnected = WiFi.isConnected();
    if (!wasConnected) {
        // Tier 3: Notify frequency scaler WiFi is activating
        frequencyScaler.setWiFiActive(true);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(RTC_TIME_ZONE, "pool.ntp.org");
        struct tm nowTm;
        if (getLocalTime(&nowTm, 5000)) {
            rtc->setDateTime((uint16_t)(nowTm.tm_year + 1900),
                             (uint8_t)(nowTm.tm_mon + 1),
                             (uint8_t)nowTm.tm_mday,
                             (uint8_t)nowTm.tm_hour,
                             (uint8_t)nowTm.tm_min,
                             (uint8_t)nowTm.tm_sec);
            rtc->syncToSystem();
            lastRtcSyncMs = millis();
        }
    }

    if (!wasConnected) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        
        // Tier 3: Notify frequency scaler WiFi is done
        frequencyScaler.setWiFiActive(false);
    }

    lastWiFiSyncMs = millis();
}

static void enterLightSleepIfNeeded()
{
    if (displaySleeping) {
        return;
    }

    watch->closeBL();
    watch->displaySleep();
    watch->stopLvglTick();
    backlightDimmed = false;

    // Tier 3: Peripheral manager handles sensor optimization
    peripheralManager.optimizeForState(STATE_LIGHT_SLEEP);

    WiFi.mode(WIFI_OFF);
    
    // Tier 3: Dynamic frequency scaling for ultra-save mode
    frequencyScaler.transitionTo(PROFILE_ULTRA_SAVE);

    displaySleeping = true;
    
    // Always enable power button wake
    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
    
    // Tier 3: Conditional motion wake - only enable if not laying flat
    // DISABLED to prevent spurious wakes causing brightness oscillation
    // This prevents false wakes from desk vibrations when watch is stationary
    if (false && shouldEnableMotionWake()) {
        gpio_wakeup_enable((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
    }
    
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();

    // === WAKE FROM SLEEP - Check wake reason BEFORE waking display ===
    
    // Tier 3: Resume balanced frequency on wake
    frequencyScaler.transitionTo(PROFILE_BALANCED);
    
    // Check if wake was from user button press (not spurious motion)
    bool buttonPressed = (digitalRead(AXP202_INT) == LOW);
    
    if (buttonPressed) {
        // Button press - this is real user interaction
        lastActivityMs = millis();
    }
    
    // Check if we should actually wake up or go back to sleep
    uint32_t inactiveMs = millis() - lastActivityMs;
    
    if (!buttonPressed && inactiveMs >= SCREEN_TIMEOUT_MS) {
        // Spurious wake (motion sensor) with no recent activity
        // Go straight back to sleep without waking display
        // Keep displaySleeping=true and let main loop re-enter sleep
        frequencyScaler.transitionTo(PROFILE_ULTRA_SAVE);
        return;
    }
    
    // === Actually wake the display ===
    watch->displayWakeup();
    watch->startLvglTick();
    rtc->syncToSystem();
    lastRtcSyncMs = millis();

    // Tier 3: Re-enable peripherals for active state
    peripheralManager.optimizeForState(STATE_ACTIVE);

    displaySleeping = false;
    watch->openBL();
    
    // Set brightness based on button press
    if (buttonPressed) {
        // Button pressed - brighten display
        Serial.println("[BRIGHTNESS] Wake from sleep - button pressed - brightening");
        watch->setBrightness(calculateContextualBrightness(false));
        backlightDimmed = false;
        lastBrightnessChangeMs = millis();
        // Clear the power management IRQ
        watch->power->readIRQ();
        watch->power->clearIRQ();
    } else {
        // Wake without button (shouldn't happen now, but handle it)
        Serial.println("[BRIGHTNESS] Wake from sleep - no button - dimmed");
        watch->setBrightness(calculateContextualBrightness(true));
        backlightDimmed = true;
        lastBrightnessChangeMs = millis();
    }
    
    // Tier 3: Force sleep manager back to active
    sleepManager.forceActive();
    
    drawFace(rtc->getDateTime());
}

static void applyBacklightPolicy()
{
    if (displaySleeping) {
        return;
    }
    
    // Simple brightness control: only dim after timeout, never auto-brighten
    // Button press handled separately via touch or can be enabled via IRQ
    
    uint32_t inactiveMs = millis() - lastActivityMs;
    
    // Dim after DIM_TIMEOUT_MS
    if (inactiveMs >= DIM_TIMEOUT_MS && !backlightDimmed) {
        Serial.println("[BRIGHTNESS] Auto-dimming after 15s inactivity");
        watch->setBrightness(calculateContextualBrightness(true));
        backlightDimmed = true;
        lastBrightnessChangeMs = millis();
    }
    
    // Sleep after SCREEN_TIMEOUT_MS (currently disabled to prevent wake oscillation)
    // Uncomment when motion wake issues are resolved
    /*
    if (inactiveMs >= SCREEN_TIMEOUT_MS) {
        enterLightSleepIfNeeded();
    }
    */
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
    static uint32_t lastBatteryUpdateMs = 0;
    static uint32_t cachedRemainingMinutes = 0;
    static bool wasCharging = false;
    static bool initialized = false;
    const uint32_t BATTERY_UPDATE_INTERVAL_MS = 5UL * 60UL * 1000UL;

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
        
        // Initialize timestamp on first run if never set
        if (!initialized) {
            if (lastFullChargeTimestamp == 0) {
                lastFullChargeTimestamp = millis() / 1000UL;
                saveLastFullChargeTimestamp();
            }
            initialized = true;
        }
        
        // Update timestamp when at full charge
        if (charging && pct >= 99) {
            lastFullChargeTimestamp = millis() / 1000UL;
            if (millis() - lastBatteryUpdateMs >= BATTERY_UPDATE_INTERVAL_MS) {
                saveLastFullChargeTimestamp();
            }
        }
        wasCharging = charging;
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

    if (millis() - lastBatteryUpdateMs >= BATTERY_UPDATE_INTERVAL_MS || lastBatteryUpdateMs == 0) {
        uint32_t remainSeconds = estimateBatterySecondsRemaining(pct, charging, chargeCurrentMa, dischargeCurrentMa);
        cachedRemainingMinutes = remainSeconds / 60UL;
        lastBatteryUpdateMs = millis();
    }

    char minBuf[8];
    snprintf(minBuf, sizeof(minBuf), "%lum", (unsigned long)cachedRemainingMinutes);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(TR_DATUM);
    canvas->setTextFont(1);
    canvas->drawString(minBuf, SCREEN_W - 1, by + bh + 4, 1);
    
    // Draw elapsed time since last full charge below remaining time
    if (lastFullChargeTimestamp > 0) {
        uint32_t currentSeconds = millis() / 1000UL;
        uint32_t elapsedSeconds = currentSeconds - lastFullChargeTimestamp;
        uint32_t elapsedMinutes = elapsedSeconds / 60UL;
        
        char elapsedBuf[8];
        snprintf(elapsedBuf, sizeof(elapsedBuf), "+%lum", (unsigned long)elapsedMinutes);
        canvas->setTextColor(TFT_CYAN, TFT_BLACK);
        canvas->drawString(elapsedBuf, SCREEN_W - 1, by + bh + 4 + 10, 1);
    }
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
        
        // Touch can now brighten display (simpler than IRQ)
        if (backlightDimmed) {
            watch->setBrightness(calculateContextualBrightness(false));
            backlightDimmed = false;
            lastBrightnessChangeMs = millis();
        }
        lastActivityMs = millis();
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

    // Tier 1: minimum CPU frequency for time accuracy (20 MHz proven by BatmanDial)
    setCpuFrequencyMhz(20);

    // Tier 1: disable WiFi by default
    WiFi.mode(WIFI_OFF);

    // Keep LVGL clock active because this is an LVGL-configured example folder.
    watch->lvgl_begin();

    // Tier 1: Start with dim brightness (only button press will brighten)
    watch->setBrightness(calculateContextualBrightness(true));

#ifdef LILYGO_WATCH_HAS_AXP202
    watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
    
    // Clear any pending IRQs before enabling
    watch->power->clearIRQ();
    
    // Only enable power button short press IRQ for brightness control
    watch->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, AXP202_ON);
    watch->power->clearIRQ();
    
    // Tier 1: disable audio rail when not used
    watch->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
#endif

    // Tier 2: sensor policy defaults while screen is active
    watch->bma->enableWakeupInterrupt(false);
    watch->bma->enableAccel();
    watch->bma->enableStepCountInterrupt(false);  // Temporarily disabled to diagnose brightness issue

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

    // Tier 3: Initialize power managers
    frequencyScaler.transitionTo(PROFILE_BALANCED);
    sleepManager.forceActive();
    peripheralManager.optimizeForState(STATE_ACTIVE);
    aodManager.setEnabled(false);  // AOD disabled by default
    Serial.println("Tier 3 power managers initialized");

    drawFace(rtc->getDateTime());

    // Start with full brightness, will auto-dim after 15 seconds
    lastActivityMs = millis();
    backlightDimmed = false;
    watch->setBrightness(calculateContextualBrightness(false));
    lastBrightnessChangeMs = millis();
    
    lastRtcSyncMs = millis();
    lastWiFiSyncMs = 0;
    
    Serial.println("[BRIGHTNESS] Setup complete - display active, will dim in 15s");
}

void loop()
{
    static uint8_t lastSecond = 255;
    static uint32_t lastUiFrame = 0;

    handleSerialCommands();
    pollTouchUI();

    // Tier 2 policies
    applyBacklightPolicy();
    periodicRtcSync();
    smartWiFiSyncChargingOnly();
    
    // Tier 3: Update dynamic frequency scaling
    frequencyScaler.update();

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
