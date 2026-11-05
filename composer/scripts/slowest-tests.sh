#!/usr/bin/env bash
# Prints the ten slowest tests of the last ctest run in <build-dir> as "seconds  name", and a GitHub warning for each
# test in the fast set (not labeled slow) that took over 10 seconds. Exits 0 always.
build_dir="${1:?usage: scripts/slowest-tests.sh <build-dir>}"
cost_file="$build_dir/Testing/Temporary/CTestCostData.txt"
if [ ! -f "$cost_file" ]; then
    echo "No ctest cost data in $build_dir"
    exit 0
fi
slow_names=$(ctest --test-dir "$build_dir" -N -L slow 2>/dev/null | sed -n 's/^ *Test *#[0-9]*: //p')
awk '/^---/ {exit} NF >= 3 {printf "%.2f %s\n", $3, $1}' "$cost_file" | sort -rn > "${cost_file}.sorted"
echo "Ten slowest tests"
head -n 10 "${cost_file}.sorted" | awk '{printf "%8.2f  %s\n", $1, $2}'
while read -r seconds name; do
    if awk -v s="$seconds" 'BEGIN {exit !(s > 10)}' && ! grep -qx -- "$name" <<<"$slow_names"; then
        echo "::warning::$name took $seconds s in the fast set; speed it up or label it slow"
    fi
done < "${cost_file}.sorted"
rm -f "${cost_file}.sorted"
exit 0
