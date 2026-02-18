/*
 * PowerOptimizedWatch - Example using integrated power management framework
 * 
 * This example demonstrates the power-optimized features now built into the
 * TTGO library through the firmware improvements integration.
 * 
 * Features demonstrated:
 * - Automatic CPU frequency scaling
 * - Smart ADC management
 * - Power event callbacks
 * - Battery state estimation
 * - Automatic sleep state management
 * 
 * Expected power consumption:
 * - Active display: ~35-40mA (vs ~65mA before optimization)
 * - Light sleep: ~2-3mA (vs ~4mA before)
 * - Battery life: 2-3x improvement for typical usage
 * 
 * Hardware: LILYGO T-Watch 2020 V1/V2/V3
 * 
 * Created: February 17, 2026
 * Based on: power_analysis_2020v1.md - Lower-Level Firmware Improvements
 */

#include "config.h"
#include <TTGO.h>

TTGOClass *watch;

// Power management is now integrated into TTGOClass
// Access via watch->getPowerManager(), watch->getFrequencyScaler(), etc.

void setup() {
    Serial.begin(115200);
    Serial.println("Power-Optimized Watch Example");
    Serial.println("Firmware improvements integrated!");
    
    // Initialize watch - power optimization is automatic!
    watch = TTGOClass::getWatch();
    watch->begin();  // Now includes power optimization by default
    
    // The library now automatically:
    // - Sets CPU to 80MHz (balanced mode)
    // - Enables only necessary ADCs
    // - Uses 25Hz sampling rate (saves ~50-100µA)
    // - Configures sleep timeouts (5s dim, 15s sleep)
    
    watch->lvgl_begin();
    
    // === Optional: Customize power management ===
    
    // Configure sleep timeouts (optional - defaults are good)
    auto powerMgr = watch->getPowerManager();
    if (powerMgr) {
        powerMgr->configureSleepTimeouts(10000, 30000);  // 10s dim, 30s sleep
        
        // Register for power state changes
        powerMgr->onStateChange([](auto state) {
            using State = twatch::power::TTGOPowerManager::SleepState;
            switch (state) {
                case State::ACTIVE:
                    Serial.println("State: ACTIVE");
                    break;
                case State::DIM:
                    Serial.println("State: DIM");
                    break;
                case State::LIGHT_SLEEP:
                    Serial.println("State: LIGHT_SLEEP");
                    break;
                default:
                    break;
            }
        });
    }
    
    // === Battery monitoring with intelligent estimation ===
    auto batteryEst = watch->getBatteryEstimator();
    if (batteryEst) {
        batteryEst->setCapacity(350.0f);  // 350mAh battery
    }
    
    // === Power event callbacks ===
    auto eventMgr = watch->getEventManager();
    if (eventMgr) {
        // Low battery warning
        eventMgr->onBatteryLow([](int percent) {
            Serial.printf("Battery low: %d%%\n", percent);
            // Could switch to ultra-save mode here
        });
        
        // Charging state
        eventMgr->onChargingStateChange([](bool charging) {
            if (charging) {
                Serial.println("Charging started");
                // Could enable WiFi sync during charging
            } else {
                Serial.println("Charging stopped");
            }
        });
        
        // Set emergency threshold (auto power-save below 10%)
        eventMgr->setEmergencyThreshold(10);
    }
    
    // === Automatic frequency scaling for WiFi ===
    auto freqScaler = watch->getFrequencyScaler();
    // Frequency scaler will automatically boost CPU to 160MHz when WiFi is active
    // No manual frequency management needed!
    
    // Create simple UI
    createWatchFaceUI();
    
    Serial.println("Setup complete - power optimized!");
    printPowerStats();
}

void loop() {
    // Power management is automatic!
    // Just handle LVGL tasks - power manager does the rest
    
    auto powerMgr = watch->getPowerManager();
    if (powerMgr) {
        // Update power manager (handles automatic state transitions)
        powerMgr->update(millis());
        
        // Get current state
        auto state = powerMgr->getCurrentState();
        
        // Only run LVGL when not sleeping
        using State = twatch::power::TTGOPowerManager::SleepState;
        if (state < State::LIGHT_SLEEP) {
            lv_task_handler();
        }
        
        // Print stats every 10 seconds
        static uint32_t lastStats = 0;
        if (millis() - lastStats > 10000) {
            printPowerStats();
            lastStats = millis();
        }
    } else {
        // Fallback if power manager not available
        lv_task_handler();
    }
    
    delay(5);
}

