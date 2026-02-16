# TTGO T-Watch 2020-V1 Coverage Denominator Policy

**Date:** 2026-02-15  
**Version:** 1.0  
**Applies to:** coverage roadmap and CI quality gates for 2020-v1

---

## 1) Purpose

Define an explicit denominator for the “100% line + branch” objective so progress is measurable and comparable across runs.

---

## 2) Coverage Tiers

### Tier A — Quality gate (must reach 100% line + branch)

Library-owned production code in 2020-v1 runtime paths:

- `src/TTGO.h`
- `src/TTGO.cpp`
- `src/LilyGoWatch.h`
- `src/board/twatch2020_v1.h`
- `src/board/twatch_power_policy.h`
- `src/drive/axp/*`
- `src/drive/bma423/*`
- `src/drive/fx50xx/*`
- `src/drive/rtc/*`
- `src/drive/i2c/*`
- `src/drive/tft/*`

### Tier B — Build/smoke gate

Examples and integration entry points that must compile for 2020-v1 and are candidates for assertion-driven runtime checks:

- `examples/BasicUnit/*` selected compile matrix
- `examples/UnitTest/*` (as they are converted to assertion-driven checks)

### Tier C — Report-only (non-blocking by default)

Third-party/upstream/vendor code stored in-repo, including LVGL trees and bundled external libraries unless ownership is explicitly adopted.

---

## 3) Initial Numeric Gates

These gates are intentionally staged and will ratchet upward:

- Sprint 1 gate (host Python policy checks): line coverage `>=95%` (implemented)
- Sprint 3 gate (native C++ policy test target): line `100%`, branch `100%` for `src/board/twatch_power_policy.h`

Tier A modules beyond the policy target remain in rollout and are not yet fully instrumented in CI.

---

## 4) Reporting Rules

- Publish separate artifacts per tier where possible.
- Do not merge Tier C into Tier A pass/fail percentages.
- Coverage summaries must always state denominator scope in text.

---

## 5) Change Control

Any changes to this denominator policy must:

1. Update this document version and date.
2. Update CI workflow and sprint scripts if gates change.
3. Be reflected in `docs/test_coverage_analysis_2020v1.md`.