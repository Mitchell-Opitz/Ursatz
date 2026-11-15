#!/usr/bin/env bash
# Prints the ten slowest tests of the last ctest run as "seconds  name", and a GitHub ::warning:: line for every
# fast-set test (no `slow` label) over 10 s. Reads <build-dir>/Testing/Temporary/CTestCostData.txt. Always exits 0.
set -u

build_dir="${1:-build}"
cost_file="$build_dir/Testing/Temporary/CTestCostData.txt"

if [ ! -f "$cost_file" ]; then
  echo "No test cost data at $cost_file"
  exit 0
fi

# Lines are "name runs cost" up to the "---" line (the lines after it are the failed tests).
costs=$(awk '/^---/ { exit } NF >= 3 { printf "%.1f  %s\n", $3, $1 }' "$cost_file" | sort -rn)

echo "Ten slowest tests (seconds):"
printf '%s\n' "$costs" | head -n 10

slow_tests=$(ctest --test-dir "$build_dir" -N -L slow 2>/dev/null | sed -n 's/^ *Test *#[0-9]*: *//p')

printf '%s\n' "$costs" | while read -r seconds name; do
  [ -n "${name:-}" ] || continue
  if awk -v s="$seconds" 'BEGIN { exit !(s > 10) }' && ! printf '%s\n' "$slow_tests" | grep -Fxq "$name"; then
    echo "::warning::Test $name took ${seconds}s in the fast set; speed it up or label it slow (over 10 s)."
  fi
done

exit 0
