#pragma once

#include "twatch_power_policy.h"

namespace twatch {
namespace power {

/**
 * @brief Hardware abstraction for power control across different watch versions
 * 
 * Provides unified power control API that works across all hardware versions,
 * handling hardware-specific differences internally.
 */
class PowerDomainAbstraction {
public:
    explicit PowerDomainAbstraction(BoardType board) : boardType(board) {}
    
    /**
     * @brief Set display power state
     * @param enable true to power on, false to power off
     * @return true if successful, false if not supported by hardware
     */
    bool setDisplayPower(bool enable) {
        // V1: Display always powered, cannot be gated
        if (boardType == BoardType::V1) {
            return false;  // Not supported
        }
        
        // V2/V3: Can control display power via LDO2
        // Implementation would call AXP202 setPowerOutPut
        return true;  // Supported
    }
    
    /**
     * @brief Set audio module power state
     * @param enable true to power on, false to power off
     * @return true if successful
     */
    bool setAudioPower(bool enable) {
        Rail audioRail = audioPowerRail(boardType);
        
        if (audioRail == Rail::None) {
            return false;  // V1 has no audio power control
        }
        
        // Would call AXP202 setPowerOutPut with appropriate LDO
        return true;
    }
    
    /**
     * @brief Set sensor power state
     * @param enable true to power on, false to power off
     * @return true if operation attempted (may not be supported)
     */
    bool setSensorPower(bool enable) {
        // V1: Sensors always powered
        // V2/V3: May have sensor power control
        // Implementation would depend on hardware version
        return true;  // Always returns true, may be no-op on V1
    }
    
    /**
     * @brief Set backlight power state
     * @param enable true to power on, false to power off
     * @return true if successful
     */
    bool setBacklightPower(bool enable) {
        if (boardType == BoardType::V1) {
            // V1 uses PWM only, no power IC control
            return false;
        }
        
        // V2/V3 have LDO2 backlight control
        return true;
    }
    
    /**
     * @brief Query if display can be power gated
     */
    bool canPowerGateDisplay() const {
        return !keepDisplayRailEnabledDuringInit(boardType);
    }
    
    /**
     * @brief Query if sensors can be power gated
     */
    bool canPowerGateSensors() const {
        // V1: sensors always powered
        return boardType != BoardType::V1;
    }
    
    /**
     * @brief Query if backlight can be power controlled via IC
     */
    bool canPowerGateBacklight() const {
        return toggleBacklightRailViaPowerIC(boardType);
    }
    
    /**
     * @brief Enter low power mode with optimal sequence for this hardware
     */
    void enterLowPowerMode() {
        // Hardware-specific power-down sequence
        if (canPowerGateDisplay()) {
            setDisplayPower(false);
        }
        
        // Disable audio on all versions
        setAudioPower(false);
        
        // Additional hardware-specific optimizations
        switch (boardType) {
            case BoardType::V1:
                // V1 specific: only PWM and LDO control
                break;
            case BoardType::V2:
                // V2 specific optimizations
                break;
            case BoardType::V3:
                // V3 specific optimizations
                break;
            default:
                break;
        }
    }
    
    /**
     * @brief Exit low power mode with optimal sequence for this hardware
     */
    void exitLowPowerMode() {
        // Hardware-specific power-up sequence
        if (canPowerGateDisplay()) {
            setDisplayPower(true);
            // Allow power to stabilize
            delay(10);
        }
        
        // Audio remains off until explicitly enabled
    }
    
    BoardType getBoardType() const { return boardType; }
    
private:
    BoardType boardType;
};

} // namespace power
} // namespace twatch
