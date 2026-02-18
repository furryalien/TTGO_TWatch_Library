#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace twatch {
namespace power {

/**
 * @brief Power configuration persistence using NVS
 * 
 * Saves and restores power settings across reboots.
 */
class PowerConfigPersistence {
public:
    enum class PowerProfile {
        PERFORMANCE,
        BALANCED,
        EFFICIENT,
        ULTRA_SAVE
    };
    
    enum class BrightnessPolicy {
        MANUAL,
        AUTO,
        CONTEXT_AWARE
    };
    
    PowerConfigPersistence() 
        : savedProfile(PowerProfile::BALANCED)
        , savedDimMs(5000)
        , savedSleepMs(15000)
        , savedBrightnessPolicy(BrightnessPolicy::AUTO)
        , persistenceEnabled(false)
        , settingsExist(false) {
        prefs.begin("ttgo_power", false);
    }
    
    ~PowerConfigPersistence() {
        prefs.end();
    }
    
    /**
     * @brief Enable or disable persistence
     */
    void setPersistenceEnabled(bool enabled) {
        persistenceEnabled = enabled;
    }
    
    bool isPersistenceEnabled() const {
        return persistenceEnabled;
    }
    
    /**
     * @brief Save power profile
     */
    bool savePowerProfile(PowerProfile profile) {
        if (!persistenceEnabled) return false;
        
        savedProfile = profile;
        prefs.putUInt("profile", static_cast<uint32_t>(profile));
        settingsExist = true;
        return true;
    }
    
    /**
     * @brief Load power profile
     */
    PowerProfile loadPowerProfile() {
        if (!persistenceEnabled) {
            return PowerProfile::BALANCED;
        }
        
        uint32_t value = prefs.getUInt("profile", static_cast<uint32_t>(PowerProfile::BALANCED));
        return static_cast<PowerProfile>(value);
    }
    
    /**
     * @brief Save display timeouts
     */
    bool saveDisplayTimeouts(uint32_t dimMs, uint32_t sleepMs) {
        if (!persistenceEnabled) return false;
        
        savedDimMs = dimMs;
        savedSleepMs = sleepMs;
        prefs.putUInt("dim_ms", dimMs);
        prefs.putUInt("sleep_ms", sleepMs);
        settingsExist = true;
        return true;
    }
    
    /**
     * @brief Load display timeouts
     */
    void loadDisplayTimeouts(uint32_t& dimMs, uint32_t& sleepMs) {
        if (!persistenceEnabled) {
            dimMs = 5000;
            sleepMs = 15000;
            return;
        }
        
        dimMs = prefs.getUInt("dim_ms", 5000);
        sleepMs = prefs.getUInt("sleep_ms", 15000);
    }
    
    /**
     * @brief Save brightness policy
     */
    bool saveBrightnessPolicy(BrightnessPolicy policy) {
        if (!persistenceEnabled) return false;
        
        savedBrightnessPolicy = policy;
        prefs.putUInt("brightness", static_cast<uint32_t>(policy));
        settingsExist = true;
        return true;
    }
    
    /**
     * @brief Load brightness policy
     */
    BrightnessPolicy loadBrightnessPolicy() {
        if (!persistenceEnabled) {
            return BrightnessPolicy::AUTO;
        }
        
        uint32_t value = prefs.getUInt("brightness", static_cast<uint32_t>(BrightnessPolicy::AUTO));
        return static_cast<BrightnessPolicy>(value);
    }
    
    /**
     * @brief Automatically restore all settings
     */
    void autoRestore() {
        if (!persistenceEnabled) return;
        
        savedProfile = loadPowerProfile();
        loadDisplayTimeouts(savedDimMs, savedSleepMs);
        savedBrightnessPolicy = loadBrightnessPolicy();
    }
    
    /**
     * @brief Clear all stored settings
     */
    void clearAllSettings() {
        prefs.clear();
        settingsExist = false;
    }
    
    /**
     * @brief Check if stored settings exist
     */
    bool hasStoredSettings() const {
        return settingsExist;
    }
    
private:
    Preferences prefs;
    PowerProfile savedProfile;
    uint32_t savedDimMs;
    uint32_t savedSleepMs;
    BrightnessPolicy savedBrightnessPolicy;
    bool persistenceEnabled;
    bool settingsExist;
};

} // namespace power
} // namespace twatch
