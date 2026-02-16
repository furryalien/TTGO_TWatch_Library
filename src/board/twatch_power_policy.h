#pragma once

namespace twatch {
namespace power {

enum class BoardType {
    V1,
    V2,
    V3,
    Other
};

enum class Rail {
    None,
    LDO2,
    LDO3,
    LDO4
};

constexpr BoardType currentBoard()
{
#if defined(LILYGO_WATCH_2020_V1)
    return BoardType::V1;
#elif defined(LILYGO_WATCH_2020_V2)
    return BoardType::V2;
#elif defined(LILYGO_WATCH_2020_V3)
    return BoardType::V3;
#else
    return BoardType::Other;
#endif
}

constexpr bool keepDisplayRailEnabledDuringInit(BoardType board)
{
    return board == BoardType::V1;
}

constexpr bool toggleBacklightRailViaPowerIC(BoardType board)
{
    return board != BoardType::V1;
}

constexpr Rail audioPowerRail(BoardType board)
{
    return board == BoardType::V2 ? Rail::LDO3 :
           (board == BoardType::V3 ? Rail::LDO4 : Rail::None);
}

constexpr Rail gpsPowerRail(BoardType board)
{
    return board == BoardType::V2 ? Rail::LDO4 : Rail::LDO3;
}

} // namespace power
} // namespace twatch
