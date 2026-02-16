from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
TTGO_HEADER = REPO_ROOT / "src" / "TTGO.h"
POLICY_HEADER = REPO_ROOT / "src" / "board" / "twatch_power_policy.h"


def test_policy_header_exists():
    assert POLICY_HEADER.exists()


def test_ttgo_uses_policy_header():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    assert '#include "board/twatch_power_policy.h"' in content


def test_ttgo_uses_policy_functions():
    content = TTGO_HEADER.read_text(encoding="utf-8")
    assert "audioPowerRail" in content
    assert "gpsPowerRail" in content
    assert "toggleBacklightRailViaPowerIC" in content
    assert "keepDisplayRailEnabledDuringInit" in content
