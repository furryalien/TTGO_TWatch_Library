#include <cassert>
#include <iostream>
#include <cstdint>
#include <cmath>

//Forward declarations for testing
namespace twatch {
namespace power {

class BatteryStateEstimator {
public:
    BatteryStateEstimator() 
        : capacityMah(350.0f)
        , lastFullChargeTimestamp(0)
        , averageDischargeCurrentMa(0.0f)
        , cycleCount(0)
        , totalDischargedMah(0.0f) {}
    
    // Configuration
    void setCapacity(float mah);
    float getCapacity() const { return capacityMah; }
    
    // Intelligent estimation
    uint32_t estimateRemainingSeconds(float currentMa, float batteryPercent);
    uint32_t estimateTimeToFullChargeSeconds(float chargingCurrentMa, float batteryPercent);
    float estimateAverageDischargeRate() const;  // mA
    
    // Track state
    void notifyFullCharge();
    void updateDischargeRate(float mA);
    
    // Historical data
    uint32_t getTimeSinceLastFullCharge(uint32_t currentTimestamp) const;  // seconds
    float getAverageBatteryLifeHours() const;  // over last charges
    
    // Health monitoring
    float estimateBatteryHealth() const;  // 0-100%
    uint32_t getCycleCount() const { return cycleCount; }
    
    // Reset/calibration
    void resetStatistics();
    void calibrate(float actualCapacityMah);
    
private:
    float capacityMah;
    uint32_t lastFullChargeTimestamp;
    float averageDischargeCurrentMa;
    uint32_t cycleCount;
    float totalDischargedMah;
};

} // namespace power
} // namespace twatch

// Test implementations
void test_default_capacity() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    // Default should be 350mAh
    assert(bse.getCapacity() == 350.0f);
    std::cout << "✓ Default battery capacity is 350mAh\n";
}

void test_set_capacity() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    bse.setCapacity(500.0f);
    assert(bse.getCapacity() == 500.0f);
    
    std::cout << "✓ Set battery capacity works\n";
}

void test_estimate_remaining_time() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    // At 50% battery with 35mA drain
    // Should have ~175mAh remaining
    // At 35mA, that's 5 hours = 18000 seconds
    uint32_t remaining = bse.estimateRemainingSeconds(35.0f, 50.0f);
    
    // Should be approximately 5 hours (18000s), allow 10% margin
    assert(remaining > 16200 && remaining < 19800);
    std::cout << "✓ Remaining time estimation works\n";
}

void test_estimate_charge_time() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    // At 50% battery with 300mA charging
    // Need to charge 175mAh
    // At 300mA, that's ~0.583 hours = 2100 seconds
    uint32_t chargeTime = bse.estimateTimeToFullChargeSeconds(300.0f, 50.0f);
    
    // Should be approximately 35 minutes (2100s), allow 20% margin
    assert(chargeTime > 1680 && chargeTime < 2520);
    std::cout << "✓ Charge time estimation works\n";
}

void test_full_charge_notification() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    uint32_t initialCycles = bse.getCycleCount();
    
    bse.notifyFullCharge();
    
    uint32_t afterCycles = bse.getCycleCount();
    assert(afterCycles == initialCycles + 1);
    
    std::cout << "✓ Full charge notification increments cycle count\n";
}

void test_discharge_rate_tracking() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    bse.updateDischargeRate(40.0f);
    bse.updateDischargeRate(35.0f);
    bse.updateDischargeRate(45.0f);
    
    float avgRate = bse.estimateAverageDischargeRate();
    
    // Average should be around 40mA
    assert(avgRate > 35.0f && avgRate < 45.0f);
    std::cout << "✓ Discharge rate tracking works\n";
}

void test_time_since_last_full_charge() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    bse.notifyFullCharge();
    
    // Simulate 3600 seconds passing
    uint32_t timeSince = bse.getTimeSinceLastFullCharge(3600);
    
    assert(timeSince == 3600);
    std::cout << "✓ Time since last full charge tracking works\n";
}

