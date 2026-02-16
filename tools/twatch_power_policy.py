from enum import Enum, auto


class BoardType(Enum):
    V1 = auto()
    V2 = auto()
    V3 = auto()
    OTHER = auto()


class Rail(Enum):
    NONE = auto()
    LDO2 = auto()
    LDO3 = auto()
    LDO4 = auto()


def keep_display_rail_enabled_during_init(board: BoardType) -> bool:
    return board == BoardType.V1


def toggle_backlight_rail_via_power_ic(board: BoardType) -> bool:
    return board != BoardType.V1


def audio_power_rail(board: BoardType) -> Rail:
    if board == BoardType.V2:
        return Rail.LDO3
    if board == BoardType.V3:
        return Rail.LDO4
    return Rail.NONE


def gps_power_rail(board: BoardType) -> Rail:
    if board == BoardType.V2:
        return Rail.LDO4
    return Rail.LDO3
