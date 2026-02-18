#include <cassert>
#include <iostream>
#include <cstdint>
#include <string>

// Forward declarations for testing
namespace twatch {
namespace power {

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
        , persistenceEnabled(false) {}
    
    // Save/load power profile
    bool savePowerProfile(PowerProfile profile);
    PowerProfile loadPowerProfile();
    
    // Save/load display timeouts
    bool saveDisplayTimeouts(uint32_t dimMs, uint32_t sleepMs);
    void loadDisplayTimeouts(uint32_t& dimMs, uint32_t& sleepMs);
    
    // Save/load brightness policy
    bool saveBrightnessPolicy(BrightnessPolicy policy);
    BrightnessPolicy loadBrightnessPolicy();
    
    // Automatic restore on initialization
    void autoRestore();
    
    // Enable/disable persistence
    void setPersistenceEnabled(bool enabled) { persistenceEnabled = enabled; }
    bool isPersistenceEnabled() const { return persistenceEnabled; }
    
    // Clear all saved settings
    void clearAllSettings();
    
    // Check if settings exist
    bool hasStoredSettings() const;
    
private:
    PowerProfile savedProfile;
    uint32_t savedDimMs;
    uint32_t savedSleepMs;
    BrightnessPolicy savedBrightnessPolicy;
    bool persistenceEnabled;
    bool settingsExist = false;
};

} // namespace power
} // namespace twatch

// Test implementations
void test_save_and_load_power_profile() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    bool saved = pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::PERFORMANCE);
    assert(saved);
    
    auto loaded = pcp.loadPowerProfile();
    assert(loaded == PowerConfigPersistence::PowerProfile::PERFORMANCE);
    
    std::cout << "✓ Save and load power profile works\n";
}

void test_save_and_load_display_timeouts() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    bool saved = pcp.saveDisplayTimeouts(10000, 30000);
    assert(saved);
    
    uint32_t dimMs = 0, sleepMs = 0;
    pcp.loadDisplayTimeouts(dimMs, sleepMs);
    
    assert(dimMs == 10000);
    assert(sleepMs == 30000);
    
    std::cout << "✓ Save and load display timeouts works\n";
}

void test_save_and_load_brightness_policy() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    bool saved = pcp.saveBrightnessPolicy(PowerConfigPersistence::BrightnessPolicy::CONTEXT_AWARE);
    assert(saved);
    
    auto loaded = pcp.loadBrightnessPolicy();
    assert(loaded == PowerConfigPersistence::BrightnessPolicy::CONTEXT_AWARE);
    
    std::cout << "✓ Save and load brightness policy works\n";
}

void test_persistence_disabled_returns_defaults() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(false);
    
    // Save should fail or do nothing
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::ULTRA_SAVE);
    
    // Load should return default
    auto loaded = pcp.loadPowerProfile();
    assert(loaded == PowerConfigPersistence::PowerProfile::BALANCED);
    
    std::cout << "✓ Disabled persistence returns defaults\n";
}

void test_auto_restore() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    // Save settings
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::EFFICIENT);
    pcp.saveDisplayTimeouts(8000, 25000);
    
    // Auto restore should load all settings
    pcp.autoRestore();
    
    // Settings should be restored
    auto profile = pcp.loadPowerProfile();
    assert(profile == PowerConfigPersistence::PowerProfile::EFFICIENT);
    
    std::cout << "✓ Auto restore loads all settings\n";
}

void test_clear_all_settings() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    // Save some settings
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::PERFORMANCE);
    pcp.saveDisplayTimeouts(10000, 30000);
    
    // Clear all
    pcp.clearAllSettings();
    
    // Should not have stored settings
    assert(!pcp.hasStoredSettings());
    
    std::cout << "✓ Clear all settings works\n";
}

void test_has_stored_settings() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    // Initially no settings
    pcp.clearAllSettings();
    assert(!pcp.hasStoredSettings());
    
    // After save, should have settings
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::BALANCED);
    assert(pcp.hasStoredSettings());
    
    std::cout << "✓ Has stored settings detection works\n";
}

void test_multiple_save_operations() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    // Multiple saves should work
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::PERFORMANCE);
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::EFFICIENT);
    pcp.savePowerProfile(PowerConfigPersistence::PowerProfile::ULTRA_SAVE);
    
    // Last save should win
    auto loaded = pcp.loadPowerProfile();
    assert(loaded == PowerConfigPersistence::PowerProfile::ULTRA_SAVE);
    
    std::cout << "✓ Multiple save operations work correctly\n";
}

void test_timeout_boundary_values() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    pcp.setPersistenceEnabled(true);
    
    // Test boundary values
    pcp.saveDisplayTimeouts(0, 0);
    uint32_t dimMs = 0, sleepMs = 0;
    pcp.loadDisplayTimeouts(dimMs, sleepMs);
    assert(dimMs == 0 && sleepMs == 0);
    
    pcp.saveDisplayTimeouts(UINT32_MAX, UINT32_MAX);
    pcp.loadDisplayTimeouts(dimMs, sleepMs);
    assert(dimMs == UINT32_MAX && sleepMs == UINT32_MAX);
    
    std::cout << "✓ Timeout boundary values work correctly\n";
}

void test_enable_disable_persistence() {
    using twatch::power::PowerConfigPersistence;
    
    PowerConfigPersistence pcp;
    
    assert(!pcp.isPersistenceEnabled());
    
    pcp.setPersistenceEnabled(true);
    assert(pcp.isPersistenceEnabled());
    
    pcp.setPersistenceEnabled(false);
    assert(!pcp.isPersistenceEnabled());
    
    std::cout << "✓ Enable/disable persistence works\n";
}

int main() {
    std::cout << "Running PowerConfigPersistence tests...\n\n";
    
    test_save_and_load_power_profile();
    test_save_and_load_display_timeouts();
    test_save_and_load_brightness_policy();
    test_persistence_disabled_returns_defaults();
    test_auto_restore();
    test_clear_all_settings();
    test_has_stored_settings();
    test_multiple_save_operations();
    test_timeout_boundary_values();
    test_enable_disable_persistence();
    
    std::cout << "\n✅ All PowerConfigPersistence tests passed!\n";
    return 0;
}