void test_battery_health_estimation() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    float health = bse.estimateBatteryHealth();
    
    // Health should be 0-100%
    assert(health >= 0.0f && health <= 100.0f);
    std::cout << "✓ Battery health estimation returns valid range\n";
}

void test_battery_health_degrades_with_cycles() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    float initialHealth = bse.estimateBatteryHealth();
    
    // Simulate many charge cycles
    for (int i = 0; i < 100; i++) {
        bse.notifyFullCharge();
    }
    
    float degradedHealth = bse.estimateBatteryHealth();
    
    // Health should degrade after many cycles
    assert(degradedHealth <= initialHealth);
    std::cout << "✓ Battery health degrades with charge cycles\n";
}

void test_cycle_count_tracking() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    assert(bse.getCycleCount() == 0);
    
    bse.notifyFullCharge();
    assert(bse.getCycleCount() == 1);
    
    bse.notifyFullCharge();
    bse.notifyFullCharge();
    assert(bse.getCycleCount() == 3);
    
    std::cout << "✓ Cycle count tracking works\n";
}

void test_reset_statistics() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    bse.notifyFullCharge();
    bse.notifyFullCharge();
    bse.updateDischargeRate(40.0f);
    
    bse.resetStatistics();
    
    assert(bse.getCycleCount() == 0);
    assert(bse.estimateAverageDischargeRate() == 0.0f);
    
    std::cout << "✓ Reset statistics works\n";
}

void test_calibration() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    // Calibrate with actual measured capacity
    bse.calibrate(320.0f);
    
    // Capacity should be updated
    assert(bse.getCapacity() == 320.0f);
    
    std::cout << "✓ Battery calibration works\n";
}

void test_average_battery_life() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    bse.updateDischargeRate(40.0f);
    bse.updateDischargeRate(35.0f);
    
    float avgLife = bse.getAverageBatteryLifeHours();
    
    // Should return reasonable value
    assert(avgLife > 0.0f && avgLife < 1000.0f);
    std::cout << "✓ Average battery life calculation works\n";
}

void test_zero_current_edge_case() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    
    // With zero current, time should be very large or infinity
    uint32_t remaining = bse.estimateRemainingSeconds(0.0f, 50.0f);
    
    // Should handle gracefully (not crash)
    assert(remaining > 0);
    std::cout << "✓ Zero current edge case handled\n";
}

void test_full_battery_remaining_time() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    // At 100% battery with 35mA drain
    uint32_t remaining = bse.estimateRemainingSeconds(35.0f, 100.0f);
    
    // Should be ~10 hours = 36000 seconds
    assert(remaining > 32400 && remaining < 39600);
    std::cout << "✓ Full battery remaining time estimation works\n";
}

void test_low_battery_remaining_time() {
    using twatch::power::BatteryStateEstimator;
    
    BatteryStateEstimator bse;
    bse.setCapacity(350.0f);
    
    // At 10% battery with 35mA drain
    uint32_t remaining = bse.estimateRemainingSeconds(35.0f, 10.0f);
    
    // Should be ~1 hour = 3600 seconds
    assert(remaining > 2880 && remaining < 4320);
    std::cout << "✓ Low battery remaining time estimation works\n";
}

int main() {
    std::cout << "Running BatteryStateEstimator tests...\n\n";
    
    test_default_capacity();
    test_set_capacity();
    test_estimate_remaining_time();
    test_estimate_charge_time();
    test_full_charge_notification();
    test_discharge_rate_tracking();
    test_time_since_last_full_charge();
    test_battery_health_estimation();
    test_battery_health_degrades_with_cycles();
    test_cycle_count_tracking();
    test_reset_statistics();
    test_calibration();
    test_average_battery_life();
    test_zero_current_edge_case();
    test_full_battery_remaining_time();
    test_low_battery_remaining_time();
    
    std::cout << "\n✅ All BatteryStateEstimator tests passed!\n";
    return 0;
}
