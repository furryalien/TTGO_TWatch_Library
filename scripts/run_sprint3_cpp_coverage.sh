#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

OUT_DIR="coverage/cpp"
BUILD_DIR="$OUT_DIR/build"
REPORT="$OUT_DIR/coverage_cpp.xml"

mkdir -p "$BUILD_DIR"

g++ -std=c++11 -fprofile-arcs -ftest-coverage -I . tests/cpp/twatch_power_policy_test.cpp -o "$BUILD_DIR/policy_test"
"$BUILD_DIR/policy_test"

gcovr --root . \
  --filter "src/board/twatch_power_policy.h" \
  --xml-pretty \
  --branches \
  --fail-under-line 100 \
  --fail-under-branch 100 \
  --output "$REPORT"

echo "Sprint 3 C++ coverage checks passed."