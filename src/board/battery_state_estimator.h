#pragma once

#include <Arduino.h>
#include <cmath>

namespace twatch {
namespace power {

/**
 * @brief Battery state estimation and health monitoring
 * 
 * Provides intelligent battery life prediction and health tracking.
 */
class BatteryStateEstimator {
public:
    BatteryStateEstimator() 
        : capacityMah(350.0f)
        , lastFullChargeTimestamp(0)
        , averageDischargeCurrentMa(0.0f)
        , cycleCount(0)
        , totalDischargedMah(0.0f)
        , sampleCount(0) {}
    
    /**
     * @brief Set battery capacity
     */
    void setCapacity(float mah) {
        capacityMah = mah;
    }
    
    float getCapacity() const {
        return capacityMah;
    }
    
    /**
     * @brief Estimate remaining battery time
     * @param currentMa Current draw in milliamps
     * @param batteryPercent Current battery percentage
     * @return Estimated seconds remaining
     */
    uint32_t estimateRemainingSeconds(float currentMa, float batteryPercent) {
        if (currentMa <= 0.001f) {
            return UINT32_MAX;  // Effectively infinite
        }
        
        float remainingMah = capacityMah * (batteryPercent / 100.0f);
        float hoursRemaining = remainingMah / currentMa;
        return static_cast<uint32_t>(hoursRemaining * 3600.0f);
    }
    
    /**
     * @brief Estimate time to full charge
     * @param chargingCurrentMa Charging current in milliamps
     * @param batteryPercent Current battery percentage
     * @return Estimated seconds to full charge
     */
    uint32_t estimateTimeToFullChargeSeconds(float chargingCurrentMa, float batteryPercent) {
        if (chargingCurrentMa <= 0.001f) {
            return UINT32_MAX;
        }
        
        float neededMah = capacityMah * ((100.0f - batteryPercent) / 100.0f);
        float hoursToCharge = neededMah / chargingCurrentMa;
        return static_cast<uint32_t>(hoursToCharge * 3600.0f);
    }
    
    /**
     * @brief Get average discharge rate
     */
    float estimateAverageDischargeRate() const {
        return averageDischargeCurrentMa;
    }
    
    /**
     * @brief Notify of full charge (increments cycle count)
     */
    void notifyFullCharge() {
        cycleCount++;
        lastFullChargeTimestamp = millis() / 1000;  // Store as seconds
    }
    
    /**
     * @brief Update discharge rate (running average)
     */
    void updateDischargeRate(float mA) {
        sampleCount++;
        float alpha = 1.0f / sampleCount;
        if (sampleCount > 100) {
            alpha = 0.01f;  // Limit to last 100 samples
            sampleCount = 100;
        }
        
        averageDischargeCurrentMa = averageDischargeCurrentMa * (1.0f - alpha) + mA * alpha;
    }
    
    /**
     * @brief Get time since last full charge
     */
    uint32_t getTimeSinceLastFullCharge(uint32_t currentTimestamp) const {
        return currentTimestamp - lastFullChargeTimestamp;
    }
    
    /**
     * @brief Get average battery life over recent charges
     */
    float getAverageBatteryLifeHours() const {
        if (averageDischargeCurrentMa <= 0.001f) {
            return 0.0f;
        }
        return capacityMah / averageDischargeCurrentMa;
    }
    
    /**
     * @brief Estimate battery health as percentage
     * 
     * Based on cycle count with typical degradation curve.
     * Approximate 20% capacity loss after 500 cycles.
     */
    float estimateBatteryHealth() const {
        // Typical li-po degradation: ~0.04% per cycle
        // After 500 cycles: ~80% health
        float degradation = cycleCount * 0.04f;
        float health = 100.0f - degradation;
        
        if (health < 0.0f) health = 0.0f;
        if (health > 100.0f) health = 100.0f;
        
        return health;
    }
    
    /**
     * @brief Get charge cycle count
     */
    uint32_t getCycleCount() const {
        return cycleCount;
    }
    
    /**
     * @brief Reset all statistics
     */
    void resetStatistics() {
        cycleCount = 0;
        averageDischargeCurrentMa = 0.0f;
        totalDischargedMah = 0.0f;
        sampleCount = 0;
    }
    
    /**
     * @brief Calibrate with actual measured capacity
     */
    void calibrate(float actualCapacityMah) {
        capacityMah = actualCapacityMah;
    }
    
private:
    float capacityMah;
    uint32_t lastFullChargeTimestamp;  // seconds
    float averageDischargeCurrentMa;
    uint32_t cycleCount;
    float totalDischargedMah;
    uint32_t sampleCount;
};

} // namespace power
} // namespace twatch
