#!/usr/bin/env bash
# Prints the ten slowest tests from ctest's cost data as "seconds  name", and a GitHub
# ::warning:: line for each fast-set (not labeled slow) test over 10 s (R-307).
# Usage: scripts/slowest-tests.sh <build-dir>. Always exits 0.
build_dir="${1:-build}"
cost_file="$build_dir/Testing/Temporary/CTestCostData.txt"

if [ ! -f "$cost_file" ]; then
  echo "No cost data at $cost_file"
  exit 0
fi

costs=$(awk '/^---/ { exit } NF >= 3 { printf "%.2f %s\n", $3, $1 }' "$cost_file" | sort -rn)

echo "Ten slowest tests (seconds  name):"
echo "$costs" | head -n 10 | awk '{ printf "%8.2f  %s\n", $1, $2 }'

slow_tests=$(ctest --test-dir "$build_dir" -N -L slow 2>/dev/null | sed -n 's/^ *Test *#[0-9]*: //p')

echo "$costs" | awk '$1 > 10 { print }' | while read -r seconds name; do
  if ! printf '%s\n' "$slow_tests" | grep -Fxq -- "$name"; then
    echo "::warning::Fast-set test $name took ${seconds}s (over 10 s): speed it up or label it slow"
  fi
done
exit 0
