#include <cassert>
#include <iostream>

// Forward declarations for testing
namespace twatch {
namespace power {

enum class BoardType { V1, V2, V3, Other };

class PowerDomainAbstraction {
public:
    explicit PowerDomainAbstraction(BoardType board) : boardType(board) {}
    
    // Power control methods
    bool setDisplayPower(bool enable);
    bool setAudioPower(bool enable);
    bool setSensorPower(bool enable);
    bool setBacklightPower(bool enable);
    
    // Capability queries
    bool canPowerGateDisplay() const;
    bool canPowerGateSensors() const;
    bool canPowerGateBacklight() const;
    
    // Optimal power sequences
    void enterLowPowerMode();
    void exitLowPowerMode();
    
    BoardType getBoardType() const { return boardType; }
    
private:
    BoardType boardType;
};

} // namespace power
} // namespace twatch

// Test implementations
void test_v1_cannot_power_gate_display() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V1);
    assert(!pda.canPowerGateDisplay());
    std::cout << "✓ V1 correctly reports display cannot be power gated\n";
}

void test_v2_can_power_gate_display() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V2);
    assert(pda.canPowerGateDisplay());
    std::cout << "✓ V2 correctly reports display can be power gated\n";
}

void test_v3_can_power_gate_display() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V3);
    assert(pda.canPowerGateDisplay());
    std::cout << "✓ V3 correctly reports display can be power gated\n";
}

void test_v1_backlight_via_pwm_only() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V1);
    assert(!pda.canPowerGateBacklight());
    std::cout << "✓ V1 correctly reports backlight is PWM only\n";
}

void test_v2_v3_backlight_via_power_ic() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
   PowerDomainAbstraction pda_v2(BoardType::V2);
    PowerDomainAbstraction pda_v3(BoardType::V3);
    
    assert(pda_v2.canPowerGateBacklight());
    assert(pda_v3.canPowerGateBacklight());
    std::cout << "✓ V2/V3 correctly report backlight can be power controlled\n";
}

void test_low_power_mode_sequence() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    // Test each board type
    for (auto board : {BoardType::V1, BoardType::V2, BoardType::V3, BoardType::Other}) {
        PowerDomainAbstraction pda(board);
        
        // Should not throw or crash
        pda.enterLowPowerMode();
        pda.exitLowPowerMode();
    }
    std::cout << "✓ Low power mode sequences work for all board types\n";
}

void test_audio_power_control() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V2);
    
    // Should be able to enable/disable audio
    assert(pda.setAudioPower(true));
    assert(pda.setAudioPower(false));
    std::cout << "✓ Audio power control works\n";
}

void test_sensor_power_control() {
    using twatch::power::PowerDomainAbstraction;
    using twatch::power::BoardType;
    
    PowerDomainAbstraction pda(BoardType::V1);
    
    // V1 sensors are always powered, but API should work
    pda.setSensorPower(true);
    pda.setSensorPower(false);
    std::cout << "✓ Sensor power control API works\n";
}

int main() {
    std::cout << "Running PowerDomainAbstraction tests...\n\n";
    
    test_v1_cannot_power_gate_display();
    test_v2_can_power_gate_display();
    test_v3_can_power_gate_display();
    test_v1_backlight_via_pwm_only();
    test_v2_v3_backlight_via_power_ic();
    test_low_power_mode_sequence();
    test_audio_power_control();
    test_sensor_power_control();
    
    std::cout << "\n✅ All PowerDomainAbstraction tests passed!\n";
    return 0;
}
