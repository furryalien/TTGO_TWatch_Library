#include <cassert>
#include <iostream>

#include "../../src/board/twatch_power_policy.h"

using twatch::power::BoardType;
using twatch::power::Rail;

static void test_common_policy_contracts()
{
    assert(twatch::power::keepDisplayRailEnabledDuringInit(BoardType::V1));
    assert(!twatch::power::keepDisplayRailEnabledDuringInit(BoardType::V2));
    assert(!twatch::power::keepDisplayRailEnabledDuringInit(BoardType::V3));

    assert(!twatch::power::toggleBacklightRailViaPowerIC(BoardType::V1));
    assert(twatch::power::toggleBacklightRailViaPowerIC(BoardType::V2));
    assert(twatch::power::toggleBacklightRailViaPowerIC(BoardType::V3));

    assert(twatch::power::audioPowerRail(BoardType::V1) == Rail::None);
    assert(twatch::power::audioPowerRail(BoardType::V2) == Rail::LDO3);
    assert(twatch::power::audioPowerRail(BoardType::V3) == Rail::LDO4);

    assert(twatch::power::gpsPowerRail(BoardType::V1) == Rail::LDO3);
    assert(twatch::power::gpsPowerRail(BoardType::V2) == Rail::LDO4);
    assert(twatch::power::gpsPowerRail(BoardType::V3) == Rail::LDO3);
}

int main()
{
    test_common_policy_contracts();

#if defined(LILYGO_WATCH_2020_V1)
    static_assert(twatch::power::currentBoard() == BoardType::V1, "Expected V1 board policy");
    static_assert(twatch::power::keepDisplayRailEnabledDuringInit(twatch::power::currentBoard()), "V1 display rail should remain enabled");
#elif defined(LILYGO_WATCH_2020_V2)
    static_assert(twatch::power::currentBoard() == BoardType::V2, "Expected V2 board policy");
    static_assert(twatch::power::audioPowerRail(twatch::power::currentBoard()) == Rail::LDO3, "V2 audio should use LDO3");
    static_assert(twatch::power::gpsPowerRail(twatch::power::currentBoard()) == Rail::LDO4, "V2 GPS should use LDO4");
#elif defined(LILYGO_WATCH_2020_V3)
    static_assert(twatch::power::currentBoard() == BoardType::V3, "Expected V3 board policy");
    static_assert(twatch::power::audioPowerRail(twatch::power::currentBoard()) == Rail::LDO4, "V3 audio should use LDO4");
#else
    static_assert(twatch::power::currentBoard() == BoardType::Other, "Expected fallback board policy");
#endif

    std::cout << "twatch_power_policy_test: PASS" << std::endl;
    return 0;
}