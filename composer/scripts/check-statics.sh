#!/usr/bin/env bash
# Fails when a file in src/ declares a file-scope static variable that is not const (a mutable pointer to const data
# counts as mutable: write "static const T *const name"). Prints each offending file and line.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit" >&2
    failed=1
done < <(find src -name '*.c' -o -name '*.h' | sort | xargs awk '
    /^static[ \t]/ && !/^static[ \t]+inline/ {
        line = $0
        if (line ~ /\(/ && line !~ /^[^(=;]*\(\*/) next      # a function (a function pointer variable has (* first)
        sub(/[ \t]*=.*/, "", line)                           # drop the initializer
        sub(/;.*/, "", line)
        if (line ~ /\*/) {
            tail = line
            sub(/.*\*/, "", tail)                            # after the last *: the pointer itself must be const
            constant = tail ~ /const/
        } else {
            constant = line ~ /const/
        }
        if (!constant) printf "%s:%d: %s\n", FILENAME, FNR, $0
    }')

if [ "$failed" -ne 0 ]; then
    echo "File-scope state is not allowed in src/: pass it through the caller's context." >&2
    exit 1
fi
