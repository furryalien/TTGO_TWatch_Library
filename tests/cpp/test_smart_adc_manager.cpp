#include <cassert>
#include <iostream>
#include <cstdint>

// Forward declarations for testing
namespace twatch {
namespace power {

class SmartADCManager {
public:
    SmartADCManager() : activeMonitors(0), samplingRate(200) {}
    
    // ADC channel masks (matching AXP202)
    static constexpr uint8_t BATT_VOL = 0x80;
    static constexpr uint8_t BATT_CUR = 0x40;
    static constexpr uint8_t VBUS_VOL = 0x08;
   static constexpr uint8_t VBUS_CUR = 0x04;
    static constexpr uint8_t TEMP_SENSOR = 0x01;
    
    // Enable specific ADC channels
    void enableMonitoring(uint8_t adcMask);
    void disableMonitoring(uint8_t adcMask);
    
    // High-level API
    void enableBatteryMonitoring();
    void disableBatteryMonitoring();
    void enableChargingMonitoring();
    void disableChargingMonitoring();
    void enableTemperatureMonitoring();
    void disableTemperatureMonitoring();
    
    // Query state
    bool isMonitoring(uint8_t adcMask) const;
    uint8_t getActiveMonitors() const { return activeMonitors; }
    uint32_t getSamplingRate() const { return samplingRate; }
    
    // Automatically optimize sampling rate based on usage
    void optimizeSamplingRate();
    
    // Estimate power savings
    float estimatePowerSavings() const;
    
private:
    uint8_t activeMonitors;
    uint32_t samplingRate;  // Hz
};

} // namespace power
} // namespace twatch

// Test implementations
void test_initial_state_no_monitors() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    assert(adc.getActiveMonitors() == 0);
    std::cout << "✓ Initial state has no monitors enabled\n";
}

void test_enable_battery_monitoring() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    adc.enableBatteryMonitoring();
    
    assert(adc.isMonitoring(SmartADCManager::BATT_VOL));
    assert(adc.isMonitoring(SmartADCManager::BATT_CUR));
    std::cout << "✓ Battery monitoring enables voltage and current ADCs\n";
}

void test_enable_charging_monitoring() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    adc.enableChargingMonitoring();
    
    assert(adc.isMonitoring(SmartADCManager::VBUS_VOL));
    assert(adc.isMonitoring(SmartADCManager::VBUS_CUR));
    std::cout << "✓ Charging monitoring enables VBUS voltage and current ADCs\n";
}

void test_disable_monitoring() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    adc.enableBatteryMonitoring();
    adc.disableBatteryMonitoring();
    
    assert(!adc.isMonitoring(SmartADCManager::BATT_VOL));
    assert(!adc.isMonitoring(SmartADCManager::BATT_CUR));
    std::cout << "✓ Disabling monitoring works correctly\n";
}

void test_selective_adc_enable() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    adc.enableMonitoring(SmartADCManager::BATT_VOL);
    
    assert(adc.isMonitoring(SmartADCManager::BATT_VOL));
    assert(!adc.isMonitoring(SmartADCManager::BATT_CUR));
    std::cout << "✓ Selective ADC channel enable works\n";
}

void test_temperature_sensor_control() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    adc.enableTemperatureMonitoring();
    
    assert(adc.isMonitoring(SmartADCManager::TEMP_SENSOR));
    
    adc.disableTemperatureMonitoring();
    assert(!adc.isMonitoring(SmartADCManager::TEMP_SENSOR));
    std::cout << "✓ Temperature sensor can be controlled independently\n";
}

void test_sampling_rate_optimization() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    
    // Default should be 200Hz
    assert(adc.getSamplingRate() == 200);
    
    // With monitoring enabled, should optimize to lower rate
    adc.enableBatteryMonitoring();
    adc.optimizeSamplingRate();
    
    // Should reduce to 25Hz for battery-only monitoring
    assert(adc.getSamplingRate() <= 25);
    std::cout << "✓ Sampling rate optimization reduces frequency for battery monitoring\n";
}

void test_power_savings_estimation() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc_all;
    adc_all.enableBatteryMonitoring();
    adc_all.enableChargingMonitoring();
    adc_all.enableTemperatureMonitoring();
    
    SmartADCManager adc_minimal;
    adc_minimal.enableBatteryMonitoring();
    adc_minimal.optimizeSamplingRate();
    
    float savings = adc_all.estimatePowerSavings();
    float minimalSavings = adc_minimal.estimatePowerSavings();
    
    // Minimal monitoring should have higher savings
    assert(minimalSavings > savings);
    std::cout << "✓ Power savings estimation correctly shows more savings with fewer monitors\n";
}

void test_multiple_enable_disable_cycles() {
    using twatch::power::SmartADCManager;
    
    SmartADCManager adc;
    
    // Enable/disable multiple times
    for (int i = 0; i < 5; i++) {
        adc.enableBatteryMonitoring();
        assert(adc.isMonitoring(SmartADCManager::BATT_VOL));
        
        adc.disableBatteryMonitoring();
        assert(!adc.isMonitoring(SmartADCManager::BATT_VOL));
    }
    std::cout << "✓ Multiple enable/disable cycles work correctly\n";
}

int main() {
    std::cout << "Running SmartADCManager tests...\n\n";
    
    test_initial_state_no_monitors();
    test_enable_battery_monitoring();
    test_enable_charging_monitoring();
    test_disable_monitoring();
    test_selective_adc_enable();
    test_temperature_sensor_control();
    test_sampling_rate_optimization();
    test_power_savings_estimation();
    test_multiple_enable_disable_cycles();
    
    std::cout << "\n✅ All SmartADCManager tests passed!\n";
    return 0;
}
