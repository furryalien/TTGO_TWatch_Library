# TTGO T-Watch Test Coverage Analysis (2020-V1 Context)

**Date:** 2026-02-15  
**Document Version:** 1.3  
**Scope requested:** Entire repository, analyzed in the context of **T-Watch 2020 V1**  
**Coverage goal requested:** **100% line + branch coverage**  
**Test types counted:** Unit tests (host, mocked hardware) + integration tests

---

## 1) Executive Summary

Testing moved from **manual-demo heavy** to an **initial automated baseline**.

- A repository-level workflow now exists at `.github/workflows/coverage_2020v1.yml`.
- Existing `examples/UnitTest/*` sketches are primarily **manual hardware validation** (human-observed behavior), not assertion-driven automated tests.
- `src/lvgl/tests/*` exists, but appears to be **upstream LVGL test code** and is not integrated as a repository-wide coverage gate for this project.
- Host automation now includes policy unit tests and source-integration checks, plus integration compile checks for selected 2020-v1 examples.

Result: coverage maturity is improved, but **repository-wide C/C++ line+branch coverage is still not established**.

---

## 2) Baseline Inventory (Static Repo Review)

### 2.1 High-level source footprint

- Total source-like files (`*.c`, `*.cpp`, `*.h`, `*.hpp`, `*.ino`): **1136**
- Under `src/`: **708**
- Under `examples/`: **428**

### 2.2 Test-like assets currently present

- `examples/UnitTest/HardwareTest/HardwareTest.ino`
- `examples/UnitTest/ScreenRotation/ScreenRotation.ino`
- `src/lvgl/tests/*` (17 files)
- Various demo sketches with `test` in filename under `examples/TFT_eSPI/*`

### 2.3 Current state by evidence

- `README.MD` explicitly says examples are primarily hardware function demonstrations.
- `docs/examples_en.md` labels `UnitTest` as hardware test directory.
- CI workflow now present: `.github/workflows/coverage_2020v1.yml`.
- Sprint scripts now present:
  - `scripts/run_sprint1_unit_tests.ps1`
  - `scripts/run_sprint2_integration_builds.ps1`

---

## 3) 2020-V1 Functional Surface (What must be covered)

2020-v1 board capabilities are defined in `src/LilyGoWatch.h` + `src/board/twatch2020_v1.h` and implemented primarily through `src/TTGO.h` plus driver folders under `src/drive/`.

### 3.1 2020-v1 critical modules

- **Board mapping / compile-time feature gates**
  - `src/LilyGoWatch.h`
  - `src/board/twatch2020_v1.h`
- **Core watch orchestration**
  - `src/TTGO.h` (most logic inline)
  - `src/TTGO.cpp` (minimal)
- **Driver subsystems in 2020-v1 path**
  - `src/drive/axp/*` (power)
  - `src/drive/bma423/*` (accelerometer)
  - `src/drive/fx50xx/*` (touch)
  - `src/drive/rtc/*` (PCF8563 RTC)
  - `src/drive/i2c/*` (I2C bus)
  - `src/drive/tft/*` (backlight helper)

### 3.2 2020-v1 behaviors with branch-heavy risk

- Initialization chain (`begin()` and `init*()` methods)
- AXP202 power-domain policy differences for 2020-v1 (e.g., LDO2 handling)
- Display sleep/wakeup and backlight control behavior
- Touch coordinate reading/rotation paths
- Sensor setup and remap logic (BMA423 axis remap by board)
- IRQ paths (PMU/touch/sensor)
- Optional LVGL path and interrupt event integration

---

## 4) Coverage Gap Assessment

## 4.1 Current coverage maturity (practical)

- **Automated unit coverage (host):** Very low / none established for core modules
- **Automated integration coverage (device):** Not formalized as assertions + pass/fail automation
- **Branch coverage confidence:** Very low due heavy compile-time macros and hardware conditionals

### 4.2 Key gaps preventing 100% line+branch

1. **No agreed denominator for “100%” on a mixed repo**
   - Repo includes upstream/vendor code and demo sketches.
   - Without a scoped denominator, 100% target is ambiguous and likely infeasible.

2. **No automated coverage instrumentation pipeline**
   - Missing build targets producing `.gcda/.gcno` + merged reports.

