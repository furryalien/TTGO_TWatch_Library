#pragma once

#include <Arduino.h>
#include <functional>

namespace twatch {
namespace power {

/**
 * @brief Unified power management for TTGO Watch
 * 
 * Provides automatic power profile management, sleep state tracking,
 * and power consumption estimation.
 */
class TTGOPowerManager {
public:
    enum class PowerProfile {
        PERFORMANCE,   // 160-240 MHz - network, heavy UI
        BALANCED,      // 80 MHz - normal operation
        EFFICIENT,     // 40 MHz -static display
        ULTRA_SAVE     // 20 MHz - minimal updates
    };
    
    enum class SleepState {
        ACTIVE,        // Full power, screen on
        IDLE,          // Reduced refresh, ready to sleep
        DIM,           // Screen dimmed
        LIGHT_SLEEP,   // Display off, quick wake
        STANDBY        // Maximum savings
    };
    
    TTGOPowerManager() 
        : autoManagement(false)
        , currentProfile(PowerProfile::BALANCED)
        , currentState(SleepState::ACTIVE)
        , lastActivityMs(0)
        , dimTimeoutMs(5000)
        , sleepTimeoutMs(15000)
        , stateCallback(nullptr)
        , profileCallback(nullptr) {
        lastActivityMs = millis();
    }
    
    /**
     * @brief Enable or disable automatic profile management
     */
    void setAutoProfileManagement(bool enable) {
        autoManagement = enable;
    }
    
    bool isAutoManagementEnabled() const {
        return autoManagement;
    }
    
    /**
     * @brief Set power profile manually
     */
    void setProfile(PowerProfile profile) {
        if (currentProfile != profile) {
            currentProfile = profile;
            applyProfile(profile);
            
            if (profileCallback) {
                profileCallback(profile);
            }
        }
    }
    
    PowerProfile getCurrentProfile() const {
        return currentProfile;
    }
    
    /**
     * @brief Record user activity (resets sleep timer)
     */
    void updateActivityTimestamp() {
        lastActivityMs = millis();
        
        // If in sleep, wake up
        if (currentState >= SleepState::LIGHT_SLEEP) {
            transitionToState(SleepState::ACTIVE);
        }
    }
    
    /**
     * @brief Get time since last activity in milliseconds
     */
    uint32_t getInactiveTimeMs() const {
        return millis() - lastActivityMs;
    }
    
    SleepState getCurrentState() const {
        return currentState;
    }
    
    /**
     * @brief Configure sleep timeouts
     * @param dimMs Time until screen dims (milliseconds)
     * @param sleepMs Time until sleep mode (milliseconds)
     */
    void configureSleepTimeouts(uint32_t dimMs, uint32_t sleepMs) {
        dimTimeoutMs = dimMs;
        sleepTimeoutMs = sleepMs;
    }
    
    /**
     * @brief Optimize peripherals for a given sleep state
     */
    void optimizePeripheralsForState(SleepState state) {
        // Implementation would control ADCs, sensors, etc.
        switch (state) {
            case SleepState::ACTIVE:
                // Everything on
                break;
            case SleepState::IDLE:
                // Reduce refresh rate
                break;
            case SleepState::DIM:
                // Lower brightness, reduce CPU
                break;
            case SleepState::LIGHT_SLEEP:
                // Minimal power, quick wake
                break;
            case SleepState::STANDBY:
                // Maximum power savings
                break;
        }
    }
    
    /**
     * @brief Estimate current power consumption in mA
     */
    float estimatePowerConsumption() const {
        float power = 0.0f;
        
        // Base power by profile
        switch (currentProfile) {
            case PowerProfile::PERFORMANCE:
                power += 80.0f;  // CPU at high frequency
                break;
            case PowerProfile::BALANCED:
                power += 30.0f;  // CPU at 80MHz
                break;
            case PowerProfile::EFFICIENT:
                power += 15.0f;  // CPU at 40MHz
                break;
            case PowerProfile::ULTRA_SAVE:
                power += 5.0f;   // CPU at 20MHz
                break;
        }
        
        // Add state-dependent power
        switch (currentState) {
            case SleepState::ACTIVE:
                power += 60.0f;  // Display + backlight
                break;
            case SleepState::IDLE:
                power += 45.0f;  // Display dimmed
                break;
            case SleepState::DIM:
                power += 20.0f;  // Very dim
                break;
            case SleepState::LIGHT_SLEEP:
                power += 2.0f;   // Minimal
                break;
            case SleepState::STANDBY:
                power += 0.5f;   // Maximum savings
                break;
        }
        
        return power;
    }
    
    /**
     * @brief Estimate battery life in hours
     * @param batteryMah Battery capacity in mAh
     */
    float estimateBatteryLifeHours(float batteryMah) const {
        float currentMa = estimatePowerConsumption();
        if (currentMa <= 0) return 9999.0f;
        return batteryMah / currentMa;
    }
    
    /**
     * @brief Register callback for state changes
     */
    void onStateChange(std::function<void(SleepState)> callback) {
        stateCallback = callback;
    }
    
    /**
     * @brief Register callback for profile changes
     */
    void onProfileChange(std::function<void(PowerProfile)> callback) {
        profileCallback = callback;
    }
    
    /**
     * @brief Update power management (call from main loop)
     */
    void update(uint32_t currentTimeMs) {
        uint32_t inactiveTime = currentTimeMs - lastActivityMs;
        
        SleepState newState = determineState(inactiveTime);
        
        if (newState != currentState) {
            transitionToState(newState);
        }
    }
    
private:
    bool autoManagement;
    PowerProfile currentProfile;
    SleepState currentState;
    uint32_t lastActivityMs;
    uint32_t dimTimeoutMs;
    uint32_t sleepTimeoutMs;
    std::function<void(SleepState)> stateCallback;
    std::function<void(PowerProfile)> profileCallback;
    
    void applyProfile(PowerProfile profile) {
        // Set CPU frequency based on profile
        switch (profile) {
            case PowerProfile::PERFORMANCE:
                setCpuFrequencyMhz(240);
                break;
            case PowerProfile::BALANCED:
                setCpuFrequencyMhz(80);
                break;
            case PowerProfile::EFFICIENT:
                setCpuFrequencyMhz(40);
                break;
            case PowerProfile::ULTRA_SAVE:
                setCpuFrequencyMhz(20);
                break;
        }
    }
    
    SleepState determineState(uint32_t inactiveTime) {
        if (inactiveTime < dimTimeoutMs) {
            return SleepState::ACTIVE;
        } else if (inactiveTime < sleepTimeoutMs) {
            return SleepState::DIM;
        } else {
            return SleepState::LIGHT_SLEEP;
        }
    }
    
    void transitionToState(SleepState newState) {
        currentState = newState;
        optimizePeripheralsForState(newState);
        
        if (stateCallback) {
            stateCallback(newState);
        }
    }
};

} // namespace power
} // namespace twatch
