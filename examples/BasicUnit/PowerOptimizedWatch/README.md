# Power Optimized Watch Example

This example demonstrates the integrated power management framework that is now built into the TTGO Watch Library.

## Features

### Automatic Power Optimization (Zero Configuration)
When you call `watch->begin()`, the library now automatically:
- ✅ Sets CPU to 80MHz (balanced mode vs 240MHz default)
- ✅ Enables only necessary ADC channels (saves ~100-200µA)
- ✅ Uses 25Hz ADC sampling rate (saves ~50-100µA)
- ✅ Disables temperature sensor (saves ~10µA)
- ✅ Configures power-efficient sleep timeouts
- ✅ Implements automatic touch power management in sleep

### Power Consumption Improvements
| State | Before | After | Improvement |
|-------|--------|-------|-------------|
| **Active Display** | ~65mA | ~35-40mA | **40-45%** |
| **Light Sleep** | ~4mA | ~2-3mA | **25-40%** |
| **Battery Life** | ~1.5 days | ~3-4 days | **2-3x** |

## Available Power Management Features

### 1. Power Manager (`watch->getPowerManager()`)
Controls power profiles and sleep states.

```cpp
auto powerMgr = watch->getPowerManager();
powerMgr->configureSleepTimeouts(10000, 30000);  // 10s dim, 30s sleep
powerMgr->setProfile(Profile::BALANCED);  // PERFORMANCE, BALANCED, EFFICIENT, ULTRA_SAVE
```

### 2. Frequency Scaler (`watch->getFrequencyScaler()`)
Automatic CPU frequency adjustment.

```cpp
auto freqScaler = watch->getFrequencyScaler();
freqScaler->notifyWiFiStateChange(true);   // Auto boost to 160MHz
// ... WiFi operations ...
freqScaler->notifyWiFiStateChange(false);  // Auto return to efficient
```

### 3. Event Manager (`watch->getEventManager()`)
Power event callbacks.

```cpp
auto eventMgr = watch->getEventManager();
eventMgr->onBatteryLow([](int percent) {
    Serial.printf("Battery: %d%%\n", percent);
});
eventMgr->onChargingStateChange([](bool charging) {
    Serial.printf("Charging: %s\n", charging ? "Yes" : "No");
});
```

### 4. Battery Estimator (`watch->getBatteryEstimator()`)
Intelligent battery predictions.

```cpp
auto batteryEst = watch->getBatteryEstimator();
batteryEst->setCapacity(350.0f);  // 350mAh battery
uint32_t secondsLeft = batteryEst->estimateRemainingSeconds(currentMa, batteryPercent);
```

## Power Profiles

| Profile | CPU Speed | Use Case | Power Draw |
|---------|-----------|----------|------------|
| **PERFORMANCE** | 240 MHz | WiFi, heavy tasks | ~80mA |
| **BALANCED** | 80 MHz | Normal operation (default) | ~30mA |
| **EFFICIENT** | 40 MHz | Static display | ~15mA |
| **ULTRA_SAVE** | 20 MHz | Minimal updates | ~5mA |

## Sleep States

Automatic transitions based on inactivity:
1. **ACTIVE** - Full power, screen on
2. **IDLE** - Reduced refresh, ready to sleep
3. **DIM** - Screen dimmed (after 5s default)
4. **LIGHT_SLEEP** - Display off, quick wake (after 15s default)
5. **STANDBY** - Maximum savings

## Migration Guide

### Before (Manual Power Management)
```cpp
void setup() {
    watch = TTGOClass::getWatch();
    watch->begin();
    
    // Manual optimizations required
    setCpuFrequencyMhz(80);
    watch->power->adc1Enable(AXP202_BATT_VOL_ADC1, true);
    watch->power->setAdcSamplingRate(AXP_ADC_SAMPLING_RATE_25HZ);
    WiFi.mode(WIFI_OFF);
    // ... many more manual settings ...
}
```

### After (Automatic Power Management)
```cpp
void setup() {
    watch = TTGOClass::getWatch();
    watch->begin();  // Power optimization automatic!
    
    // Optional: customize if needed
    auto powerMgr = watch->getPowerManager();
    powerMgr->configureSleepTimeouts(10000, 30000);
}
```

## Hardware Requirements

- LILYGO T-Watch 2020 V1, V2, or V3
- Works across all versions automatically

## Expected Results

With default settings:
- **Active display power:** ~35-40mA (vs ~65mA before)
- **Sleep power:** ~2-3mA (vs ~4mA before)
- **Battery life:** 2-3x longer for typical usage
- **No code changes needed** - optimization is automatic!

## Further Reading

- [Firmware Implementation Summary](../../../docs/firmware_improvements_implementation_summary.md)
- [Power Analysis Document](../../../docs/power_analysis_2020v1.md)
- [Test Coverage Verification](../../../docs/test_coverage_verification.md)

## Author

TTGO Watch Library - Power Management Framework
Based on power_analysis_2020v1.md - Lower-Level Firmware Improvements
February 17, 2026
