#pragma once

#include <Arduino.h>

namespace twatch {
namespace power {

/**
 * @brief Smart ADC management for power optimization
 * 
 * Automatically enables only requested ADC channels and optimizes
 * sampling rate for power savings (~100-200µA potential savings).
 */
class SmartADCManager {
public:
    // ADC channel masks (matching AXP202)
    static constexpr uint8_t BATT_VOL = 0x80;
    static constexpr uint8_t BATT_CUR = 0x40;
    static constexpr uint8_t VBUS_VOL = 0x08;
    static constexpr uint8_t VBUS_CUR = 0x04;
    static constexpr uint8_t TEMP_SENSOR = 0x01;
    
    SmartADCManager() : activeMonitors(0), samplingRate(200) {}
    
    /**
     * @brief Enable specific ADC channels
     * @param adcMask Bitmask of ADC channels to enable
     */
    void enableMonitoring(uint8_t adcMask) {
        if (activeMonitors == 0) {
            // First monitor - configure initial sampling rate
            samplingRate = 200;  // Default
        }
        activeMonitors |= adcMask;
    }
    
    /**
     * @brief Disable specific ADC channels
     * @param adcMask Bitmask of ADC channels to disable
     */
    void disableMonitoring(uint8_t adcMask) {
        activeMonitors &= ~adcMask;
        
        if (activeMonitors == 0) {
            // No monitors, can stop sampling
            samplingRate = 0;
        }
    }
    
    /**
     * @brief Enable battery voltage and current monitoring
     */
    void enableBatteryMonitoring() {
        enableMonitoring(BATT_VOL | BATT_CUR);
    }
    
    /**
     * @brief Disable battery monitoring
     */
    void disableBatteryMonitoring() {
        disableMonitoring(BATT_VOL | BATT_CUR);
    }
    
    /**
     * @brief Enable VBUS (charging) monitoring
     */
    void enableChargingMonitoring() {
        enableMonitoring(VBUS_VOL | VBUS_CUR);
    }
    
    /**
     * @brief Disable charging monitoring
     */
    void disableChargingMonitoring() {
        disableMonitoring(VBUS_VOL | VBUS_CUR);
    }
    
    /**
     * @brief Enable temperature sensor monitoring
     */
    void enableTemperatureMonitoring() {
        enableMonitoring(TEMP_SENSOR);
    }
    
    /**
     * @brief Disable temperature sensor monitoring
     */
    void disableTemperatureMonitoring() {
        disableMonitoring(TEMP_SENSOR);
    }
    
    /**
     * @brief Check if specific ADC channel is being monitored
     */
    bool isMonitoring(uint8_t adcMask) const {
        return (activeMonitors & adcMask) != 0;
    }
    
    /**
     * @brief Get active monitor bitmask
     */
    uint8_t getActiveMonitors() const {
        return activeMonitors;
    }
    
    /**
     * @brief Get current sampling rate in Hz
     */
    uint32_t getSamplingRate() const {
        return samplingRate;
    }
    
    /**
     * @brief Optimize sampling rate based on active monitors
     * 
     * Reduces sampling rate to 25Hz for battery-only monitoring,
     * saving ~50-100µA compared to default 200Hz.
     */
    void optimizeSamplingRate() {
        if (activeMonitors == 0) {
            samplingRate = 0;
            return;
        }
        
        // If only battery monitoring (no VBUS), use lower rate
        bool onlyBattery = (activeMonitors & (BATT_VOL | BATT_CUR)) != 0 &&
                          (activeMonitors & (VBUS_VOL | VBUS_CUR | TEMP_SENSOR)) == 0;
        
        if (onlyBattery) {
            samplingRate = 25;  // Sufficient for battery monitoring
        } else {
            samplingRate = 200;  // Keep higher rate for other sensors
        }
    }
    
    /**
     * @brief Estimate power savings compared to all ADCs enabled at 200Hz
     * @return Estimated savings in µA
     */
    float estimatePowerSavings() const {
        // Baseline: All ADCs at 200Hz ≈ 200µA
        // Each disabled ADC saves ~30-40µA
        // Lower sampling rate saves ~50-100µA
        
        int disabledCount = 0;
        if (!(activeMonitors & BATT_VOL)) disabledCount++;
        if (!(activeMonitors & BATT_CUR)) disabledCount++;
        if (!(activeMonitors & VBUS_VOL)) disabledCount++;
        if (!(activeMonitors & VBUS_CUR)) disabledCount++;
        if (!(activeMonitors & TEMP_SENSOR)) disabledCount++;
        
        float savings = disabledCount * 35.0f;  // µA per disabled ADC
        
        // Sampling rate reduction savings
        if (samplingRate <= 25) {
            savings += 75.0f;  // Additional savings from lower rate
        }
        
        return savings;
    }
    
private:
    uint8_t activeMonitors;
    uint32_t samplingRate;  // Hz
};

} // namespace power
} // namespace twatch
