#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/gagguino-native-tests
count=0
for test_file in tests/*.cpp; do
  test_name="$(basename "$test_file" .cpp)"
  echo ">>> $test_name"
  g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. "$test_file" -o "/tmp/gagguino-native-tests/$test_name"
  "/tmp/gagguino-native-tests/$test_name"
  count=$((count + 1))
done
echo "PASS: $count native regression test executables"
