#pragma once

#include <Arduino.h>
#include <functional>
#include <map>
#include <string>

namespace twatch {
namespace power {

/**
 * @brief Event-driven power management system
 * 
 * Provides callbacks for power events and tracks power budget.
 */
class PowerEventManager {
public:
    using BatteryLowCallback = std::function<void(int percent)>;
    using ChargingStateCallback = std::function<void(bool charging)>;
    using SleepStateCallback = std::function<void(int state)>;
    using PowerButtonCallback = std::function<void()>;
    
    PowerEventManager() 
        : emergencyThreshold(10)
        , totalPowerBudget(0.0f)
        , isEmergency(false) {}
    
    /**
     * @brief Register battery low callback
     */
    void onBatteryLow(BatteryLowCallback callback) {
        batteryLowCb = callback;
    }
    
    /**
     * @brief Register charging state change callback
     */
    void onChargingStateChange(ChargingStateCallback callback) {
        chargingStateCb = callback;
    }
    
    /**
     * @brief Register sleep state change callback
     */
    void onSleepStateChange(SleepStateCallback callback) {
        sleepStateCb = callback;
    }
    
    /**
     * @brief Register power button press callback
     */
    void onPowerButtonPress(PowerButtonCallback callback) {
        powerButtonCb = callback;
    }
    
    /**
     * @brief Register a power consumer
     * @param name Consumer name
     * @param mA Power consumption in milliamps
     */
    void registerPowerConsumer(const char* name, float mA) {
        std::string key(name);
        float oldValue = 0.0f;
        
        if (powerConsumers.find(key) != powerConsumers.end()) {
            oldValue = powerConsumers[key];
        }
        
        powerConsumers[key] = mA;
        totalPowerBudget = totalPowerBudget - oldValue + mA;
    }
    
    /**
     * @brief Unregister a power consumer
     */
    void unregisterPowerConsumer(const char* name) {
        std::string key(name);
        if (powerConsumers.find(key) != powerConsumers.end()) {
            totalPowerBudget -= powerConsumers[key];
            powerConsumers.erase(key);
        }
    }
    
    /**
     * @brief Get total power consumption from all registered consumers
     */
    float getTotalPowerConsumption() const {
        return totalPowerBudget;
    }
    
    /**
     * @brief Set battery percentage for emergency mode
     */
    void setEmergencyThreshold(int batteryPercent) {
        emergencyThreshold = batteryPercent;
    }
    
    int getEmergencyThreshold() const {
        return emergencyThreshold;
    }
    
    /**
     * @brief Check if in emergency power mode
     */
    bool isEmergencyMode() const {
        return isEmergency;
    }
    
    /**
     * @brief Trigger battery low event
     */
    void triggerBatteryLow(int percent) {
        if (percent <= emergencyThreshold) {
            isEmergency = true;
        }
        
        if (batteryLowCb) {
            batteryLowCb(percent);
        }
    }
    
    /**
     * @brief Trigger charging state change
     */
    void triggerChargingState(bool charging) {
        if (charging) {
            isEmergency = false;  // Exit emergency mode when charging
        }
        
        if (chargingStateCb) {
            chargingStateCb(charging);
        }
    }
    
    /**
     * @brief Trigger sleep state change
     */
    void triggerSleepStateChange(int state) {
        if (sleepStateCb) {
            sleepStateCb(state);
        }
    }
    
    /**
     * @brief Trigger power button press
     */
    void triggerPowerButton() {
        if (powerButtonCb) {
            powerButtonCb();
        }
    }
    
private:
    BatteryLowCallback batteryLowCb;
    ChargingStateCallback chargingStateCb;
    SleepStateCallback sleepStateCb;
    PowerButtonCallback powerButtonCb;
    
    int emergencyThreshold;
    float totalPowerBudget;
    bool isEmergency;
    std::map<std::string, float> powerConsumers;
};

} // namespace power
} // namespace twatch
