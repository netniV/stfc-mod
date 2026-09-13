#!/usr/bin/env bash
set -euo pipefail

# Pass the include directory of the toml++ package used by the normal build.
toml_include="${1:?usage: run-config-save.sh TOML_INCLUDE_DIR}"
cd "$(dirname "$0")/.."
mkdir -p build/config-save-test
test_root="$(mktemp -d "$PWD/build/config-save-test/run-XXXXXX")"

clang++ -std=c++23 -I mods/src -I "$toml_include" \
  tests/config_save_test.cc mods/src/config_save.cc -o "$test_root/test"
"$test_root/test" "$test_root/ordinary"
clang++ -std=c++23 -I mods/src -I "$toml_include" \
  tests/config_save_failure_test.cc -o "$test_root/failure-test"
"$test_root/failure-test" "$test_root/failures"
clang++ -std=c++23 -I mods/src -I "$toml_include" \
  tests/toml_editor_test.cc mods/src/toml_editor.cc mods/src/config_save.cc -o "$test_root/editor-test"
"$test_root/editor-test" "$test_root/editor"
clang++ -std=c++23 -pthread -I mods/src -I "$toml_include" \
  tests/runtime_config_writer_test.cc -o "$test_root/worker-test"
"$test_root/worker-test"
