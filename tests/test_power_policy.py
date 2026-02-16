from tools.twatch_power_policy import (
    BoardType,
    Rail,
    audio_power_rail,
    gps_power_rail,
    keep_display_rail_enabled_during_init,
    toggle_backlight_rail_via_power_ic,
)


def test_backlight_policy():
    assert toggle_backlight_rail_via_power_ic(BoardType.V1) is False
    assert toggle_backlight_rail_via_power_ic(BoardType.V2) is True
    assert toggle_backlight_rail_via_power_ic(BoardType.V3) is True


def test_init_display_rail_policy():
    assert keep_display_rail_enabled_during_init(BoardType.V1) is True
    assert keep_display_rail_enabled_during_init(BoardType.V2) is False
    assert keep_display_rail_enabled_during_init(BoardType.OTHER) is False


def test_audio_rail_policy():
    assert audio_power_rail(BoardType.V1) == Rail.NONE
    assert audio_power_rail(BoardType.V2) == Rail.LDO3
    assert audio_power_rail(BoardType.V3) == Rail.LDO4


def test_gps_rail_policy():
    assert gps_power_rail(BoardType.V1) == Rail.LDO3
    assert gps_power_rail(BoardType.V2) == Rail.LDO4
    assert gps_power_rail(BoardType.V3) == Rail.LDO3