3. **No host-side hardware abstraction seam for deterministic tests**
   - Core logic tightly coupled to hardware driver calls.

4. **Branch explosion from board and feature macros**
   - Many compile-time branches in `TTGO.h` are not exercised in tests.

5. **Manual sketches do not provide repeatable regression gating**
   - They support smoke/manual verification but not CI-quality line+branch accounting.

---

## 5) Definition of “100%” Needed for This Repo

Because scope was set to the **entire repository**, use a staged denominator policy that is still auditable:

### Coverage denominator policy

- **Tier A (Must hit 100% line+branch):**
  - Library-owned production code for 2020-v1 runtime paths:
    - `src/TTGO.h`, `src/TTGO.cpp`, `src/LilyGoWatch.h`, `src/board/twatch2020_v1.h`, `src/drive/{axp,bma423,fx50xx,rtc,i2c,tft}`
- **Tier B (Compile/Smoke coverage):**
  - `examples/*` compile matrix + selected runtime integration tests
- **Tier C (Third-party/upstream embedded code in repo):**
  - Track separately; do not block 2020-v1 library quality gate unless ownership is assumed

If strict “entire repo 100%” is still mandatory (including vendor/upstream code), effort and maintenance cost increase substantially and should be explicitly accepted.

---

## 6) Actionable TODO Backlog (Prioritized)

Use this as the working implementation checklist.

## P0 — Establish measurable automation (blockers)

- [x] Create a reproducible test harness layout (host + ESP32 integration targets)
- [x] Add coverage instrumentation configuration for line+branch reports
- [x] Add report generation and merge step (single summary artifact)
- [x] Add pass/fail thresholds and baseline quality gate
- [x] Add initial CI workflow to run tests and publish coverage reports
- [x] Freeze denominator policy (Tier A/B/C) in repository docs

## P1 — Core 2020-v1 unit coverage (host, mocked hardware)

- [ ] Add tests for `TTGOClass::begin()` path with each disable flag combination
- [ ] Add tests for `initPower()` 2020-v1 branch behavior (especially LDO2/LDO3 semantics)
- [ ] Add tests for display control (`displayOff`, `displaySleep`, `displayWakeup`)
- [ ] Add tests for backlight methods (`openBL`, `closeBL`, `setBrightness`)
- [ ] Add tests for touch API paths (`touched`, `getTouch`) including rotation branches
- [ ] Add tests for sensor init/remap behavior specific to `LILYGO_WATCH_2020_V1`
- [ ] Add tests for IRQ helper registration methods (power/touch/sensor)
- [ ] Add negative-path tests (null handlers, init failures, probe failures)

## P1 — Driver-level unit tests (host)

- [ ] Add isolated unit tests for AXP wrapper behavior in `src/drive/axp/*`
- [ ] Add isolated unit tests for BMA423 wrapper behavior in `src/drive/bma423/*`
- [ ] Add isolated unit tests for FT touch wrapper in `src/drive/fx50xx/*`
- [ ] Add isolated unit tests for RTC wrapper in `src/drive/rtc/*`
- [ ] Add isolated unit tests for I2C abstraction in `src/drive/i2c/*`
- [ ] Add isolated unit tests for TFT backlight helper in `src/drive/tft/*`

## P2 — 2020-v1 integration coverage (device-run)

- [ ] Convert current manual `examples/UnitTest/HardwareTest` into assertion-driven integration checks
- [ ] Convert `examples/UnitTest/ScreenRotation` into assertion-driven integration checks
- [ ] Add integration tests for sleep/wakeup transitions (touch/PMU/IMU wake sources)
- [ ] Add integration tests for RTC set/get + alarm interrupt behavior
- [ ] Add integration tests for power-state transitions and current-draw sanity envelopes
- [ ] Add integration tests for audio path enable/disable behavior on 2020-v1 constraints

## P2 — Examples and regression safety net

- [x] Add compile-matrix checks for representative examples in `examples/BasicUnit/*`
- [ ] Define minimal set of required “must compile” examples for 2020-v1
- [ ] Add smoke execution checklist for selected examples on real device

## P3 — Branch closure and hardening

