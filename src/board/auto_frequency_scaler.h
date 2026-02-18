#pragma once

#include <Arduino.h>

namespace twatch {
namespace power {

/**
 * @brief Automatic CPU frequency scaling based on system state
 * 
 * Saves 20-40mA by adjusting CPU frequency based on workload.
 */
class AutoFrequencyScaler {
public:
    AutoFrequencyScaler() 
        : wifiActive(false)
        , animationActive(false)
        , displayActive(true)
        , currentFrequencyMhz(80)
        , frequencyChangeCount(0)
        , manualFrequencyRequest(0) {}
    
    /**
     * @brief Notify WiFi state change
     */
    void notifyWiFiStateChange(bool active) {
        wifiActive = active;
        reevaluateFrequency();
    }
    
    /**
     * @brief Notify animation state change
     */
    void notifyAnimationStateChange(bool active) {
        animationActive = active;
        reevaluateFrequency();
    }
    
    /**
     * @brief Notify display state change
     */
    void notifyDisplayStateChange(bool active) {
        displayActive = active;
        reevaluateFrequency();
    }
    
    /**
     * @brief Request specific frequency (high priority)
     */
    void requestFrequency(uint32_t mhz, const char* reason) {
        manualFrequencyRequest = mhz;
        applyFrequency(mhz);
    }
    
    /**
     * @brief Release manual frequency request
     */
    void releaseFrequencyRequest() {
        manualFrequencyRequest = 0;
        reevaluateFrequency();
    }
    
    /**
     * @brief Get current CPU frequency in MHz
     */
    uint32_t getCurrentFrequencyMhz() const {
        return currentFrequencyMhz;
    }
    
    /**
     * @brief Get recommended frequency based on current state
     */
    uint32_t getRecommendedFrequencyMhz() const {
        // Manual request has highest priority
        if (manualFrequencyRequest > 0) {
            return manualFrequencyRequest;
        }
        
        // WiFi requires high frequency
        if (wifiActive) {
            return 160;
        }
        
        // Display off allows minimum frequency
        if (!displayActive) {
            return 20;
        }
        
        // Animation needs moderate-high frequency
        if (animationActive) {
            return 80;
        }
        
        // Static display - low frequency
        return 40;
    }
    
    /**
     * @brief Apply frequency change immediately
     */
    void applyFrequencyChange() {
        uint32_t newFreq = getRecommendedFrequencyMhz();
        if (newFreq != currentFrequencyMhz) {
            applyFrequency(newFreq);
        }
    }
    
    /**
     * @brief Get number of frequency changes
     */
    uint32_t getFrequencyChangeCount() const {
        return frequencyChangeCount;
    }
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics() {
        frequencyChangeCount = 0;
    }
    
private:
    bool wifiActive;
    bool animationActive;
    bool displayActive;
    uint32_t currentFrequencyMhz;
    uint32_t frequencyChangeCount;
    uint32_t manualFrequencyRequest;
    
    void reevaluateFrequency() {
        uint32_t newFreq = getRecommendedFrequencyMhz();
        if (newFreq != currentFrequencyMhz) {
            applyFrequency(newFreq);
        }
    }
    
    void applyFrequency(uint32_t mhz) {
        setCpuFrequencyMhz(mhz);
        currentFrequencyMhz = mhz;
        frequencyChangeCount++;
    }
};

} // namespace power
} // namespace twatch
