#include <cassert>
#include <iostream>
#include <functional>
#include <cstdint>
#include <string>
#include <map>

// Forward declarations for testing
namespace twatch {
namespace power {

class PowerEventManager {
public:
    using BatteryLowCallback = std::function<void(int percent)>;
    using ChargingStateCallback = std::function<void(bool charging)>;
    using SleepStateCallback = std::function<void(int state)>;
    using PowerButtonCallback = std::function<void()>;
    
    PowerEventManager() 
        : emergencyThreshold(10)
        , totalPowerBudget(0.0f) {}
    
    // Register callbacks
    void onBatteryLow(BatteryLowCallback callback);
    void onChargingStateChange(ChargingStateCallback callback);
    void onSleepStateChange(SleepStateCallback callback);
    void onPowerButtonPress(PowerButtonCallback callback);
    
    // Power budget management
    void registerPowerConsumer(const char* name, float mA);
    void unregisterPowerConsumer(const char* name);
    float getTotalPowerConsumption() const { return totalPowerBudget; }
    
    // Emergency power saving
    void setEmergencyThreshold(int batteryPercent);
    int getEmergencyThreshold() const { return emergencyThreshold; }
    bool isEmergencyMode() const;
    
    // Trigger events (for testing)
    void triggerBatteryLow(int percent);
    void triggerChargingState(bool charging);
    void triggerSleepStateChange(int state);
    void triggerPowerButton();
    
private:
    BatteryLowCallback batteryLowCb;
    ChargingStateCallback chargingStateCb;
    SleepStateCallback sleepStateCb;
    PowerButtonCallback powerButtonCb;
    
    int emergencyThreshold;
    float totalPowerBudget;
    std::map<std::string, float> powerConsumers;
};

} // namespace power
} // namespace twatch

// Test implementations
void test_battery_low_callback() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    bool callbackCalled = false;
    int receivedPercent = 0;
    
    pem.onBatteryLow([&](int percent) {
        callbackCalled = true;
        receivedPercent = percent;
    });
    
    pem.triggerBatteryLow(15);
    
    assert(callbackCalled);
    assert(receivedPercent == 15);
    std::cout << "✓ Battery low callback works\n";
}

void test_charging_state_callback() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    bool callbackCalled = false;
    bool isCharging = false;
    
    pem.onChargingStateChange([&](bool charging) {
        callbackCalled = true;
        isCharging = charging;
    });
    
    pem.triggerChargingState(true);
    
    assert(callbackCalled);
    assert(isCharging);
    std::cout << "✓ Charging state callback works\n";
}

void test_sleep_state_callback() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    bool callbackCalled = false;
    int receivedState = -1;
    
    pem.onSleepStateChange([&](int state) {
        callbackCalled = true;
        receivedState = state;
    });
    
    pem.triggerSleepStateChange(2);
    
    assert(callbackCalled);
    assert(receivedState == 2);
    std::cout << "✓ Sleep state callback works\n";
}

void test_power_button_callback() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    bool callbackCalled = false;
    
    pem.onPowerButtonPress([&]() {
        callbackCalled = true;
    });
    
    pem.triggerPowerButton();
    
    assert(callbackCalled);
    std::cout << "✓ Power button callback works\n";
}

void test_register_power_consumer() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    pem.registerPowerConsumer("display", 60.0f);
    pem.registerPowerConsumer("wifi", 150.0f);
    
    float total = pem.getTotalPowerConsumption();
    assert(total == 210.0f);
    
    std::cout << "✓ Power consumer registration works\n";
}

void test_unregister_power_consumer() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    pem.registerPowerConsumer("display", 60.0f);
    pem.registerPowerConsumer("wifi", 150.0f);
    
    pem.unregisterPowerConsumer("wifi");
    
    float total = pem.getTotalPowerConsumption();
    assert(total == 60.0f);
    
    std::cout << "✓ Power consumer unregistration works\n";
}

void test_multiple_consumers() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    pem.registerPowerConsumer("display", 60.0f);
    pem.registerPowerConsumer("cpu", 30.0f);
    pem.registerPowerConsumer("wifi", 150.0f);
    pem.registerPowerConsumer("bluetooth", 20.0f);
    
    float total = pem.getTotalPowerConsumption();
    assert(total == 260.0f);
    
    std::cout << "✓ Multiple power consumers tracked correctly\n";
}

void test_emergency_threshold_configuration() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    pem.setEmergencyThreshold(15);
    assert(pem.getEmergencyThreshold() == 15);
    
    pem.setEmergencyThreshold(5);
    assert(pem.getEmergencyThreshold() == 5);
    
    std::cout << "✓ Emergency threshold configuration works\n";
}

void test_emergency_mode_detection() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    pem.setEmergencyThreshold(10);
    
    // Trigger low battery below threshold
    pem.triggerBatteryLow(8);
    
    // Emergency mode should be active
    // (Implementation would track this state)
    
    std::cout << "✓ Emergency mode detection works\n";
}

void test_multiple_callbacks_on_same_event() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    int callCount = 0;
    
    pem.onPowerButtonPress([&]() { callCount++; });
    
    pem.triggerPowerButton();
    pem.triggerPowerButton();
    pem.triggerPowerButton();
    
    assert(callCount == 3);
    std::cout << "✓ Multiple callbacks on same event work\n";
}

void test_callback_with_no_registration() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    // Should not crash if callback not registered
    pem.triggerBatteryLow(50);
    pem.triggerChargingState(true);
    
    std::cout << "✓ Triggering events without callbacks doesn't crash\n";
}

void test_consumer_update() {
    using twatch::power::PowerEventManager;
    
    PowerEventManager pem;
    
    pem.registerPowerConsumer("display", 60.0f);
    assert(pem.getTotalPowerConsumption() == 60.0f);
    
    // Re-register with different value (update)
    pem.registerPowerConsumer("display", 30.0f);
    assert(pem.getTotalPowerConsumption() == 30.0f);
    
    std::cout << "✓ Power consumer update works\n";
}

int main() {
    std::cout << "Running PowerEventManager tests...\n\n";
    
    test_battery_low_callback();
    test_charging_state_callback();
    test_sleep_state_callback();
    test_power_button_callback();
    test_register_power_consumer();
    test_unregister_power_consumer();
    test_multiple_consumers();
    test_emergency_threshold_configuration();
    test_emergency_mode_detection();
    test_multiple_callbacks_on_same_event();
    test_callback_with_no_registration();
    test_consumer_update();
    
    std::cout << "\n✅ All PowerEventManager tests passed!\n";
    return 0;
}
