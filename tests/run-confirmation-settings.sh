#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/settings-test
for fixture in boolean_settings boolean_view native_boolean_callback; do
  clang++ -std=c++23 -pthread -I mods/src -I third_party/libil2cpp \
    "tests/${fixture}_test.cc" -o "build/settings-test/$fixture"
  "build/settings-test/$fixture"
done