void createWatchFaceUI() {
    // Create a simple watch face
    lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, "Power-Optimized\nWatch\n\nTap to wake");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
    
    // Style
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, LV_STATE_DEFAULT, &lv_font_montserrat_24);
    lv_obj_add_style(label, LV_LABEL_PART_MAIN, &style);
}

void printPowerStats() {
    Serial.println("\n=== Power Statistics ===");
    
    auto powerMgr = watch->getPowerManager();
    if (powerMgr) {
        // Current power consumption estimate
        float currentMa = powerMgr->estimatePowerConsumption();
        Serial.printf("Estimated current draw: %.1f mA\n", currentMa);
        
        // Battery life estimate
        float batteryLife = powerMgr->estimateBatteryLifeHours(350.0f);
        Serial.printf("Estimated battery life: %.1f hours\n", batteryLife);
        
        // Current profile and state
        auto profile = powerMgr->getCurrentProfile();
        auto state = powerMgr->getCurrentState();
        Serial.printf("Profile: %d, State: %d\n", (int)profile, (int)state);
    }
    
    // Battery voltage
    if (watch->power) {
        float voltage = watch->power->getBattVoltage();
        float percentage = ((voltage - 3.3) / (4.2 - 3.3)) * 100.0f;
        percentage = constrain(percentage, 0, 100);
        Serial.printf("Battery: %.2fV (%.0f%%)\n", voltage, percentage);
        
        bool charging = watch->power->isChargeing();
        Serial.printf("Charging: %s\n", charging ? "Yes" : "No");
    }
    
    // Event manager stats
    auto eventMgr = watch->getEventManager();
    if (eventMgr) {
        float totalPower = eventMgr->getTotalPowerConsumption();
        Serial.printf("Registered power consumers: %.1f mA\n", totalPower);
    }
    
    // CPU frequency
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    
    Serial.println("========================\n");
}

/*
 * Example WiFi usage with automatic frequency scaling:
 * 
 * void syncWithWiFi() {
 *     auto freqScaler = watch->getFrequencyScaler();
 *     
 *     // Notify frequency scaler (automatic boost to 160MHz)
 *     if (freqScaler) {
 *         freqScaler->notifyWiFiStateChange(true);
 *     }
 *     
 *     WiFi.begin(ssid, password);
 *     // ... do WiFi operations ...
 *     WiFi.disconnect();
 *     
 *     // Notify WiFi off (automatic return to efficient frequency)
 *     if (freqScaler) {
 *         freqScaler->notifyWiFiStateChange(false);
 *     }
 * }
 * 
 * Example battery time estimation:
 * 
 * void showBatteryInfo() {
 *     auto batteryEst = watch->getBatteryEstimator();
 *     if (batteryEst) {
 *         float voltage = watch->power->getBattVoltage();
 *         float percentage = ((voltage - 3.3) / (4.2 - 3.3)) * 100.0f;
 *         float currentMa = 40.0f;  // Example current draw
 *         
 *         uint32_t secondsRemaining = batteryEst->estimateRemainingSeconds(currentMa, percentage);
 *         float hoursRemaining = secondsRemaining / 3600.0f;
 *         
 *         Serial.printf("Time remaining: %.1f hours\n", hoursRemaining);
 *     }
 * }
 * 
 * Example manual power profile:
 * 
 * void setHighPerformanceMode() {
 *     auto powerMgr = watch->getPowerManager();
 *     if (powerMgr) {
 *         using Profile = twatch::power::TTGOPowerManager::PowerProfile;
 *         powerMgr->setProfile(Profile::PERFORMANCE);  // 240MHz
 *     }
 * }
 * 
 * void setUltraSaveMode() {
 *     auto powerMgr = watch->getPowerManager();
 *     if (powerMgr) {
 *         using Profile = twatch::power::TTGOPowerManager::PowerProfile;
 *         powerMgr->setProfile(Profile::ULTRA_SAVE);  // 20MHz
 *     }
 * }
 */
