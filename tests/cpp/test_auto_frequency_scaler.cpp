#include <cassert>
#include <iostream>
#include <cstdint>

// Forward declarations for testing
namespace twatch {
namespace power {

class AutoFrequencyScaler {
public:
    AutoFrequencyScaler() 
        : wifiActive(false)
        , animationActive(false)
        , displayActive(true)
        , currentFrequencyMhz(80) {}
    
    // Notify state changes
    void notifyWiFiStateChange(bool active);
    void notifyAnimationStateChange(bool active);
    void notifyDisplayStateChange(bool active);
    
    // Manual frequency request (high priority)
    void requestFrequency(uint32_t mhz, const char* reason);
    void releaseFrequencyRequest();
    
    // Query current frequency
    uint32_t getCurrentFrequencyMhz() const { return currentFrequencyMhz; }
    
    // Get recommended frequency based on current state
    uint32_t getRecommendedFrequencyMhz() const;
    
    // Apply frequency changes
    void applyFrequencyChange();
    
    // Statistics
    uint32_t getFrequencyChangeCount() const { return frequencyChangeCount; }
    void resetStatistics();
    
private:
    bool wifiActive;
    bool animationActive;
    bool displayActive;
    uint32_t currentFrequencyMhz;
    uint32_t frequencyChangeCount = 0;
    uint32_t manualFrequencyRequest = 0;
    
    void reevaluateFrequency();
};

} // namespace power
} // namespace twatch

// Test implementations
void test_default_frequency() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    // Default should be 80 MHz (balanced)
    assert(afs.getCurrentFrequencyMhz() == 80);
    std::cout << "✓ Default frequency is 80 MHz\n";
}

void test_wifi_requires_high_frequency() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.notifyWiFiStateChange(true);
    uint32_t freq = afs.getRecommendedFrequencyMhz();
    
    // WiFi should require 160 MHz or higher
    assert(freq >= 160);
    std::cout << "✓ WiFi active requires 160+ MHz\n";
}

void test_wifi_off_reduces_frequency() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.notifyWiFiStateChange(true);
    afs.applyFrequencyChange();
    uint32_t highFreq = afs.getCurrentFrequencyMhz();
    
    afs.notifyWiFiStateChange(false);
    afs.applyFrequencyChange();
    uint32_t lowFreq = afs.getCurrentFrequencyMhz();
    
    assert(lowFreq < highFreq);
    std::cout << "✓ WiFi off reduces frequency\n";
}

void test_display_off_minimum_frequency() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.notifyDisplayStateChange(false);
    uint32_t freq = afs.getRecommendedFrequencyMhz();
    
    // Display off should allow minimum frequency (20 MHz)
    assert(freq <= 40);
    std::cout << "✓ Display off allows minimum frequency\n";
}

void test_animation_increases_frequency() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    // Static display
    afs.notifyAnimationStateChange(false);
    uint32_t staticFreq = afs.getRecommendedFrequencyMhz();
    
    // Animation active
    afs.notifyAnimationStateChange(true);
    uint32_t animFreq = afs.getRecommendedFrequencyMhz();
    
    assert(animFreq >= staticFreq);
    std::cout << "✓ Animation increases frequency\n";
}

void test_manual_frequency_request() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.requestFrequency(240, "test_operation");
    uint32_t freq = afs.getRecommendedFrequencyMhz();
    
    assert(freq == 240);
    std::cout << "✓ Manual frequency request overrides automatic scaling\n";
}

void test_release_frequency_request() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.requestFrequency(240, "test_operation");
    afs.releaseFrequencyRequest();
    
    uint32_t freq = afs.getRecommendedFrequencyMhz();
    
    // Should return to automatic frequency
    assert(freq < 240);
    std::cout << "✓ Release frequency request returns to automatic scaling\n";
}

void test_frequency_change_tracking() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    uint32_t initialCount = afs.getFrequencyChangeCount();
    
    afs.notifyWiFiStateChange(true);
    afs.applyFrequencyChange();
    
    uint32_t afterChange = afs.getFrequencyChangeCount();
    
    assert(afterChange > initialCount);
    std::cout << "✓ Frequency changes are tracked\n";
}

void test_statistics_reset() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    afs.notifyWiFiStateChange(true);
    afs.applyFrequencyChange();
    afs.notifyWiFiStateChange(false);
    afs.applyFrequencyChange();
    
    afs.resetStatistics();
    
    assert(afs.getFrequencyChangeCount() == 0);
    std::cout << "✓ Statistics reset works\n";
}

void test_priority_hierarchy() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    // Manual request should override wifi
    afs.notifyWiFiStateChange(true);
    afs.requestFrequency(240, "manual_test");
    
    uint32_t freq = afs.getRecommendedFrequencyMhz();
    assert(freq == 240);
    
    // After release, WiFi should take effect
    afs.releaseFrequencyRequest();
    freq = afs.getRecommendedFrequencyMhz();
    assert(freq >= 160);
    
    std::cout << "✓ Priority hierarchy works (manual > wifi > display)\n";
}

void test_multiple_state_combinations() {
    using twatch::power::AutoFrequencyScaler;
    
    AutoFrequencyScaler afs;
    
    // WiFi + Animation should be high frequency
    afs.notifyWiFiStateChange(true);
    afs.notifyAnimationStateChange(true);
    uint32_t freq1 = afs.getRecommendedFrequencyMhz();
    assert(freq1 >= 160);
    
    // No WiFi, animation only - moderate frequency
    afs.notifyWiFiStateChange(false);
    afs.notifyAnimationStateChange(true);
    uint32_t freq2 = afs.getRecommendedFrequencyMhz();
    assert(freq2 < freq1);
    
    // Everything off - minimum frequency
    afs.notifyAnimationStateChange(false);
    afs.notifyDisplayStateChange(false);
    uint32_t freq3 = afs.getRecommendedFrequencyMhz();
    assert(freq3 <= freq2);
    
    std::cout << "✓ Multiple state combinations work correctly\n";
}

int main() {
    std::cout << "Running AutoFrequencyScaler tests...\n\n";
    
    test_default_frequency();
    test_wifi_requires_high_frequency();
    test_wifi_off_reduces_frequency();
    test_display_off_minimum_frequency();
    test_animation_increases_frequency();
    test_manual_frequency_request();
    test_release_frequency_request();
    test_frequency_change_tracking();
    test_statistics_reset();
    test_priority_hierarchy();
    test_multiple_state_combinations();
    
    std::cout << "\n✅ All AutoFrequencyScaler tests passed!\n";
    return 0;
}
