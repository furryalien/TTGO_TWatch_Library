#pragma once

#include <Arduino.h>

namespace twatch {
namespace power {

/**
 * @brief Manages sleep transitions with spurious wake prevention
 * 
 * Prevents false wakes from vibration/motion and oscillating brightness issues.
 */
class SleepTransitionManager {
public:
    enum class WakeSource {
        BUTTON,
        TOUCH,
        MOTION,
        RTC_ALARM,
        UNKNOWN
    };
    
    SleepTransitionManager() 
        : lastRealActivity(0)
        , sleepDebounceMs(2000)
        , spuriousWakeCount(0) {
        lastRealActivity = millis();
    }
    
    /**
     * @brief Determine if wake should proceed or is spurious
     */
    bool shouldActuallyWake(WakeSource source) {
        uint32_t now = millis();
        uint32_t timeSinceActivity = now - lastRealActivity;
        
        // Button and touch are always real
        if (isButtonOrTouch(source)) {
            lastRealActivity = now;
            return true;
        }
        
        // RTC alarm is always real
        if (source == WakeSource::RTC_ALARM) {
            lastRealActivity = now;
            return true;
        }
        
        // Motion only counts if recent activity (within debounce window)
        if (isMotionOnly(source)) {
            if (timeSinceActivity < sleepDebounceMs) {
                lastRealActivity = now;
                return true;
            } else {
                // Spurious wake from vibration
                spuriousWakeCount++;
                return false;
            }
        }
        
        // Unknown source - be conservative, allow wake
        lastRealActivity = now;
        return true;
    }
    
    /**
     * @brief Record activity from a specific source
     */
    void recordActivity(WakeSource source) {
        if (isButtonOrTouch(source) || source == WakeSource::RTC_ALARM) {
            lastRealActivity = millis();
        }
    }
    
    /**
     * @brief Set debounce time for motion wake
     */
    void setSleepDebounceMs(uint32_t ms) {
        sleepDebounceMs = ms;
    }
    
    uint32_t getSleepDebounceMs() const {
        return sleepDebounceMs;
    }
    
    /**
     * @brief Get count of spurious wakes
     */
    uint32_t getSpuriousWakeCount() const {
        return spuriousWakeCount;
    }
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics() {
        spuriousWakeCount = 0;
    }
    
    /**
     * @brief Get time since last real activity
     */
    uint32_t getTimeSinceLastActivityMs(uint32_t currentMs) const {
        return currentMs - lastRealActivity;
    }
    
    /**
     * @brief Enter sleep mode
     * @param maxSleepMs Maximum sleep duration (0 = indefinite)
     */
    void enterSleep(uint32_t maxSleepMs = 0) {
        // Implementation would:
        // 1. Prepare peripherals for sleep
        // 2. Configure wake sources
        // 3. Enter ESP32 light sleep
        // 4. Validate wake source on wake
        // 5. Re-enter sleep if spurious
    }
    
private:
    uint32_t lastRealActivity;
    uint32_t sleepDebounceMs;
    uint32_t spuriousWakeCount;
    
    bool isButtonOrTouch(WakeSource source) const {
        return source == WakeSource::BUTTON || source == WakeSource::TOUCH;
    }
    
    bool isMotionOnly(WakeSource source) const {
        return source == WakeSource::MOTION;
    }
};

} // namespace power
} // namespace twatch
