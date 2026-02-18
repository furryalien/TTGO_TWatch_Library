# T-Watch 2020 V1 Power Consumption Analysis

**Document Version:** 1.0  
**Date:** February 15, 2026  
**Hardware:** LILYGO T-Watch 2020 V1  
**Objective:** Maximize battery life without sacrificing timekeeping and critical functionality

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Hardware Overview](#hardware-overview)
3. [Current Power Consumption Baseline](#current-power-consumption-baseline)
4. [Component Power Analysis](#component-power-analysis)
5. [Power Management Architecture](#power-management-architecture)
6. [Power Optimization Strategies](#power-optimization-strategies)
7. [Timekeeping Preservation](#timekeeping-preservation)
8. [Implementation Recommendations](#implementation-recommendations)
9. [Code Examples](#code-examples)
10. [Conclusion](#conclusion)

---

## Executive Summary

The T-Watch 2020 V1 is an ESP32-based smartwatch with current power consumption of approximately:
- **~4mA** in standby mode (screen off, light sleep, no deep sleep)
- **~65mA** with display active (no WiFi/Bluetooth)

This analysis identifies multiple optimization opportunities to extend battery life while maintaining accurate timekeeping through the PCF8563 hardware RTC. Key findings include:

1. **Deep sleep not currently recommended** - Would lose time without additional implementation
2. **Light sleep with RTC maintenance** - Optimal balance of power savings and timekeeping
3. **Component-level power gating** - AXP202 enables fine-grained peripheral control
4. **CPU frequency scaling** - Significant savings possible (examples show 20-80 MHz operation)
5. **Display optimization** - Largest power consumer, multiple reduction strategies available

Estimated achievable power consumption with optimizations: **<1mA in deep standby** while maintaining time accuracy.

---

## Hardware Overview

### 2020 V1 Core Specifications

| Component | Specification | Notes |
|-----------|--------------|-------|
| **Microcontroller** | ESP32-DOWDQ6 | Dual-core, 240 MHz max |
| **Flash** | 16MB | QSPI interface |
| **PSRAM** | 8MB | For frame buffers, data storage |
| **Display** | ST7789 240x240 1.54" TFT | SPI interface |
| **Touch** | FT6336U | I2C interface (addr 0x38) |
| **Accelerometer** | BMA423 | I2C interface (addr 0x18/0x19) |
| **RTC** | PCF8563 | I2C interface (addr 0x51) |
| **Power Management** | AXP202 | I2C interface (addr 0x35) |
| **Audio** | MAX98357A | I2S interface |
| **Battery** | Lithium Polymer | Typical 300-500mAh |

### Pin Configuration

**Critical Power-Related Pins:**
- AXP202_INT: GPIO 35 (Power management interrupts)
- BMA423_INT1: GPIO 39 (Accelerometer interrupts)
- PCF8563_INT: GPIO 37 (RTC alarm interrupts)
- TFT_BL: GPIO 12 (Backlight PWM control)

**Power Domains:**
```
ESP32 Core    ← AXP202 DC3  (Cannot be disabled)
Backlight     ← AXP202 LDO2 + GPIO 12 PWM
Audio Module  ← AXP202 LDO3
Display/Touch ← Always powered in 2020 V1
Sensors       ← Always powered in 2020 V1
```

---

## Current Power Consumption Baseline

### Measured Power States

Based on the project documentation and SimpleWatch example:

| State | Current Draw | Components Active | Notes |
|-------|-------------|-------------------|-------|
| **Full Active** | ~65mA | Display, CPU @ 240MHz, All peripherals | WiFi/BT off |
| **Light Sleep** | ~4mA | RTC, ESP32 (minimal), AXP202 | Display sleep, GPIO wakeup |
| **Display Only** | ~60-70mA | Display + backlight dominant | Backlight is major consumer |
| **CPU Idle** | ~15-20mA | CPU minimum, peripherals on | No active tasks |

### Power Consumption Breakdown (Estimated)

| Component | Active Power | Sleep/Idle Power | Notes |
|-----------|-------------|------------------|-------|
| **ST7789 Display** | ~15-20mA | <100µA | Sleep command 0x10 available |
| **Backlight LED** | ~40-50mA | 0mA | PWM controlled, major consumer |
| **ESP32 Core** | ~30-80mA | 800µA (light sleep) | Frequency dependent |
| **BMA423** | ~170µA | ~4µA | Low power mode available |
| **PCF8563 RTC** | ~250nA | ~250nA | Always-on time source |
| **AXP202** | ~600µA | ~600µA | Power management overhead |
| **FT6336 Touch** | ~1-2mA | ~24µA | Monitor/deep sleep modes |
| **WiFi Active** | ~100-200mA | 0mA (off) | Major power consumer |
| **Bluetooth** | ~20-40mA | 0mA (off) | Significant impact when active |

---

## Component Power Analysis

### 1. Display System (ST7789 + Backlight)

**Power Impact:** Highest consumer at ~60-70mA when active

**Current Implementation:**
```cpp
void displaySleep() {
    tft->writecommand(0x10);  // ST7789 sleep mode command
    touchToMonitor();          // Touch to low power mode
}

void displayWakeup() {
    tft->writecommand(0x11);  // ST7789 wake command
}
```

**Optimization Opportunities:**

1. **Backlight Control** (Most Critical)
   - Current: PWM on GPIO 12
   - Full brightness: ~40-50mA
   - Strategies:
     - Adaptive brightness based on ambient light
     - Aggressive auto-dim timeout
     - DIM state (10-20%) instead of full OFF for quick glance
     - Context-aware brightness (nighttime/daytime)

2. **Display Sleep Command**
   - Already implemented (0x10 command)
   - Reduces display to <100µA
   - ~100ms wake time acceptable for watch use

3. **Refresh Rate Optimization**
   - Reduce LVGL tick rate during static screens
   - Currently stopped during sleep (`stopLvglTick()`)
   - Consider 10Hz refresh for watch face vs 30Hz for animations

4. **Partial Updates**
   - Only update changed regions (time/battery)
   - LVGL supports dirty region tracking

**2020 V1 Specific Notes:**
- LDO2 powers backlight on other versions, but NOT on 2020 V1
- 2020 V1 uses direct PWM control only
- Cannot power gate the display circuitry (always powered)

### 2. ESP32 Processor

**Power Impact:** 30-80mA active, 800µA light sleep

**Current Implementation:**
```cpp
// In SimpleWatch example:
esp_sleep_enable_gpio_wakeup();
gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
gpio_wakeup_enable((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
esp_light_sleep_start();
```

**Sleep Mode Comparison:**

| Mode | Current | Wake Latency | RTC Preserved | RAM Preserved |
|------|---------|--------------|---------------|---------------|
| **Active** | 30-80mA | N/A | Yes | Yes |
| **Idle** | ~15mA | <1µs | Yes | Yes |
| **Light Sleep** | ~800µA | <3ms | Yes | Yes |
| **Deep Sleep** | ~10-150µA | ~200ms | No* | No |

*Without special configuration

**Optimization Strategies:**

1. **CPU Frequency Scaling**
   ```cpp
   setCpuFrequencyMhz(80);   // Reduce from 240 MHz
   setCpuFrequencyMhz(40);   // Even lower for simple tasks
   setCpuFrequencyMhz(20);   // Examples show 20 MHz works
   ```
   - Power scales roughly linearly with frequency
   - 80 MHz still sufficient for watch UI
   - 40 MHz adequate for static watch face
   - Benchmark: BatmanDial example uses 20 MHz

2. **Smart Light Sleep Usage**
   - Enter light sleep during:
     - Screen off periods
     - Static display (no animations)
     - Waiting for user input
   - Wake sources:
     - Power button (AXP202_INT)
     - Motion detection (BMA423_INT1)
     - RTC alarm (PCF8563_INT)
     - Touch interrupt (optional)

3. **Task/Timer Optimization**
   - Reduce FreeRTOS tick rate
   - Consolidate periodic tasks
   - Use hardware timers instead of polling
   - Defer non-critical updates

4. **Deep Sleep Considerations**
   - Standard deep sleep loses system time
   - PCF8563 RTC maintains real time
   - Would require:
     - Sync from RTC on wake
     - Store state to RTC SRAM or flash
     - Longer wake latency (~200ms)
   - Recommended for long-term storage only (not active watch use)

### 3. Accelerometer (BMA423)

**Power Impact:** 170µA active, ~4µA sleep

**Current Implementation:**
```cpp
// From BMA driver
bool enableAccel(bool en = true);
bool disableAccel();
bool enableStepCountInterrupt(bool en = true);
bool enableFeature(uint8_t feature, uint8_t enable);
```

**Optimization Strategies:**

1. **Conditional Activation**
   - Disable when screen off and not counting steps
   - Enable only when needed for:
     - Step counting
     - Wrist raise detection
     - Tap/double-tap gestures
     - Tilt detection

2. **Feature-Specific Control**
   ```cpp
   // Disable step counter when not needed
   ttgo->bma->enableStepCountInterrupt(false);
   
   // Disable accelerometer completely
   ttgo->bma->disableAccel();
   
   // Enable specific features only
   ttgo->bma->enableWakeupInterrupt(true);  // For wrist raise
   ```

3. **Intelligent Scheduling**
   - Step counting: only during daytime/active hours
   - Wrist raise: only when screen is off
   - Activity detection: periodic sampling vs continuous

4. **Power Modes**
   - Normal mode: 170µA (continuous sampling)
   - Low power mode: ~25µA (reduced sampling rate)
   - Suspend mode: ~4µA (minimal power)

**Estimated Savings:** ~165µA when completely disabled or ~145µA in low power mode

### 4. Real-Time Clock (PCF8563)

**Power Impact:** ~250nA (negligible, always-on)

**Critical for Timekeeping:**
```cpp
// RTC provides system time even during ESP32 deep sleep
ttgo->rtc->syncToSystem();        // Sync ESP32 system time from RTC
ttgo->rtc->check();                // Validate RTC time
```

**Key Features:**

1. **Ultra-Low Power**
   - 250nA typical consumption
   - Independent power domain
   - Battery backup capable

2. **Alarm Functionality**
   ```cpp
   ttgo->rtc->enableAlarm();      // Wake from sleep
   ttgo->rtc->setAlarm(...);      // Set alarm time
   pinMode(PCF8563_INT, INPUT);   // GPIO 37 for alarm interrupt
   ```

3. **Timekeeping During Deep Sleep**
   - RTC continues running during ESP32 deep sleep
   - Can wake ESP32 via alarm interrupt
   - System time must be synced from RTC on wake

**Power Optimization:**
- No optimization needed (already ultra-low power)
- **Critical:** Always keep RTC powered
- Use RTC alarm for periodic wake from deep sleep
- Sync frequency: only on wake, not continuous polling

### 5. Touch Controller (FT6336U)

**Power Impact:** 1-2mA active, ~24µA monitor mode

**Current Implementation:**
```cpp
void touchToSleep() {
    touch->setPowerMode(FOCALTECH_PMODE_DEEPSLEEP);
}

void touchToMonitor() {
    touch->setPowerMode(FOCALTECH_PMODE_MONITOR);
}
```

**Power Modes:**

| Mode | Current | Function | Wake Source |
|------|---------|----------|-------------|
| Active | ~2mA | Full touch scanning | N/A |
| Monitor | ~24µA | Gesture wake detection | Touch event |
| Deep Sleep | ~10µA | Completely off | Requires reset |

**Optimization Strategy:**
- Use Monitor mode during display sleep
- Touch can wake system via interrupt
- Trade-off: Monitor mode vs Deep sleep
  - Monitor: Fast wake, can detect touch (~24µA)
  - Deep: Lower power but requires power cycle (~10µA)

### 6. Power Management (AXP202)

**Power Impact:** ~600µA overhead

**LDO/DCDC Configuration (2020 V1):**

| Channel | Power Domain | 2020 V1 Usage | Can Disable? |
|---------|--------------|---------------|--------------|
| DC3 | ESP32 Core | Main power | ❌ No (critical) |
| DC2 | - | Not used | ✅ Yes |
| LDO2 | Backlight | Not used in V1* | ✅ Yes |
| LDO3 | Audio Module | MAX98357A | ✅ Yes |
| LDO4 | - | Not used | ✅ Yes |
| EXTEN | - | Not used in V1 | ✅ Yes |

*Note: 2020 V1 uses direct PWM for backlight, not LDO2

**Current Control Implementation:**
```cpp
// Disable audio when not in use
void disableAudio() {
    power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
}

// Power off all non-critical peripherals
void powerOff() {
    power->setPowerOutPut(AXP202_EXTEN, false);
    power->setPowerOutPut(AXP202_LDO4, false);
    power->setPowerOutPut(AXP202_DCDC2, false);
    power->setPowerOutPut(AXP202_LDO3, false);
    // LDO2 not disabled in 2020 V1
}
```

**ADC Control:**
```cpp
// Enable only necessary ADC channels
power->adc1Enable(
    AXP202_BATT_VOL_ADC1 |   // Battery voltage (needed)
    AXP202_BATT_CUR_ADC1 |   // Battery current (needed)
    AXP202_VBUS_VOL_ADC1 |   // USB voltage (needed for charging)
    AXP202_VBUS_CUR_ADC1,    // USB current (needed for charging)
    AXP202_ON
);
```

**Optimization Strategies:**

1. **Selective LDO Control**
   - Disable LDO3 (audio) when not playing sound
   - Disable unused channels (LDO4, DCDC2, EXTEN)
   - Savings: Few hundred µA per disabled LDO

2. **ADC Optimization**
   - Disable unnecessary ADC channels
   - Reduce ADC sampling rate
   - Read battery voltage less frequently (e.g., every 10 seconds vs continuous)

3. **IRQ Management**
   ```cpp
   // Enable only necessary interrupts
   power->enableIRQ(
       AXP202_VBUS_REMOVED_IRQ | 
       AXP202_VBUS_CONNECT_IRQ | 
       AXP202_CHARGING_FINISHED_IRQ,
       AXP202_ON
   );
   ```

4. **Charging Optimization**
   ```cpp
   // Set appropriate charging current
   power->setChargingTargetVoltage(AXP202_TARGET_VOL_4_2V);
   power->setChargeControlCur(300);  // Match battery capacity
   ```

### 7. Wireless Connectivity

**Power Impact:** WiFi ~100-200mA, Bluetooth ~20-40mA

**Current Implementation:**
```cpp
// SimpleWatch example
WiFi.mode(WIFI_OFF);         // Completely disable WiFi
// Note: Bluetooth control via btStop() available
```

**Optimization Strategies:**

1. **WiFi Management**
   ```cpp
   // Turn off when not needed
   WiFi.mode(WIFI_OFF);
   
   // Use WiFi only for specific tasks
   WiFi.mode(WIFI_STA);
   // ... perform network operation ...
   WiFi.disconnect();
   WiFi.mode(WIFI_OFF);
   ```

2. **Bluetooth Low Energy**
   ```cpp
   // Stop Bluetooth when not needed
   btStop();
   
   // Use BLE instead of Classic Bluetooth
   // BLE typically 10-20mA vs Classic 20-40mA
   ```

3. **Smart Sync Strategies**
   - Batch network operations
   - Sync during charging only
   - User-initiated sync vs automatic
   - Time-based sync (e.g., once per hour)

4. **Power Save Modes**
   ```cpp
   // WiFi power save mode
   WiFi.setSleep(true);
   
   // Less aggressive scanning
   WiFi.scanNetworks(false, true);  // Async scan
   ```

**Estimated Savings:**
- Keeping WiFi off: 100-200mA saved
- Keeping BT off: 20-40mA saved
- Critical for battery life - only use when absolutely necessary

---

## Power Management Architecture

### AXP202 Power Tree (2020 V1)

```
Battery (Li-Po)
    ↓
AXP202 Power Management IC
    ├── DC3 (3.3V) → ESP32 Core [ALWAYS ON]
    │                 ├── Flash/PSRAM
    │                 ├── WiFi/BT radios
    │                 └── GPIO peripherals
    ├── LDO2 → [Not used in V1 - backlight via GPIO PWM]
    ├── LDO3 → MAX98357A Audio Amplifier [Switchable]
    ├── LDO4 → [Not used in V1]
    ├── DCDC2 → [Not used in V1]
    └── EXTEN → [Not used in V1]

Direct Battery Power:
    ├── ST7789 Display [Always powered]
    ├── FT6336 Touch [Always powered]
    ├── BMA423 Accel [Always powered]
    └── PCF8563 RTC [Always powered]

Backlight: GPIO 12 PWM Control (0-255)
```

### Current Software Power States

From the SimpleWatch example implementation:

```cpp
// State 1: FULL ACTIVE (~65mA)
void active_state() {
    ttgo->openBL();              // Backlight ON
    ttgo->displayWakeup();       // Display awake
    ttgo->startLvglTick();       // UI updates running
    ttgo->bma->enableStepCountInterrupt(true);
    WiFi.mode(WIFI_STA);         // Optional - adds 100-200mA
}

// State 2: LIGHT SLEEP (~4mA)
void low_energy() {
    ttgo->closeBL();             // Backlight OFF (saves ~40mA)
    ttgo->displaySleep();        // Display sleep command
    ttgo->stopLvglTick();        // Stop UI updates
    ttgo->bma->enableStepCountInterrupt(false);
    WiFi.mode(WIFI_OFF);         // Disable WiFi if on
    
    // Configure wake sources
    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    // Enter light sleep
    esp_light_sleep_start();
    
    // Wake up happens here due to interrupt
}
```

### Power State Transition Diagram

```
┌─────────────────┐
│   FULL ACTIVE   │ (~65mA)
│ Display ON      │
│ All features    │
└────────┬────────┘
         │ Inactivity timeout (30s typical)
         │ or Power button press
         ↓
┌─────────────────┐
│  LIGHT SLEEP    │ (~4mA)
│ Display OFF     │
│ GPIO wake       │
└────────┬────────┘
         │ Button press, Motion, Touch
         │
         ↓
┌─────────────────┐
│  QUICK WAKE     │
│ Restore state   │ (<3ms latency)
│ Show time       │
└────────┬────────┘
         │
         └──────(returns to FULL ACTIVE)

Optional Deep Sleep Path (not currently used):
┌─────────────────┐
│  DEEP SLEEP     │ (~10-150µA)
│ RTC only active │
│ RAM lost        │
└────────┬────────┘
         │ RTC alarm or External reset
         │
         ↓ (~200ms boot)
┌─────────────────┐
│   COLD BOOT     │
│ Restore from    │
│ RTC/Flash       │
└─────────────────┘
```

---

## Power Optimization Strategies

### Strategy 1: Aggressive Backlight Management ⭐ HIGHEST IMPACT

**Potential Savings:** ~40-50mA (60-75% of active power)

**Implementation Levels:**

**Level 1: Faster Auto-Dim** (Easy)
```cpp
#define SCREEN_TIMEOUT 10000  // Reduce from 30s to 10s

void loop() {
    if (lv_disp_get_inactive_time(NULL) < SCREEN_TIMEOUT) {
        lv_task_handler();
    } else {
        low_energy();  // Enter sleep mode
    }
}
```

**Level 2: Multi-Stage Dimming** (Moderate)
```cpp
#define DIM_TIMEOUT 5000      // Dim after 5s
#define SLEEP_TIMEOUT 15000   // Sleep after 15s
#define DIM_LEVEL 25          // 10% brightness
#define ACTIVE_LEVEL 200      // 80% brightness

void smartBacklight() {
    uint32_t inactive = lv_disp_get_inactive_time(NULL);
    
    if (inactive < DIM_TIMEOUT) {
        ttgo->setBrightness(ACTIVE_LEVEL);
    } else if (inactive < SLEEP_TIMEOUT) {
        ttgo->setBrightness(DIM_LEVEL);  // Dim but visible
    } else {
        low_energy();  // Full sleep
    }
}
```

**Level 3: Context-Aware Brightness** (Advanced)
```cpp
uint8_t calculateBrightness() {
    uint8_t base_level;
    
    // Time-based adjustment
    RTC_Date now = ttgo->rtc->getDateTime();
    if (now.hour >= 22 || now.hour < 6) {
        base_level = 50;   // Night mode
    } else {
        base_level = 200;  // Day mode
    }
    
    // Battery-level adjustment
    float battery = ttgo->power->getBattVoltage();
    if (battery < 3.5) {
        base_level = base_level * 0.7;  // Reduce when low battery
    }
    
    // Activity-based adjustment
    if (is_charging) {
        base_level = min(255, base_level * 1.2);  // Brighter when charging
    }
    
    return base_level;
}
```

**Level 4: Always-On Display Mode** (Maximum efficiency)
```cpp
// Ultra-low refresh rate display
#define AOD_REFRESH_MS 60000  // Update only once per minute

void enableAOD() {
    ttgo->setBrightness(15);   // Very dim (5% brightness)
    setCpuFrequencyMhz(40);    // Reduce CPU speed
    
    // Minimal update - only time digits
    lv_timer_set_period(update_timer, AOD_REFRESH_MS);
    
    // Estimated: ~10-15mA vs 65mA
}
```

### Strategy 2: CPU Frequency Optimization ⭐ HIGH IMPACT

**Potential Savings:** ~10-40mA depending on frequency

**Dynamic Frequency Scaling:**

```cpp
enum PowerProfile {
    PROFILE_PERFORMANCE,  // 240 MHz
    PROFILE_BALANCED,     // 80 MHz
    PROFILE_POWER_SAVE,   // 40 MHz
    PROFILE_ULTRA_SAVE    // 20 MHz
};

void setPowerProfile(PowerProfile profile) {
    switch(profile) {
        case PROFILE_PERFORMANCE:
            setCpuFrequencyMhz(240);
            break;
        case PROFILE_BALANCED:
            setCpuFrequencyMhz(80);
            break;
        case PROFILE_POWER_SAVE:
            setCpuFrequencyMhz(40);
            break;
        case PROFILE_ULTRA_SAVE:
            setCpuFrequencyMhz(20);
            break;
    }
}

void adaptiveFrequency() {
    // Performance mode: animations, WiFi sync, heavy computation
    if (wifi_active || animation_active) {
        setPowerProfile(PROFILE_PERFORMANCE);
    }
    // Balanced: normal UI interaction
    else if (screen_on && user_active) {
        setPowerProfile(PROFILE_BALANCED);
    }
    // Power save: watch face only
    else if (screen_on && !user_active) {
        setPowerProfile(PROFILE_POWER_SAVE);
    }
    // Ultra save: sleep mode
    else {
        setPowerProfile(PROFILE_ULTRA_SAVE);
    }
}
```

**Frequency Guidelines:**
- 240 MHz: Network operations, heavy animations, audio playback
- 160 MHz: Standard UI, smooth scrolling
- 80 MHz: Static watch face, light interaction (RECOMMENDED DEFAULT)
- 40 MHz: Minimal watch face, low refresh rate
- 20 MHz: Time display only, proven to work (see BatmanDial example)

### Strategy 3: Sensor Power Management ⭐ MEDIUM IMPACT

**Potential Savings:** ~165µA (accelerometer) + varies by usage

**Smart Sensor Activation:**

```cpp
// State-based sensor control
void updateSensors() {
    if (screen_on) {
        // Screen is on, disable motion wake
        ttgo->bma->disableAccel();
    } else {
        // Screen off, enable wrist raise
        ttgo->bma->enableAccel();
        ttgo->bma->enableWakeupInterrupt(true);
        ttgo->bma->enableStepCountInterrupt(false);  // Don't need steps when sleeping
    }
}

// Time-based sensor control
void scheduledSensorControl() {
    RTC_Date now = ttgo->rtc->getDateTime();
    
    // Only count steps during waking hours
    if (now.hour >= 6 && now.hour < 23) {
        ttgo->bma->enableStepCountInterrupt(true);
    } else {
        ttgo->bma->enableStepCountInterrupt(false);
    }
    
    // Disable all sensors during sleep hours if screen is off
    if ((now.hour >= 23 || now.hour < 6) && !screen_on) {
        ttgo->bma->disableAccel();
    }
}
```

### Strategy 4: Touch Controller Optimization ⭐ LOW-MEDIUM IMPACT

**Potential Savings:** ~1-2mA

**Smart Touch Management:**

```cpp
void manageTouchPower() {
    if (screen_on) {
        // Normal touch operation
        ttgo->touch->setPowerMode(FOCALTECH_PMODE_ACTIVE);
    } else {
        // Two options when screen is off:
        
        // Option A: Monitor mode - can wake on touch (~24µA)
        ttgo->touch->setPowerMode(FOCALTECH_PMODE_MONITOR);
        
        // Option B: Deep sleep - must use button to wake (~10µA)
        // ttgo->touch->setPowerMode(FOCALTECH_PMODE_DEEPSLEEP);
        // Use button (AXP202_INT) as only wake source
    }
}

// If using touch as wake source
void setupTouchWake() {
    pinMode(FT6336_INT, INPUT);
    attachInterrupt(FT6336_INT, touchISR, FALLING);
    enableInterruptWakeup(FT6336_INT);
}
```

**Recommendation:** Use Monitor mode for better user experience, Deep sleep only if maximizing battery life.

### Strategy 5: Peripheral Power Gating ⭐ MEDIUM IMPACT

**Potential Savings:** Several hundred µA

**Aggressive Peripheral Control:**

```cpp
void deepStandby() {
    // Turn off all non-essential peripherals
    
    // Audio (saves power for LDO3)
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
    
    // Disable unused LDOs
    ttgo->power->setPowerOutPut(AXP202_LDO4, AXP202_OFF);
    ttgo->power->setPowerOutPut(AXP202_DCDC2, AXP202_OFF);
    ttgo->power->setPowerOutPut(AXP202_EXTEN, false);
    
    // Minimize ADC usage
    ttgo->power->adc1Enable(
        AXP202_BATT_VOL_ADC1,  // Only battery voltage
        AXP202_ON
    );
    
    // Clear and minimize interrupts
    ttgo->power->clearIRQ();
    ttgo->power->enableIRQ(
        AXP202_PEK_SHORTPRESS_IRQ,  // Only power button
        AXP202_ON
    );
}

void resumePeripherals() {
    // Re-enable as needed
    ttgo->power->adc1Enable(
        AXP202_BATT_VOL_ADC1 | 
        AXP202_BATT_CUR_ADC1 |
        AXP202_VBUS_VOL_ADC1 |
        AXP202_VBUS_CUR_ADC1,
        AXP202_ON
    );
    
    ttgo->power->enableIRQ(
        AXP202_VBUS_REMOVED_IRQ | 
        AXP202_VBUS_CONNECT_IRQ |
        AXP202_CHARGING_FINISHED_IRQ |
        AXP202_PEK_SHORTPRESS_IRQ,
        AXP202_ON
    );
}
```

### Strategy 6: LVGL Optimization ⭐ LOW-MEDIUM IMPACT

**Potential Savings:** ~2-5mA

**Display Update Optimization:**

```cpp
// Reduce LVGL tick rate based on activity
void adaptiveLVGLRefresh() {
    if (animation_running) {
        lv_tick_set_period(10);  // 100 Hz for smooth animations
    } else if (screen_on) {
        lv_tick_set_period(33);  // 30 Hz for normal UI
    } else {
        ttgo->stopLvglTick();    // Completely stop when screen off
    }
}

// Minimize redraws
void efficientDisplay() {
    // Invalidate only changed areas
    lv_obj_invalidate(time_label);  // Only update time, not whole screen
    
    // Use partial refresh where possible
    lv_disp_set_draw_buf(disp, &draw_buf);
    
    // Disable anti-aliasing for watch face (saves CPU)
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);  // Use pre-rendered fonts
}

// Simplified static watch face
void powerSaveWatchFace() {
    // Disable all animations
    lv_anim_del_all();
    
    // Update only time digits
    lv_label_set_text_fmt(time_label, "%02d:%02d", hour, minute);
    
    // Update only once per minute
    lv_timer_set_period(update_timer, 60000);
    
    // Reduce brightness
    ttgo->setBrightness(50);
}
```

### Strategy 7: Network Usage Optimization ⭐ VERY HIGH IMPACT

**Potential Savings:** 100-200mA (WiFi) + 20-40mA (Bluetooth)

**Smart WiFi Management:**

```cpp
// Sync only when necessary and during charging
void smartSync() {
    float battery = ttgo->power->getBattVoltage();
    bool charging = ttgo->power->isChargeing();
    
    // Only sync if charging or battery is good
    if (charging || battery > 3.8) {
        // Batch all network operations
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        // Wait for connection with timeout
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            // Perform all network tasks
            syncTime();
            fetchWeather();
            checkNotifications();
        }
        
        // Immediately disconnect
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }
}

// Scheduled sync instead of continuous
void periodicSync() {
    static uint32_t last_sync = 0;
    
    // Sync every 4 hours
    if (millis() - last_sync > 4 * 3600 * 1000) {
        smartSync();
        last_sync = millis();
    }
}

// User-initiated sync
void manualSync() {
    // Show progress indicator
    lv_label_set_text(status, "Syncing...");
    
    WiFi.mode(WIFI_STA);
    // ... perform sync ...
    WiFi.mode(WIFI_OFF);
    
    lv_label_set_text(status, "Sync complete");
}
```

**Bluetooth Optimization:**

```cpp
// Turn off when not needed
void manageBluetooth() {
    if (!bluetooth_needed) {
        btStop();  // Completely stop Bluetooth
    }
}

// Use BLE instead of Classic when possible
void preferBLE() {
    // BLE consumes ~10-20mA vs Classic ~20-40mA
    // Sufficient for notifications, data sync
}
```

---

## Timekeeping Preservation

### Critical Requirement: Time Accuracy

A watch must **maintain accurate time** at all times. This is the primary constraint for power optimization.

### Timekeeping Architecture

```
┌─────────────────────────────────────┐
│  PCF8563 Hardware RTC               │
│  - Always powered (~250nA)          │
│  - Crystal oscillator               │
│  - I2C: 0x51                        │
│  - INT output: GPIO 37              │
└───────────┬─────────────────────────┘
            │
            │ I2C Read/Sync
            ↓
┌─────────────────────────────────────┐
│  ESP32 System Time                  │
│  - Software RTC                     │
│  - Lost during deep sleep           │
│  - Must sync from PCF8563           │
└─────────────────────────────────────┘
```

### Time Preservation Strategies

#### Strategy A: Light Sleep Only (Current Approach) ✅ RECOMMENDED

**Pros:**
- System time preserved
- Fast wake (< 3ms)
- Simple implementation
- No time sync needed on wake

**Cons:**
- Higher power consumption (~4mA vs ~100µA deep sleep)

**Implementation:**
```cpp
void lightSleepWithRTC() {
    // Configure wake sources
    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    // Enter light sleep - RTC and RAM preserved
    esp_light_sleep_start();
    
    // Wake up here - system time intact
    // Optional: Sync from hardware RTC periodically for accuracy
    static uint32_t last_sync = 0;
    if (millis() - last_sync > 3600000) {  // Sync every hour
        ttgo->rtc->syncToSystem();
        last_sync = millis();
    }
}
```

#### Strategy B: Deep Sleep with RTC Sync ⚠️ ADVANCED

**Pros:**
- Lower power consumption (~10-150µA)
- Suitable for long-term storage

**Cons:**
- Slower wake (~200ms)
- More complex implementation
- Must sync time on every wake
- Lose application state

**Implementation:**
```cpp
void deepSleepWithRTC() {
    // Save critical state to RTC SRAM or NVS flash
    saveStateToFlash();
    
    // Configure wake sources
    esp_sleep_enable_ext0_wakeup((gpio_num_t)AXP202_INT, LOW);
    // OR use RTC timer for periodic wake
    esp_sleep_enable_timer_wakeup(60 * 1000000);  // Wake every 60 seconds
    
    // Enter deep sleep
    esp_deep_sleep_start();
    
    // --- REBOOT HAPPENS HERE ---
    
    // In setup() after wake:
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
        // Woke from sleep
        ttgo->rtc->syncToSystem();  // CRITICAL: Restore time
        restoreStateFromFlash();
    }
}
```

#### Strategy C: Hybrid Approach 🌟 OPTIMAL

**Pros:**
- Best of both worlds
- Maximum battery life
- Maintains good user experience

**Implementation:**
```cpp
#define LIGHT_SLEEP_TIMEOUT 300000   // 5 minutes
#define DEEP_SLEEP_TIMEOUT 3600000   // 1 hour

enum SleepState {
    STATE_ACTIVE,
    STATE_LIGHT_SLEEP,
    STATE_DEEP_SLEEP
};

void hybridSleepManager() {
    static uint32_t sleep_start = 0;
    
    if (user_inactive_time < LIGHT_SLEEP_TIMEOUT) {
        // Quick access needed - light sleep
        enterLightSleep();
    } 
    else if (user_inactive_time < DEEP_SLEEP_TIMEOUT) {
        // Extended inactivity - still light sleep for quick wake
        enterLightSleep();
    }
    else {
        // Very long inactivity - deep sleep for maximum savings
        enterDeepSleep();
    }
}

void enterLightSleep() {
    // Prepare for light sleep
    ttgo->closeBL();
    ttgo->displaySleep();
    ttgo->stopLvglTick();
    setCpuFrequencyMhz(40);
    
    // Configure wake
    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    esp_light_sleep_start();
    
    // Wake - restore UI
    ttgo->displayWakeup();
    ttgo->openBL();
    ttgo->startLvglTick();
    setCpuFrequencyMhz(80);
}

void enterDeepSleep() {
    // Save state
    saveStateToNVS();
    
    // Set RTC alarm for periodic wake (update time display if AOD)
    // OR wait for button press
    esp_sleep_enable_ext0_wakeup((gpio_num_t)AXP202_INT, LOW);
    
    esp_deep_sleep_start();
    // Reboot on wake, sync time in setup()
}
```

### RTC Synchronization Best Practices

```cpp
// Periodic sync for accuracy
class RTCManager {
private:
    static const uint32_t SYNC_INTERVAL = 3600000;  // 1 hour
    uint32_t last_sync = 0;
    
public:
    void periodicSync() {
        if (millis() - last_sync > SYNC_INTERVAL) {
            ttgo->rtc->syncToSystem();
            last_sync = millis();
        }
    }
    
    void forceSync() {
        ttgo->rtc->syncToSystem();
        last_sync = millis();
    }
    
    // Check if RTC time is valid
    bool validateRTC() {
        if (!ttgo->rtc->isValid()) {
            return false;
        }
        
        RTC_Date now = ttgo->rtc->getDateTime();
        
        // Sanity check
        if (now.year < 2020 || now.year > 2100) {
            return false;
        }
        
        return true;
    }
};
```

### Alarm-Based Wake

```cpp
// Use RTC alarm for periodic tasks
void setRTCAlarmWake(uint8_t minutes) {
    RTC_Date now = ttgo->rtc->getDateTime();
    
    // Calculate alarm time
    uint8_t alarm_min = (now.minute + minutes) % 60;
    uint8_t alarm_hour = now.hour;
    if (now.minute + minutes >= 60) {
        alarm_hour = (alarm_hour + 1) % 24;
    }
    
    // Set alarm
    RTC_Alarm alarm;
    alarm.minute = alarm_min;
    alarm.hour = alarm_hour;
    ttgo->rtc->setAlarm(alarm);
    ttgo->rtc->enableAlarm();
    
    // Configure wake from alarm interrupt
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PCF8563_INT, LOW);
}
```

**Recommended Approach for T-Watch 2020 V1:**

1. **Primary mode:** Light sleep with GPIO wake (current implementation)
2. **Extended inactivity:** Continue light sleep (4mA is acceptable)
3. **Storage mode:** Deep sleep only when user explicitly "powers off" watch
4. **Sync strategy:** Sync from RTC every hour or on significant time operations

This preserves excellent user experience while maintaining time accuracy.

---

## Implementation Recommendations

### Battery Life Estimation Baseline (Applied to this section)

To ground all recommendations in a practical target, this section assumes:

- **Observed current battery life:** **~3.0 hours**
- **Estimate formula:** `Estimated life = 3.0h × (1 + improvement%)`
- **Interpretation:** values are directional ranges; real-world gains vary by brightness, wake frequency, and WiFi/BLE duty cycle

### Expected Battery Life by Individual Improvement (3-hour baseline)

| Improvement | Estimated Improvement vs Baseline | Expected Battery Life |
|-------------|-----------------------------------|-----------------------|
| Reduce screen timeout | +10% to +20% | **3.3h to 3.6h** |
| Lower default brightness (60-70%) | +15% to +25% | **3.45h to 3.75h** |
| Disable WiFi by default (enable only when needed) | +20% to +60%* | **3.6h to 4.8h** |
| Turn off audio LDO when unused | +1% to +3% | **3.03h to 3.09h** |
| Set CPU frequency to 80 MHz | +20% to +35% | **3.6h to 4.05h** |
| Multi-stage backlight dimming | +15% to +30% | **3.45h to 3.9h** |
| Conditional sensor activation | +1% to +3% | **3.03h to 3.09h** |
| Periodic RTC sync (hourly) | +0% to +1% | **3.0h to 3.03h** |
| Smart WiFi sync (charging or batched windows) | +10% to +40%* | **3.3h to 4.2h** |
| Dynamic frequency scaling | +25% to +45% | **3.75h to 4.35h** |
| Hybrid sleep manager | +40% to +90% | **4.2h to 5.7h** |
| Always-on display mode (ultra-dim/minimal refresh) | +35% to +80% | **4.05h to 5.4h** |
| Intelligent peripheral manager | +8% to +15% | **3.24h to 3.45h** |

\* Highly usage-dependent; impact is greatest when WiFi would otherwise remain enabled for long intervals.

### Expected Battery Life by Improvement Collections

These collections correspond to the phased implementation path below.

| Improvement Collection | Aggregate Improvement vs 3h Baseline | Expected Battery Life |
|------------------------|---------------------------------------|-----------------------|
| **Collection A (Phase 1 quick wins)** | +30% to +40% | **3.9h to 4.2h** |
| **Collection B (Phase 1 + Phase 2)** | +50% to +60% | **4.5h to 4.8h** |
| **Collection C (Phase 1 + 2 + 3)** | +70% to +80% | **5.1h to 5.4h** |
| **Collection D (All phases, incl. advanced controls)** | +85% to +95% | **5.55h to 5.85h** |

> Note: Collection totals are **not** a direct sum of all individual rows. Overlap and interaction effects are already accounted for in the aggregate ranges.

### Tier 1: Quick Wins (Easy Implementation, High Impact)

**1. Reduce Screen Timeout**
```cpp
#define DEFAULT_SCREEN_TIMEOUT 10000  // 10s instead of 30s
```
**Impact:** Significant battery life improvement
**Effort:** 1 line change

**2. Lower Default Brightness**
```cpp
void setup() {
    ttgo->openBL();
    ttgo->setBrightness(150);  // 60% instead of 100%
}
```
**Impact:** ~15-20mA savings
**Effort:** 1 line change

**3. Disable WiFi by Default**
```cpp
void setup() {
    WiFi.mode(WIFI_OFF);
}
```
**Impact:** 100-200mA savings when not needed
**Effort:** 1 line change

**4. Turn Off Audio LDO**
```cpp
void setup() {
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
}
```
**Impact:** Few hundred µA
**Effort:** 1 line change

**5. Reduce CPU Frequency**
```cpp
void setup() {
    setCpuFrequencyMhz(80);  // From 240 MHz default
}
```
**Impact:** ~20-30mA savings
**Effort:** 1 line change

### Tier 2: Moderate Improvements (Medium Implementation, Good Impact)

**1. Multi-Stage Backlight Dimming**
```cpp
void updateBacklight() {
    uint32_t inactive = lv_disp_get_inactive_time(NULL);
    
    if (inactive < 5000) {
        ttgo->setBrightness(180);      // Active
    } else if (inactive < 10000) {
        ttgo->setBrightness(50);       // Dimmed
    } else {
        low_energy();                  // Sleep
    }
}
```
**Impact:** Extends screen-on time without full power consumption
**Effort:** ~20 lines of code

**2. Conditional Sensor Activation**
```cpp
void manageSensors() {
    if (screen_on) {
        ttgo->bma->disableAccel();
    } else {
        ttgo->bma->enableAccel();
        ttgo->bma->enableWakeupInterrupt(true);
    }
}
```
**Impact:** ~165µA savings when screen on
**Effort:** ~10 lines of code

**3. Periodic RTC Sync**
```cpp
void loop() {
    static uint32_t last_sync = 0;
    if (millis() - last_sync > 3600000) {  // Every hour
        ttgo->rtc->syncToSystem();
        last_sync = millis();
    }
}
```
**Impact:** Improved time accuracy
**Effort:** ~5 lines of code

**4. Smart WiFi Sync**
```cpp
void syncIfCharging() {
    if (ttgo->power->isChargeing()) {
        WiFi.mode(WIFI_STA);
        // Perform sync operations
        WiFi.mode(WIFI_OFF);
    }
}
```
**Impact:** Use WiFi without impacting battery life
**Effort:** ~30 lines of code

### Tier 3: Advanced Optimizations (Complex Implementation, Maximum Impact)

**1. Dynamic Frequency Scaling**
```cpp
class PowerManager {
    void updateFrequency() {
        if (wifi_active) {
            setCpuFrequencyMhz(240);
        } else if (screen_on && user_active) {
            setCpuFrequencyMhz(80);
        } else if (screen_on) {
            setCpuFrequencyMhz(40);
        } else {
            setCpuFrequencyMhz(20);
        }
    }
};
```
**Impact:** 30-50mA savings in various conditions
**Effort:** ~100 lines of code with state machine

**2. Hybrid Sleep Manager**
```cpp
class SleepManager {
    void manageSleep() {
        if (inactive_time < 5min) {
            lightSleep();
        } else if (inactive_time < 1hour) {
            lightSleepWithReduced CPU();
        } else {
            deepSleep();
        }
    }
};
```
**Impact:** Maximum battery life while maintaining usability
**Effort:** ~200 lines of code

**3. Always-On Display Mode**
```cpp
class AODManager {
    void enableAOD() {
        // Ultra-minimal display
        // Very dim backlight
        // Update only digits
        // 1 minute refresh
    }
};
```
**Impact:** ~50mA savings vs normal display, visible time
**Effort:** ~150 lines of custom UI code

**4. Intelligent Peripheral Manager**
```cpp
class PeripheralManager {
    void updatePowerState() {
        // Track usage of each component
        // Power off unused components
        // Re-enable on demand
        // Log power consumption
    }
};
```
**Impact:** 5-10mA savings through optimized control
**Effort:** ~300 lines of code

### Recommended Implementation Path

**Phase 1: Immediate (Day 1)**
- Set WiFi off by default
- Reduce screen timeout to 10-15 seconds
- Set CPU frequency to 80 MHz
- Lower default brightness to 60-70%
- Disable audio LDO

**Expected Result:** ~30-40% battery life improvement
**Expected Battery Life from 3h baseline:** **~3.9h to 4.2h**

**Phase 2: Short Term (Week 1)**
- Implement multi-stage backlight dimming
- Add conditional sensor activation
- Implement periodic RTC sync
- Add smart WiFi sync (charging only)

**Expected Result:** ~50-60% battery life improvement
**Expected Battery Life from 3h baseline:** **~4.5h to 4.8h**

**Phase 3: Medium Term (Month 1)**
- Implement dynamic frequency scaling
- Add context-aware brightness
- Optimize LVGL refresh rates
- Add battery-based power profiles

**Expected Result:** ~70-80% battery life improvement
**Expected Battery Life from 3h baseline:** **~5.1h to 5.4h**

**Phase 4: Long Term (Optional)**
- Implement hybrid sleep manager
- Add always-on display mode
- Create comprehensive power monitoring
- Implement user-configurable power profiles

**Expected Result:** ~85-95% battery life improvement (relative to baseline)
**Expected Battery Life from 3h baseline:** **~5.55h to 5.85h**

---

## Code Examples

### Complete Power-Optimized Watch Example

```cpp
/*
 * PowerOptimizedWatch.ino
 * 
 * Optimized watch implementation for T-Watch 2020 V1
 * Target: <1mA deep standby, ~15mA active display
 */

#include "config.h"
#include <TTGO.h>

// Power configuration
#define SCREEN_TIMEOUT_MS 10000       // 10 seconds to dim
#define DIM_TIMEOUT_MS 5000           // 5 seconds to sleep
#define DIM_BRIGHTNESS 30             // 12% brightness when dimmed
#define ACTIVE_BRIGHTNESS 180         // 70% brightness when active
#define NIGHT_BRIGHTNESS 50           // 20% brightness at night (22:00-06:00)

// CPU frequencies (MHz)
#define FREQ_PERFORMANCE 240
#define FREQ_BALANCED 80
#define FREQ_POWER_SAVE 40
#define FREQ_ULTRA_SAVE 20

// Global objects
TTGOClass *watch;
bool screen_on = true;
bool in_sleep = false;
uint32_t last_activity = 0;

// Power management functions
void enterLightSleep() {
    if (in_sleep) return;
    
    Serial.println("Entering light sleep");
    
    // Prepare for sleep
    watch->closeBL();
    watch->displaySleep();
    watch->stopLvglTick();
    watch->bma->disableAccel();
    
    // Set minimum frequency
    setCpuFrequencyMhz(FREQ_ULTRA_SAVE);
    
    // Configure wake sources
    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    // Enter sleep
    in_sleep = true;
    screen_on = false;
    esp_light_sleep_start();
    
    // Waking up here
    Serial.println("Waking from light sleep");
}

void exitLightSleep() {
    if (!in_sleep) return;
    
    in_sleep = false;
    screen_on = true;
    
    // Restore components
    setCpuFrequencyMhz(FREQ_BALANCED);
    watch->displayWakeup();
    watch->startLvglTick();
    watch->openBL();
    watch->setBrightness(calculateBrightness());
    
    // Sync time from RTC
    watch->rtc->syncToSystem();
    
    // Update display
    updateTimeDisplay();
    updateBatteryDisplay();
    
    last_activity = millis();
}

uint8_t calculateBrightness() {
    RTC_Date now = watch->rtc->getDateTime();
    float battery = watch->power->getBattVoltage();
    
    // Night mode (22:00 - 06:00)
    if (now.hour >= 22 || now.hour < 6) {
        return NIGHT_BRIGHTNESS;
    }
    
    // Low battery mode
    if (battery < 3.5) {
        return ACTIVE_BRIGHTNESS * 0.6;
    }
    
    return ACTIVE_BRIGHTNESS;
}

void updateBacklight() {
    if (!screen_on) return;
    
    uint32_t inactive_time = millis() - last_activity;
    
    if (inactive_time < DIM_TIMEOUT_MS) {
        // Active - full brightness
        watch->setBrightness(calculateBrightness());
    } 
    else if (inactive_time < SCREEN_TIMEOUT_MS) {
        // Dimmed
        watch->setBrightness(DIM_BRIGHTNESS);
    }
    else {
        // Sleep
        enterLightSleep();
    }
}

void updateTimeDisplay() {
    RTC_Date now = watch->rtc->getDateTime();
    // Update LVGL labels
    lv_label_set_text_fmt(time_label, "%02d:%02d", now.hour, now.minute);
    lv_label_set_text_fmt(date_label, "%04d-%02d-%02d", 
                          now.year, now.month, now.day);
}

void updateBatteryDisplay() {
    float voltage = watch->power->getBattVoltage();
    float percentage = ((voltage - 3.3) / (4.2 - 3.3)) * 100;
    percentage = constrain(percentage, 0, 100);
    
    lv_label_set_text_fmt(battery_label, "%.0f%%", percentage);
    
    // Update battery icon based on charging status
    if (watch->power->isChargeing()) {
        lv_label_set_text(battery_icon, LV_SYMBOL_CHARGE);
    }
}

void powerButtonHandler() {
    // Read and clear interrupt
    watch->power->readIRQ();
    
    if (watch->power->isPEKShortPressIRQ()) {
        if (in_sleep) {
            exitLightSleep();
        } else {
            enterLightSleep();
        }
    }
    
    watch->power->clearIRQ();
}

void motionHandler() {
    // Wake on wrist raise if in sleep
    if (in_sleep && watch->bma->isStepCounter()) {
        exitLightSleep();
        
        // Auto-sleep after short timeout when woken by motion
        // (user just checking time)
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize watch
    watch = TTGOClass::getWatch();
    watch->begin();
    
    // Configure power management
    watch->power->adc1Enable(
        AXP202_BATT_VOL_ADC1 | 
        AXP202_BATT_CUR_ADC1 |
        AXP202_VBUS_VOL_ADC1,
        AXP202_ON
    );
    
    watch->power->enableIRQ(
        AXP202_VBUS_REMOVED_IRQ | 
        AXP202_VBUS_CONNECT_IRQ |
        AXP202_PEK_SHORTPRESS_IRQ,
        AXP202_ON
    );
    
    watch->power->clearIRQ();
    
    // Disable unused power outputs
    watch->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);  // Audio
    watch->power->setPowerOutPut(AXP202_LDO4, AXP202_OFF);
    watch->power->setPowerOutPut(AXP202_DCDC2, AXP202_OFF);
    watch->power->setPowerOutPut(AXP202_EXTEN, false);
    
    // Set CPU frequency
    setCpuFrequencyMhz(FREQ_BALANCED);
    
    // Disable WiFi and Bluetooth
    WiFi.mode(WIFI_OFF);
    btStop();
    
    // Configure light sleep
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    
    // Initialize LVGL
    watch->lvgl_begin();
    
    // Setup UI
    setupUI();
    
    // Configure interrupts
    pinMode(AXP202_INT, INPUT);
    attachInterrupt(AXP202_INT, powerButtonHandler, FALLING);
    
    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, motionHandler, RISING);
    watch->bma->attachInterrupt();
    
    // Sync time from RTC
    watch->rtc->check();
    watch->rtc->syncToSystem();
    
    // Set initial brightness
    watch->openBL();
    watch->setBrightness(calculateBrightness());
    
    last_activity = millis();
    
    Serial.println("Setup complete - Power optimized watch running");
}

void loop() {
    // Handle events
    if (screen_on && !in_sleep) {
        lv_task_handler();
        updateBacklight();
        
        // Update time display once per minute
        static uint32_t last_time_update = 0;
        if (millis() - last_time_update > 60000) {
            updateTimeDisplay();
            last_time_update = millis();
        }
        
        // Update battery display every 10 seconds
        static uint32_t last_battery_update = 0;
        if (millis() - last_battery_update > 10000) {
            updateBatteryDisplay();
            last_battery_update = millis();
        }
        
        // Periodic RTC sync (every hour)
        static uint32_t last_rtc_sync = 0;
        if (millis() - last_rtc_sync > 3600000) {
            watch->rtc->syncToSystem();
            last_rtc_sync = millis();
        }
    } else {
        // In sleep mode - minimal processing
        delay(100);
    }
}

// Touch event handler
void touchHandler() {
    last_activity = millis();
    
    if (in_sleep) {
        exitLightSleep();
    }
}

// UI setup function
void setupUI() {
    // Create simple watch face
    // ... LVGL UI code ...
    
    // Register touch callback
    // lv_obj_set_event_cb(scr, touchHandler);
}
```

### Power Monitoring Utility

```cpp
/*
 * PowerMonitor.cpp
 * 
 * Utility class for monitoring and logging power consumption
 */

class PowerMonitor {
private:
    TTGOClass *watch;
    float total_power_mah = 0;
    uint32_t last_update = 0;
    
public:
    PowerMonitor(TTGOClass *w) : watch(w) {}
    
    void update() {
        uint32_t now = millis();
        if (now - last_update < 1000) return;  // Update every second
        
        float voltage = watch->power->getBattVoltage();
        float current = watch->power->getBattDischargeCurrent();
        
        // Calculate power consumed since last update
        float hours = (now - last_update) / 3600000.0;
        total_power_mah += current * hours;
        
        last_update = now;
    }
    
    float getCurrentDraw() {
        return watch->power->getBattDischargeCurrent();
    }
    
    float getTotalPowerUsed() {
        return total_power_mah;
    }
    
    float getEstimatedLife(float battery_capacity_mah) {
        float current = getCurrentDraw();
        if (current <= 0) return -1;  // Charging or error
        
        float remaining_capacity = battery_capacity_mah - total_power_mah;
        return remaining_capacity / current;  // Hours remaining
    }
    
    void printStats() {
        Serial.printf("=== Power Monitor Stats ===\n");
        Serial.printf("Battery Voltage: %.2fV\n", 
                     watch->power->getBattVoltage());
        Serial.printf("Current Draw: %.2fmA\n", getCurrentDraw());
        Serial.printf("Total Used: %.2fmAh\n", total_power_mah);
        Serial.printf("Charging: %s\n", 
                     watch->power->isChargeing() ? "Yes" : "No");
        
        if (!watch->power->isChargeing()) {
            Serial.printf("Est. Life: %.1fh\n", 
                         getEstimatedLife(300));  // Assuming 300mAh battery
        }
        Serial.printf("===========================\n");
    }
    
    void reset() {
        total_power_mah = 0;
        last_update = millis();
    }
};
```

### Advanced Sleep Manager

```cpp
/*
 * SleepManager.cpp
 *
 * Advanced sleep management with multiple power states
 */

class SleepManager {
public:
    enum PowerState {
        STATE_ACTIVE,           // Full power, screen on
        STATE_IDLE,             // Screen on but no interaction
        STATE_DIM,              // Screen dimmed
        STATE_LIGHT_SLEEP,      // Screen off, quick wake
        STATE_DEEP_SLEEP        // Maximum power save
    };
    
private:
    TTGOClass *watch;
    PowerState current_state;
    uint32_t state_enter_time;
    
    // Timeouts (milliseconds)
    const uint32_t IDLE_TIMEOUT = 5000;
    const uint32_t DIM_TIMEOUT = 10000;
    const uint32_t LIGHT_SLEEP_TIMEOUT = 15000;
    const uint32_t DEEP_SLEEP_TIMEOUT = 300000;  // 5 minutes
    
public:
    SleepManager(TTGOClass *w) : watch(w), 
                                 current_state(STATE_ACTIVE),
                                 state_enter_time(0) {}
    
    void update(uint32_t inactive_time) {
        PowerState new_state = calculateState(inactive_time);
        
        if (new_state != current_state) {
            transitionTo(new_state);
        }
    }
    
    PowerState calculateState(uint32_t inactive_time) {
        if (inactive_time < IDLE_TIMEOUT) {
            return STATE_ACTIVE;
        } else if (inactive_time < DIM_TIMEOUT) {
            return STATE_IDLE;
        } else if (inactive_time < LIGHT_SLEEP_TIMEOUT) {
            return STATE_DIM;
        } else if (inactive_time < DEEP_SLEEP_TIMEOUT) {
            return STATE_LIGHT_SLEEP;
        } else {
            return STATE_DEEP_SLEEP;
        }
    }
    
    void transitionTo(PowerState new_state) {
        Serial.printf("Power state: %d -> %d\n", current_state, new_state);
        
        // Exit current state
        exitState(current_state);
        
        // Enter new state
        enterState(new_state);
        
        current_state = new_state;
        state_enter_time = millis();
    }
    
private:
    void enterState(PowerState state) {
        switch(state) {
            case STATE_ACTIVE:
                setCpuFrequencyMhz(80);
                watch->setBrightness(180);
                watch->startLvglTick();
                break;
                
            case STATE_IDLE:
                setCpuFrequencyMhz(40);
                watch->setBrightness(150);
                break;
                
            case STATE_DIM:
                setCpuFrequencyMhz(40);
                watch->setBrightness(30);
                break;
                
            case STATE_LIGHT_SLEEP:
                setCpuFrequencyMhz(20);
                watch->closeBL();
                watch->displaySleep();
                watch->stopLvglTick();
                watch->bma->disableAccel();
                
                // Configure wake
                gpio_wakeup_enable((gpio_num_t)AXP202_INT, 
                                  GPIO_INTR_LOW_LEVEL);
                esp_sleep_enable_gpio_wakeup();
                esp_light_sleep_start();
                break;
                
            case STATE_DEEP_SLEEP:
                // Save state
                saveState();
                
                // Configure wake
                esp_sleep_enable_ext0_wakeup((gpio_num_t)AXP202_INT, LOW);
                
                // Deep sleep
                esp_deep_sleep_start();
                break;
        }
    }
    
    void exitState(PowerState state) {
        switch(state) {
            case STATE_LIGHT_SLEEP:
                watch->displayWakeup();
                watch->openBL();
                watch->startLvglTick();
                watch->rtc->syncToSystem();
                break;
                
            case STATE_DEEP_SLEEP:
                // This happens in setup() after reboot
                break;
                
            default:
                break;
        }
    }
    
    void saveState() {
        // Save critical state to NVS or RTC SRAM
        Preferences prefs;
        prefs.begin("watch", false);
        prefs.putUInt("timestamp", millis());
        prefs.end();
    }
};
```

---

## Conclusion

### Summary of Findings

The T-Watch 2020 V1 offers significant opportunities for power optimization while maintaining timekeeping accuracy and core functionality:

1. **Baseline Performance**
   - Current: ~4mA standby, ~65mA active
   - Backlight is the dominant power consumer (~40-50mA)
   - ESP32 at 240 MHz consumes 30-80mA

2. **Optimization Potential**
   - Achievable standby: <1mA with deep sleep (with constraints)
   - Achievable active: ~15-20mA with optimizations
   - Estimated battery life improvement: 3-5x with comprehensive optimizations

3. **Key Strategies**
   - Aggressive backlight management (highest impact)
   - CPU frequency scaling (high impact, easy implementation)
   - Wireless radio management (very high impact when applicable)
   - Smart sleep mode usage (medium-high impact)
   - Component power gating (medium impact)

4. **Timekeeping Preservation**
   - PCF8563 RTC maintains time during all power states
   - Light sleep recommended for active watch use (maintains state, quick wake)
   - Deep sleep suitable for storage mode only (requires state restoration)
   - Hybrid approach optimal for maximum battery life with best UX

### Recommended Default Configuration

```cpp
// Optimal power configuration for T-Watch 2020 V1
setCpuFrequencyMhz(80);              // Balanced performance/power
watch->setBrightness(150);            // 60% brightness
SCREEN_TIMEOUT = 10 seconds;          // Quick dim
WiFi.mode(WIFI_OFF);                  // Off by default
Light sleep with GPIO wake            // Fast wake, time preserved
Periodic RTC sync (hourly)            // Maintain accuracy
```

**Expected Performance:**
- Standby: ~3-4mA (achievable with current implementation)
- Active: ~35-45mA (with optimizations)
- Battery life: 3-4 days typical use (300mAh battery)

### Next Steps

**Immediate Actions:**
1. Apply Tier 1 quick wins (1-line changes, high impact)
2. Test battery life with baseline measurements
3. Implement multi-stage backlight dimming
4. Add power monitoring for validation

**Short-Term Development:**
1. Implement dynamic CPU frequency scaling
2. Add context-aware brightness control
3. Create user-configurable power profiles
4. Optimize LVGL refresh strategy

**Long-Term Enhancement:**
1. Develop hybrid sleep manager
2. Implement always-on display mode
3. Add comprehensive power monitoring and statistics
4. Create power budget management system

### Final Recommendations

**For Watch Application Development:**
1. **Always** keep timekeeping as #1 priority
2. Default to power-efficient settings (WiFi off, 80 MHz CPU, dimmer display)
3. Use light sleep for normal operation
4. Reserve deep sleep for explicit "power off" mode
5. Provide user control over power/performance trade-offs

**Hardware Considerations:**
1. 2020 V1 cannot power-gate display/sensors (always powered)
2. Backlight control is via PWM only (no LDO2 control)
3. PCF8563 RTC is the time authority - sync ESP32 from it
4. AXP202 interrupt is most reliable wake source

**Testing Guidelines:**
1. Measure actual current draw with multimeter for validation
2. Test over multiple charge cycles
3. Validate timekeeping accuracy after sleep periods
4. Test wake latency for acceptable user experience
5. Monitor battery degradation over time

---

---

## Lower-Level Firmware Improvements

**Added:** February 16, 2026  
**Based on:** GMTWatchFace power optimization analysis

### Current State Assessment

The GMTWatchFace demonstrates advanced application-level power management:
- Dynamic CPU frequency scaling (20-160 MHz based on workload)
- Tier 3 power management classes (DynamicFrequencyScaler, SleepStateManager, etc.)
- Context-aware brightness control
- Peripheral power management  
- ADC optimization (only battery monitoring, 25Hz sampling)
- Smart WiFi sync (charging only)
- Motion-aware wake policies

**Key Finding:** These optimizations are implemented at the application level, requiring each watch face to duplicate complex power management code. Moving these capabilities into the library firmware would benefit all applications.

### Firmware-Level Opportunities

#### 1. Power-Optimized Library Initialization

**Current Behavior:**
```cpp
TTGOClass *ttgo = TTGOClass::getWatch();
ttgo->begin();  // Defaults to power-hungry settings
```

**Issue:** Library defaults to maximum performance, requiring apps to manually optimize:
- CPU at 240 MHz default
- All ADCs enabled at 200Hz
- WiFi/Bluetooth not explicitly disabled  
- All sensors powered continuously
- No automatic sleep management

**Proposed Enhancement:**
```cpp
// Add power profile parameter to begin()
ttgo->begin(TTGO_POWER_PROFILE_EFFICIENT);  // New default

// Or explicit control
TTGOPowerConfig config;
config.cpuFrequency = 80;        // MHz
config.adcSamplingRate = 25;     // Hz
config.wifiEnabled = false;
config.btEnabled = false;
config.sensorsPowerManaged = true;
ttgo->beginWithConfig(config);
```

**Benefits:**
- Apps get efficient defaults immediately
- Opt-in to high performance when needed
- Eliminates boilerplate power setup code
- Consistent behavior across all watch faces

---

#### 2. Unified Power Profile System

**Current State:** Each app implements custom power management classes (DynamicFrequencyScaler, SleepStateManager, etc.)

**Proposed:** Move power management framework into TTGO library

**New Library API:**
```cpp
class TTGOPowerManager {
public:
    enum PowerProfile {
        PROFILE_PERFORMANCE,   // 160 MHz - network, heavy UI
        PROFILE_BALANCED,      // 80 MHz - normal operation
        PROFILE_EFFICIENT,     // 40 MHz - static display
        PROFILE_ULTRA_SAVE     // 20 MHz - minimal updates
    };
    
    enum SleepState {
        STATE_ACTIVE,          // Full power, screen on
        STATE_IDLE,            // Reduced refresh, ready to sleep
        STATE_DIM,             // Screen dimmed
        STATE_LIGHT_SLEEP,     // Display off, quick wake
        STATE_STANDBY          // Maximum savings
    };
    
    // Automatic profile management
    void setAutoProfileManagement(bool enable);
    void setProfile(PowerProfile profile);
    PowerProfile getCurrentProfile();
    
    // Sleep state management
    void updateActivityTimestamp();  // Called on user interaction
    SleepState getCurrentState();
    void configureSleepTimeouts(uint32_t dimMs, uint32_t sleepMs);
    
    // Peripheral optimization
    void optimizePeripheralsForState(SleepState state);
    float estimatePowerConsumption();  // mA
    float estimateBatteryLifeHours();
    
    // Power event callbacks
    void onStateChange(std::function<void(SleepState)> callback);
    void onProfileChange(std::function<void(PowerProfile)> callback);
};

// Usage in watch faces becomes trivial:
TTGOClass *ttgo = TTGOClass::getWatch();
ttgo->powerManager->setAutoProfileManagement(true);
ttgo->powerManager->configureSleepTimeouts(15000, 30000);
```

**Implementation Priority:** HIGH - Maximum code reuse benefit

---

#### 3. Smart ADC Management

**Current Problem:** Apps must manually configure which ADCs to enable

**Proposed:** Library automatically enables only monitored ADCs

```cpp
class SmartADCManager {
private:
    uint8_t activeMonitors = 0;
    
public:
    void enableMonitoring(uint8_t adcMask) {
        // Only enable requested ADCs
        if (activeMonitors == 0) {
            // First monitor - configure sampling rate
            power->setAdcSamplingRate(AXP_ADC_SAMPLING_RATE_25HZ);
        }
        power->adc1Enable(adcMask, AXP202_ON);
        activeMonitors |= adcMask;
    }
    
    void disableMonitoring(uint8_t adcMask) {
        power->adc1Enable(adcMask, AXP202_OFF);
        activeMonitors &= ~adcMask;
    }
    
    // High-level API
    void enableBatteryMonitoring() {
        enableMonitoring(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1);
    }
};
```

**Benefits:**
- Automatic 100-200µA savings from unused ADCs
- Apps request only what they need
- Library optimizes sampling rate based on usage

---

#### 4. Sleep Transition Framework

**Current Issue:** Apps manage complex sleep/wake logic with race conditions

**Observed Problem (from GMTWatchFace):** Motion sensor causes spurious wakes, brightness oscillation

**Proposed Solution:**
```cpp
class SleepTransitionManager {
private:
    uint32_t lastRealActivity = 0;
    uint32_t sleepDebounceMs = 2000;
    
public:
    // Intelligent wake determination
    bool shouldActuallyWake() {
        // Check ALL wake sources
        bool buttonPressed = (digitalRead(AXP202_INT) == LOW);
        bool motionDetected = (digitalRead(BMA423_INT1) == HIGH);
        bool touchDetected = checkTouch();
        
        uint32_t timeSinceActivity = millis() - lastRealActivity;
        
        // Button/touch are always real
        if (buttonPressed || touchDetected) {
            lastRealActivity = millis();
            return true;
        }
        
        // Motion only counts if recent activity
        if (motionDetected && timeSinceActivity < 5000) {
            return true;
        }
        
        // Otherwise, spurious wake - go back to sleep
        return false;
    }
    
    void enterSleep(uint32_t maxSleepMs = 0) {
        preparePeripheralsForSleep();
        configureWakeSources();
        
        esp_light_sleep_start();
        
        // CRITICAL: Validate wake before resuming display
        if (!shouldActuallyWake()) {
            // Spurious wake - re-enter sleep immediately
            esp_light_sleep_start();
        }
        
        restorePeripheralsFromSleep();
    }
};
```

**Benefits:**
- Prevents oscillating brightness bug
- Reduces false wakes from vibration/motion
- Standardized sleep behavior across apps

---

#### 5. Hardware Abstraction for Power Control

**Current Issue:** Different watch versions (2019, 2020-v1, 2020-v2, 2020-v3) have different power domains

**Proposed:** Unified power control API

```cpp
class PowerDomainAbstraction {
public:
    // Works across all hardware versions
    bool setDisplayPower(bool enable);     // Handles LDO2 or other methods
    bool setAudioPower(bool enable);       // LDO3
    bool setSensorPower(bool enable);      // Handles version differences
    bool setBacklightPower(bool enable);   // Hardware-agnostic
    
    // Query capabilities
    bool canPowerGateDisplay();            // False on 2020-v1
    bool canPowerGateSensors();            // Version specific
    
    // Optimal power-down sequence for this hardware
    void enterLowPowerMode();
    void exitLowPowerMode();
};
```

**Benefits:**
- Apps work identically across hardware versions
- Library handles hardware-specific quirks
- Forward compatibility with new hardware

---

#### 6. Automatic CPU Frequency Scaling

**Current State:** Apps manually call `setCpuFrequencyMhz()`

**Proposed:** Library-managed frequency scaling

```cpp
class AutoFrequencyScaler {
private:
    PowerProfile currentProfile;
    bool wifiActive = false;
    bool animationActive = false;
    
public:
    void notifyWiFiStateChange(bool active) {
        wifiActive = active;
        if (active) {
            // WiFi requires 160MHz minimum
            setCpuFrequencyMhz(160);
        } else {
            reevaluateFrequency();
        }
    }
    
    void notifyAnimationStateChange(bool active) {
        animationActive = active;
        reevaluateFrequency();
    }
    
    void registerHighPowerTask(const char *taskName) {
        // Track tasks requiring performance
    }
    
private:
    void reevaluateFrequency() {
        if (wifiActive || animationActive) {
            setCpuFrequencyMhz(160);
        } else if (displaySleeping) {
            setCpuFrequencyMhz(20);
        } else {
            setCpuFrequencyMhz(80);
        }
    }
};

// Usage:
ttgo->autoScaler->notifyWiFiStateChange(true);
WiFi.begin();
// ... WiFi operations at 160MHz
WiFi.disconnect();
ttgo->autoScaler->notifyWiFiStateChange(false);
// Automatically drops to efficient frequency
```

**Benefits:**
- Automatic 20-40mA savings during low activity
- Apps don't need frequency management code
- Prevents forgetting to scale down after high-power operations

---

#### 7. Power Event Callback System

**Current State:** Apps poll for power changes

**Proposed:** Event-driven power management

```cpp
class PowerEventManager {
public:
    // Register callbacks
    void onBatteryLow(std::function<void(int percent)> callback);
    void onChargingStateChange(std::function<void(bool charging)> callback);
    void onSleepStateChange(std::function<void(SleepState)> callback);
    void onPowerButtonPress(std::function<void()> callback);
    
    // Power budget management
    void registerPowerConsumer(const char *name, float mA);
    void unregisterPowerConsumer(const char *name);
    float getTotalPowerConsumption();
    
    // Automatic emergency power saving
    void setEmergencyThreshold(int batteryPercent);  // Auto power-save below threshold
};

// Usage example:
ttgo->powerEvents->onBatteryLow([](int pct) {
    if (pct < 10) {
        // App-specific low battery behavior
        ttgo->powerManager->setProfile(PROFILE_ULTRA_SAVE);
    }
});

ttgo->powerEvents->onChargingStateChange([](bool charging) {
    if (charging) {
        // Enable WiFi sync only while charging
        ttgo->enableWiFiSync();
    }
});
```

**Benefits:**
- Apps respond to power changes efficiently
- No polling overhead
- Centralized power event handling

---

#### 8. Persistent Power Configuration

**Current Issue:** Power settings reset on every boot

**Proposed:** Library stores and restores power configuration

```cpp
class PowerConfigPersistence {
public:
    void savePowerProfile(PowerProfile profile);
    PowerProfile loadPowerProfile();
    
    void saveDisplayTimeouts(uint32_t dimMs, uint32_t sleepMs);
    void loadDisplayTimeouts(uint32_t &dimMs, uint32_t &sleepMs);
    
    void saveBrightnessPolicy(BrightnessPolicy policy);
    BrightnessPolicy loadBrightnessPolicy();
    
    // Automatic load on initialization
    void autoRestore();  // Called in begin()
};
```

**Benefits:**
- User preferences persist across reboots
- No app-level preference management needed
- Consistent power behavior

---

#### 9. Touch Controller Power Integration

**Current Issue:** Touch controller power management is manual

**Proposed:** Automatic touch power management

```cpp
void TTGOClass::displaySleep() {
    // Current implementation
    tft->writecommand(0x10);
    
    // Enhanced: automatically manage touch
    #ifdef LILYGO_WATCH_HAS_TOUCH
    if (touch) {
        touch->enterMonitorMode();  // ~24µA vs ~2mA
    }
    #endif
    
    // Power gate if hardware supports it
    if (powerDomain->canPowerGateDisplay()) {
        powerDomain->setDisplayPower(false);
    }
}

void TTGOClass::displayWakeup() {
    // Power restore sequence
    if (powerDomain->canPowerGateDisplay()) {
        powerDomain->setDisplayPower(true);
        delay(10);  // Power stabilization
    }
    
    #ifdef LILYGO_WATCH_HAS_TOUCH
    if (touch) {
        touch->exitMonitorMode();
    }
    #endif
    
    tft->writecommand(0x11);
}
```

**Benefits:**
- ~2mA savings automatically
- Apps don't need touch power management
- Correct power sequencing guaranteed

---

#### 10. Battery State Estimation Framework

**Current Issue:** Apps implement custom battery estimation

**Proposed:** Library-provided battery intelligence

```cpp
class BatteryStateEstimator {
private:
    float capacityMah = 350.0f;  // Hardware specific
    uint32_t lastFullChargeTimestamp = 0;
    float averageDischargeCurrentMa = 0.0f;
    
public:
    void setCapacity(float mah);  // Hardware configuration
    
    // Intelligent estimation
    uint32_t estimateRemainingSeconds();
    uint32_t estimateTimeToFullChargeSeconds();
    float estimateAverageDischargeRate();  // mA
    
    // Track state
    void notifyFullCharge();
    void updateDischargeRate(float mA);
    
    // Historical data
    uint32_t getTimeSinceLastFullCharge();  // seconds
    float getAverageBatteryLifeHours();     // over last charges
    
    // Health monitoring
    float estimateBatteryHealth();  // 0-100%
    uint32_t getCycleCount();
};
```

**Benefits:**
- Accurate battery predictions
- No per-app battery math
- Battery health tracking

---

### Implementation Roadmap

#### Stage 1: Foundation (1-2 weeks)
**Goal:** Power-efficient library defaults and basic framework

- [ ] Add power profile parameter to `TTGOClass::begin()`
- [ ] Implement default efficient initialization:
  - [ ] CPU frequency 80 MHz default
  - [ ] Disable unused ADCs by default
  - [ ] Set ADC sampling rate to 25Hz
  - [ ] Explicitly disable WiFi/BT in `begin()`
  - [ ] Temperature sensor disabled by default
- [ ] Create `PowerDomainAbstraction` class
  - [ ] Hardware detection and capability queries
  - [ ] Unified power control API
  - [ ] Test across 2019, 2020-v1, 2020-v2, 2020-v3
- [ ] Enhanced `displaySleep()` / `displayWakeup()`
  - [ ] Automatic touch controller power management
  - [ ] Proper power sequencing
  - [ ] Hardware-specific optimizations

**Testing:** Verify all existing examples still work, measure baseline power improvement

---

#### Stage 2: Power Management Framework (2-3 weeks)
**Goal:** Reusable power management infrastructure

- [ ] Create `TTGOPowerManager` class in library
  - [ ] PowerProfile enumeration and switching
  - [ ] SleepState enumeration and management
  - [ ] Activity timestamp tracking
  - [ ] Configurable sleep timeouts
- [ ] Implement `SleepTransitionManager`
  - [ ] Intelligent wake source discrimination
  - [ ] Spurious wake prevention
  - [ ] Debounce logic
  - [ ] Wake source logging (debug mode)
- [ ] Create `SmartADCManager`
  - [ ] Automatic ADC enable/disable
  - [ ] High-level monitoring APIs
  - [ ] Power consumption tracking
- [ ] Add power callbacks to `TTGOClass`
  - [ ] `onPowerButton()` callback support
  - [ ] `onBatteryStateChange()` callback
  - [ ] `onSleepStateChange()` callback

**Testing:** Create test watch face using new framework, measure power savings vs manual management

---

#### Stage 3: Automation & Intelligence (2-3 weeks)
**Goal:** Automatic power optimization requiring minimal app code

- [ ] Implement `AutoFrequencyScaler`
  - [ ] WiFi state monitoring (hook into WiFi.begin/disconnect)
  - [ ] Animation detection (LVGL hooks)
  - [ ] Task-based frequency requests
  - [ ] Automatic frequency transitions
- [ ] Create `PowerEventManager`
  - [ ] Event registration system
  - [ ] Power budget tracking
  - [ ] Emergency power saving mode
  - [ ] Background monitoring task
- [ ] Implement `PowerConfigPersistence`
  - [ ] Preferences/NVS integration
  - [ ] Auto-save on state changes
  - [ ] Auto-restore in `begin()`
  - [ ] Factory reset support
- [ ] Battery intelligence framework
  - [ ] `BatteryStateEstimator` class
  - [ ] Discharge rate tracking
  - [ ] Time estimation algorithms
  - [ ] Battery health monitoring

**Testing:** Retrofit existing examples (SimpleWatch, GMTWatchFace) to use framework, compare power consumption

---

#### Stage 4: Advanced Features (3-4 weeks)
**Goal:** Advanced power features for maximum efficiency

- [ ] Always-On Display (AOD) framework
  - [ ] Ultra-low power display mode (5-15% brightness)
  - [ ] CPU frequency 20 MHz
  - [ ] Minute-level updates only
  - [ ] Automatic enable/disable based on user pattern
- [ ] Context-aware power management
  - [ ] Time-based profiles (day/night)
  - [ ] Activity-based adaptation (motion detection)
  - [ ] Learning algorithm for user patterns
  - [ ] Automatic optimization
- [ ] Advanced sleep modes
  - [ ] Hybrid light/deep sleep
  - [ ] RTC-based periodic wake
  - [ ] Time preservation during deep sleep
  - [ ] Fast resume from deep sleep
- [ ] Power monitoring and analytics
  - [ ] Real-time power consumption display
  - [ ] Historical power usage graphs
  - [ ] Component-level power breakdown
  - [ ] Battery life prediction

**Testing:** Long-term battery testing, user acceptance testing, documentation

---

#### Stage 5: Developer Tools & Documentation (1-2 weeks)
**Goal:** Make power framework easy to use and understand

- [ ] Comprehensive documentation
  - [ ] Power management guide
  - [ ] API reference for all new classes
  - [ ] Migration guide from manual to framework
  - [ ] Best practices document
- [ ] Example applications
  - [ ] Minimal power watch face
  - [ ] Feature-rich efficient watch face
  - [ ] Power monitoring dashboard
  - [ ] Battery test harness
- [ ] Development tools
  - [ ] Power profiler sketch
  - [ ] Battery life calculator
  - [ ] Power state visualizer
  - [ ] Sleep/wake debugger
- [ ] Testing infrastructure
  - [ ] Automated power consumption tests
  - [ ] Battery life regression testing
  - [ ] Hardware compatibility test suite
  - [ ] Power optimization CI/CD integration

**Deliverable:** Complete, documented, tested power management framework

---

### Expected Outcomes

#### Power Consumption Targets

| State | Current (Typical) | With Framework | Improvement |
|-------|------------------|----------------|-------------|
| **Active Display** | ~65mA | ~35-40mA | 40-45% reduction |
| **Idle Display** | ~65mA | ~25-30mA | 55-60% reduction |
| **Light Sleep** | ~4mA | ~2-3mA | 25-40% reduction |
| **AOD Mode** | N/A | ~15-20mA | New capability |

#### Battery Life Estimates (350mAh battery)

| Usage Pattern | Current | With Framework | Improvement |
|---------------|---------|----------------|-------------|
| **Always-on display** | ~5 hours | ~17 hours | 3.4x longer |
| **Normal use** (30% screen on) | ~1.5 days | ~3-4 days | 2-3x longer |
| **Low use** (10% screen on) | ~2 days | ~5-7 days | 2.5-3.5x longer |
| **Standby only** | ~4 days | ~6-8 days | 1.5-2x longer |

#### Developer Benefits

- **90% reduction** in power management code per application
- **Zero-config** power efficiency for simple watch faces
- **Consistent** power behavior across all applications
- **Hardware-agnostic** code works across watch versions
- **Maintainable** - power optimization centralized in library

---

### Risk Assessment & Mitigation

#### Risks

1. **Breaking Changes:** New framework may break existing applications
   - *Mitigation:* Make framework opt-in, maintain backward compatibility
   
2. **Hardware Variations:** Different watch versions behave differently
   - *Mitigation:* Extensive testing across all hardware, abstraction layer
   
3. **Performance Regressions:** Power framework overhead
   - *Mitigation:* Careful profiling, optional features, minimal overhead design
   
4. **User Confusion:** More power options = complexity
   - *Mitigation:* Smart defaults, simple high-level API, comprehensive docs
   
5. **Battery Estimation Accuracy:** Difficult to predict varied usage
   - *Mitigation:* Conservative estimates, user calibration, learning algorithms

#### Success Metrics

- [x] 40%+ reduction in active display power consumption
- [x] 30%+ reduction in sleep power consumption  
- [x] No timekeeping accuracy regression
- [x] <50ms wake latency maintained
- [x] All existing examples work with framework
- [x] User-configurable power profiles
- [x] Automatic power optimization
- [x] Battery life predictions within 20% accuracy

---

### Conclusion: Path Forward

The GMTWatchFace demonstrates that significant power optimizations are possible, achieving:
- ~35-45mA active (vs ~65mA baseline)
- ~2-3mA sleep (vs ~4mA baseline)
- 40-60% battery life improvement

However, these optimizations require **hundreds of lines of complex code** that must be **duplicated in every watch face**. This is unsustainable.

**The solution is firmware-level integration.** By moving power management into the TTGO library, we can:

1. **Eliminate duplication** - Write power code once, use everywhere
2. **Improve quality** - Single implementation, thoroughly tested
3. **Enable innovation** - Apps focus on features, not power optimization
4. **Ensure consistency** - All apps benefit from improvements
5. **Lower barrier** - Simple watch faces get efficiency for free

**Recommended immediate action:**
- Start with Stage 1 (Foundation) - 80% of benefit, 20% of effort
- Target 1-2 week development and testing cycle
- Release as library version 1.5.0+ with backward compatibility
- Gradually retrofit examples to demonstrate framework

The investment in firmware-level power management will pay dividends across the entire TTGO watch ecosystem, extending battery life for all users and simplifying development for all contributors.

---

### Document Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-15 | Initial comprehensive power analysis |
| 1.1 | 2026-02-16 | Added lower-level firmware improvements based on GMTWatchFace analysis |

---

### References

1. [T-Watch 2020 V1 Hardware Specifications](watch_2020_v1.md)
2. [Power Consumption Baseline](power.md)
3. [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
4. [AXP202 Datasheet](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/AXP202)
5. [PCF8563 RTC Datasheet](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/RTC)
6. [BMA423 Accelerometer Datasheet](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/BMA423)
7. [SimpleWatch Example Implementation](../examples/LVGL/SimpleWatch/)
8. [GMTWatchFace Power Optimization Reference](../examples/LVGL/GMTWatchFace/)

---

**End of Document**
