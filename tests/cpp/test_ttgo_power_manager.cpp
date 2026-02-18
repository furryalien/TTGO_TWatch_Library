#include <cassert>
<parameter name="content">#include <cassert>
#include <iostream>
#include <cstdint>
#include <functional>

// Forward declarations for testing
namespace twatch {
namespace power {

class TTGOPowerManager {
public:
    enum class PowerProfile {
        PERFORMANCE,   // 160-240 MHz - network, heavy UI
        BALANCED,      // 80 MHz - normal operation
        EFFICIENT,     // 40 MHz - static display
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
        , sleepTimeoutMs(15000) {}
    
    // Automatic profile management
    void setAutoProfileManagement(bool enable);
    bool isAutoManagementEnabled() const { return autoManagement; }
    
    void setProfile(PowerProfile profile);
    PowerProfile getCurrentProfile() const { return currentProfile; }
    
    // Sleep state management
    void updateActivityTimestamp();
    uint32_t getInactiveTimeMs() const;
    SleepState getCurrentState() const { return currentState; }
    void configureSleepTimeouts(uint32_t dimMs, uint32_t sleepMs);
    
    // Peripheral optimization
    void optimizePeripheralsForState(SleepState state);
    
    // Power estimation
    float estimatePowerConsumption() const;      // mA
    float estimateBatteryLifeHours(float batteryMah) const;
    
    // Power event callbacks
    using StateChangeCallback = std::function<void(SleepState)>;
    using ProfileChangeCallback = std::function<void(PowerProfile)>;
    
    void onStateChange(StateChangeCallback callback);
    void onProfileChange(ProfileChangeCallback callback);
    
    // Update method (called regularly from main loop)
    void update(uint32_t currentTimeMs);
    
private:
    bool autoManagement;
    PowerProfile currentProfile;
    SleepState currentState;
    uint32_t lastActivityMs;
    uint32_t dimTimeoutMs;
    uint32_t sleepTimeoutMs;
    StateChangeCallback stateCallback;
    ProfileChangeCallback profileCallback;
};

} // namespace power
} // namespace twatch

// Test implementations
void test_default_state() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    assert(pm.getCurrentProfile() == TTGOPowerManager::PowerProfile::BALANCED);
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::ACTIVE);
    assert(!pm.isAutoManagementEnabled());
    std::cout << "✓ Default state is BALANCED profile and ACTIVE state\n";
}

void test_profile_switching() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    pm.setProfile(TTGOPowerManager::PowerProfile::PERFORMANCE);
    assert(pm.getCurrentProfile() == TTGOPowerManager::PowerProfile::PERFORMANCE);
    
    pm.setProfile(TTGOPowerManager::PowerProfile::ULTRA_SAVE);
    assert(pm.getCurrentProfile() == TTGOPowerManager::PowerProfile::ULTRA_SAVE);
    
    std::cout << "✓ Profile switching works correctly\n";
}

void test_activity_timestamp_tracking() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    pm.updateActivityTimestamp();
    uint32_t inactive1 = pm.getInactiveTimeMs();
    
    // Simulate time passing
    pm.update(10000);  // 10 seconds later
    uint32_t inactive2 = pm.getInactiveTimeMs();
    
    assert(inactive2 > inactive1);
    std::cout << "✓ Activity timestamp tracking works\n";
}

void test_sleep_timeout_configuration() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    pm.configureSleepTimeouts(3000, 10000);  // 3s dim, 10s sleep
    
    // Should accept configuration without error
    std::cout << "✓ Sleep timeout configuration works\n";
}

void test_state_transitions_by_inactivity() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    pm.configureSleepTimeouts(5000, 10000);  // 5s dim, 10s sleep
    
    // Start active
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::ACTIVE);
    
    // After 6s, should move to DIM
    pm.update(6000);
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::DIM);
    
    // After 11s total, should move to LIGHT_SLEEP
    pm.update(11000);
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::LIGHT_SLEEP);
    
    std::cout << "✓ State transitions based on inactivity work correctly\n";
}

