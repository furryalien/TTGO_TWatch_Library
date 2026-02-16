from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
TTGO_HEADER = REPO_ROOT / "src" / "TTGO.h"


def _extract_method_block(content: str, method_name: str) -> str:
    marker = f"void {method_name}("
    start = content.find(marker)
    assert start != -1, f"Method not found: {method_name}"
    open_brace = content.find("{", start)
    assert open_brace != -1, f"Method body not found: {method_name}"

    depth = 0
    for index in range(open_brace, len(content)):
        char = content[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return content[open_brace : index + 1]

    raise AssertionError(f"Unclosed method block for: {method_name}")


def test_enable_audio_uses_policy_switch():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    block = _extract_method_block(content, "enableAudio")
    assert "audioPowerRail" in block
    assert "AXP202_LDO3" in block
    assert "AXP202_LDO4" in block


def test_gps_methods_use_policy_rail_selection():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    on_block = _extract_method_block(content, "trunOnGPS")
    off_block = _extract_method_block(content, "turnOffGPS")
    assert "gpsPowerRail" in on_block
    assert "gpsPowerRail" in off_block
    assert "AXP202_LDO4" in on_block and "AXP202_LDO3" in on_block
    assert "AXP202_LDO4" in off_block and "AXP202_LDO3" in off_block


def test_backlight_methods_guard_axp_toggle_with_policy():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    open_block = _extract_method_block(content, "openBL")
    close_block = _extract_method_block(content, "closeBL")
    assert "toggleBacklightRailViaPowerIC" in open_block
    assert "toggleBacklightRailViaPowerIC" in close_block
    assert "#ifdef LILYGO_WATCH_HAS_AXP202" in open_block
    assert "#ifdef LILYGO_WATCH_HAS_AXP202" in close_block


def test_init_power_keeps_v1_display_rail_enabled_by_policy():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    init_power_block = _extract_method_block(content, "initPower")
    assert "keepDisplayRailEnabledDuringInit" in init_power_block
    assert "AXP202_LDO2" in init_power_block