- [ ] Add branch-targeted tests for macro-guarded code paths in `TTGO.h`
- [ ] Add tests for error/timeout conditions in I2C-dependent operations
- [ ] Add mutation-style checks or fault-injection tests for critical power/sleep paths
- [ ] Raise thresholds gradually to 100% once flaky paths are stabilized

---

## 7) Suggested Module-by-Module Coverage Targets

Initial recommended acceptance targets for a practical rollout:

- `src/TTGO.h` + 2020-v1 board paths: **>=90% line/branch first**, then 100%
- `src/drive/{axp,bma423,fx50xx,rtc,i2c,tft}`: **>=95% line/branch first**, then 100%
- 2020-v1 integration suite: cover all critical runtime transitions at least once per release

Then ratchet thresholds upward to strict 100% after stability.

---

## 8) Risks and Constraints

- Heavy compile-time branching means one build cannot cover all branches.
- Hardware-dependent timing and interrupt behavior can introduce flakiness without robust test harness controls.
- Vendor/upstream code inside repo inflates denominator and may be outside practical ownership.
- Manual test-only sketches are valuable for bring-up but insufficient for repeatable coverage accounting.

---

## 9) Recommended First 2 Sprints

### Sprint 1

- Lock denominator policy (Tier A/B/C)
- Stand up coverage pipeline + CI artifact output
- Add first wave of unit tests for `begin/initPower/display/backlight`
- Convert one manual integration scenario to assertion-based test

### Sprint 2

- Expand touch/sensor/RTC unit tests
- Add sleep/wakeup integration tests
- Add compile matrix for selected examples
- Set and enforce first numeric thresholds

---

## 10) Current Conclusion

For 2020-v1 quality confidence, the repository remains at an **early-to-intermediate testing maturity stage**: automation now exists, but full C/C++ line+branch accounting is still missing. Reaching 100% is possible only after:

1) denominator policy is formalized,  
2) coverage tooling/CI is introduced, and  
3) branch-targeted unit + integration suites are implemented for `TTGO.h` and 2020-v1 driver paths.

---

## 11) Sprint Execution Results (Completed)

### Sprint 1 delivered

- Added 2020-v1 power policy abstraction used by core code:
  - `src/board/twatch_power_policy.h`
  - integrated into `src/TTGO.h` for audio/GPS/backlight/init-power branch decisions
- Added host-side automated tests:
  - `tests/test_power_policy.py`
  - `tests/test_ttgo_policy_integration.py`
  - `tools/twatch_power_policy.py`
- Added Sprint 1 runner:
  - `scripts/run_sprint1_unit_tests.ps1`

### Sprint 1 measured output

- Test run result: **7 passed**
- Python coverage for `tools/twatch_power_policy.py`: **100% line coverage** (25/25 lines)
- Limitation: this is not yet repository-wide C/C++ line+branch coverage.

### Sprint 2 delivered

- Added integration compile-check runner:
  - `scripts/run_sprint2_integration_builds.ps1`
- Added CI workflow running both Sprint stages:
  - `.github/workflows/coverage_2020v1.yml`
- Integration compile checks executed for 2020-v1 context:
  - `examples/BasicUnit/AXP20x_IRQ`
  - `examples/BasicUnit/BMA423_Accel`
  - `examples/BasicUnit/RTC`
  - `examples/BasicUnit/TouchPad`

### Sprint 2 measured output

- Compile-check result: **4/4 selected examples succeeded** with `-D LILYGO_WATCH_2020_V1`.

---

## 12) Updated Next Steps (Post Sprint 2)

1. Add C/C++-native line+branch instrumentation (gcov/lcov-compatible path) for Tier A modules.
2. Add first assertion-driven tests for `TTGOClass::begin()`, `initPower()`, `openBL()/closeBL()`, and display sleep/wake behavior.
3. Define and publish numeric quality gates (initially lower, then ratchet upward).
4. Convert `examples/UnitTest/HardwareTest` and `ScreenRotation` from manual-only flows into assertion-driven integration checks.
5. Separate reporting for Tier A/B/C in CI to avoid denominator ambiguity and to track progress to 100% transparently.

---

## 13) Sprint 3 Execution Results (Completed)

### Sprint 3 delivered