void test_activity_resets_sleep_timer() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    pm.configureSleepTimeouts(5000, 10000);
    
    // Start activity
    pm.updateActivityTimestamp();
    pm.update(3000);  // 3s later
    
    // Still active
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::ACTIVE);
    
    // New activity resets timer
    pm.updateActivityTimestamp();
    pm.update(3000);  // Another 3s (6s total, but timer reset at 3s)
    
    // Should still be active
    assert(pm.getCurrentState() == TTGOPowerManager::SleepState::ACTIVE);
    
    std::cout << "✓ Activity correctly resets sleep timer\n";
}

void test_state_change_callback() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    pm.configureSleepTimeouts(5000, 10000);
    
    bool callbackCalled = false;
    TTGOPowerManager::SleepState receivedState;
    
    pm.onStateChange([&](TTGOPowerManager::SleepState state) {
        callbackCalled = true;
        receivedState = state;
    });
    
    // Trigger state change
    pm.update(6000);  // Move to DIM
    
    assert(callbackCalled);
    assert(receivedState == TTGOPowerManager::SleepState::DIM);
    std::cout << "✓ State change callback is triggered correctly\n";
}

void test_profile_change_callback() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    bool callbackCalled = false;
    TTGOPowerManager::PowerProfile receivedProfile;
    
    pm.onProfileChange([&](TTGOPowerManager::PowerProfile profile) {
        callbackCalled = true;
        receivedProfile = profile;
    });
    
    pm.setProfile(TTGOPowerManager::PowerProfile::PERFORMANCE);
    
    assert(callbackCalled);
    assert(receivedProfile == TTGOPowerManager::PowerProfile::PERFORMANCE);
    std::cout << "✓ Profile change callback is triggered correctly\n";
}

void test_power_consumption_estimation() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    // Active state should have higher power consumption
    pm.setProfile(TTGOPowerManager::PowerProfile::PERFORMANCE);
    float activePower = pm.estimatePowerConsumption();
    
    // Sleep state should have lower power consumption
    pm.update(20000);  // Force to LIGHT_SLEEP
    float sleepPower = pm.estimatePowerConsumption();
    
    assert(activePower > sleepPower);
    std::cout << "✓ Power consumption estimation varies correctly by state\n";
}

void test_battery_life_estimation() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    float batteryLife = pm.estimateBatteryLifeHours(350.0f);  // 350mAh battery
    
    // Should return a reasonable value (positive and > 0)
    assert(batteryLife > 0);
    assert(batteryLife < 1000);  // Less than 1000 hours
    std::cout << "✓ Battery life estimation returns reasonable values\n";
}

void test_auto_profile_management() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    pm.setAutoProfileManagement(true);
    assert(pm.isAutoManagementEnabled());
    
    pm.setAutoProfileManagement(false);
    assert(!pm.isAutoManagementEnabled());
    
    std::cout << "✓ Auto profile management can be enabled/disabled\n";
}

void test_peripheral_optimization() {
    using twatch::power::TTGOPowerManager;
    
    TTGOPowerManager pm;
    
    // Should not crash for any state
    pm.optimizePeripheralsForState(TTGOPowerManager::SleepState::ACTIVE);
    pm.optimizePeripheralsForState(TTGOPowerManager::SleepState::LIGHT_SLEEP);
    pm.optimizePeripheralsForState(TTGOPowerManager::SleepState::STANDBY);
    
    std::cout << "✓ Peripheral optimization works for all states\n";
}

int main() {
    std::cout << "Running TTGOPowerManager tests...\n\n";
    
    test_default_state();
    test_profile_switching();
    test_activity_timestamp_tracking();
    test_sleep_timeout_configuration();
    test_state_transitions_by_inactivity();
    test_activity_resets_sleep_timer();
    test_state_change_callback();
    test_profile_change_callback();
    test_power_consumption_estimation();
    test_battery_life_estimation();
    test_auto_profile_management();
    test_peripheral_optimization();
    
    std::cout << "\n✅ All TTGOPowerManager tests passed!\n";
    return 0;
}