- Added native C++ assertion test target for 2020 board power policy:
  - `tests/cpp/twatch_power_policy_test.cpp`
- Added Sprint 3 coverage runners:
  - `scripts/run_sprint3_cpp_coverage.ps1` (local; graceful skip if toolchain unavailable)
  - `scripts/run_sprint3_cpp_coverage.sh` (CI; strict 100/100 line+branch gate for policy header)
- Added CI job:
  - `.github/workflows/coverage_2020v1.yml` → `sprint3-cpp-coverage` on `ubuntu-latest`
- Added assertion-driven TTGO behavior contract tests:
  - `tests/test_ttgo_behavior_contracts.py`
- Added denominator policy document:
  - `docs/test_coverage_denominator_policy_2020v1.md`

### Sprint 3 measured output (local)

- Sprint 1 rerun: **11 passed**, Python gate satisfied at **100%** (fail-under 95).
- Sprint 2 rerun: **4/4 selected examples compile-check succeeded** for 2020-v1.
- Sprint 3 local run: **skipped** due missing `g++` on local Windows PATH (expected behavior).

### Sprint 3 CI gate definition

- In CI (`ubuntu-latest`), Sprint 3 enforces:
  - line coverage: **100%**
  - branch coverage: **100%**
  - scope: `src/board/twatch_power_policy.h`

---

## 14) Updated Next Steps (Post Sprint 3)

1. Expand native C++ coverage beyond policy header into Tier A modules (`TTGO`, `drive/*`) using the same reporting path.
2. Add assertion-driven tests for `TTGOClass::begin()` disable-flag combinations and `initPower()` branch behavior.
3. Add assertion-driven tests for display/backlight behavior contracts (`displaySleep`, `displayWakeup`, `openBL`, `closeBL`).
4. Convert `examples/UnitTest/HardwareTest` and `ScreenRotation` to pass/fail integration checks.
5. Split CI artifacts by Tier A/B/C and begin ratcheting Tier A thresholds module-by-module.

---

## 15) Pause / Handoff Snapshot (Current State)

Testing implementation is **paused intentionally** at this point.

### 15.1 Where we are now

- Sprints 1–3 are implemented and documented.
- CI workflow includes:
  - Sprint 1 Python unit + coverage gate
  - Sprint 2 PlatformIO compile checks
  - Sprint 3 C++ policy coverage gate (Ubuntu)
- Denominator policy is now explicit in:
  - `docs/test_coverage_denominator_policy_2020v1.md`
- Latest local validation observed before pause:
  - Sprint 1: **11 passed**, 100% on policy Python module
  - Sprint 2: **4/4** selected examples compile
  - Sprint 3 local: expected skip without `g++` on Windows PATH

### 15.2 Highest-priority next implementation work

1. Add assertion-driven tests for `TTGOClass::begin()` disable-flag combinations.
2. Add assertion-driven tests for `initPower()` branch behavior for 2020-v1 semantics.
3. Add assertion-driven tests for display/backlight contracts (`displaySleep`, `displayWakeup`, `openBL`, `closeBL`).
4. Expand native C++ coverage path from `twatch_power_policy.h` into additional Tier A modules.
5. Start converting `examples/UnitTest/*` from manual demos to pass/fail integration checks.

### 15.3 Practical resume tips

- Re-run local baseline first:
  - `./scripts/run_sprint1_unit_tests.ps1`
  - `./scripts/run_sprint2_integration_builds.ps1`
  - `./scripts/run_sprint3_cpp_coverage.ps1`
- On Windows, install a host C++ toolchain (`g++`) and `gcovr` if Sprint 3 local execution is required.
- Keep Tier A/Tier B/Tier C reporting separated to avoid denominator drift.
- Prefer small, behavior-contract tests first for `TTGO.h` methods before deeper driver mocks.
- After each added test batch, update this document’s measured-output sections.

### 15.4 Known constraints to keep in mind

- Compile-time macro branching in `TTGO.h` creates a large branch matrix; one build variant cannot cover everything.
- Hardware-interrupt/timing paths are prone to flakiness without controlled harness assumptions.
- Vendor/upstream code under repo tree should remain non-blocking unless ownership is explicitly expanded.
