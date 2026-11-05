#!/usr/bin/env bash
# Fails when a file in cli/ includes anything of the Composer other than its public API header
# (ursatz_composer/composer_api.h): another ursatz_composer/ header, anything from src/, or a header of Ursatz or the
# Analyzer. The CLI may include its own headers, cJSON (to build requests) and system headers. Prints each offending line.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

failed=0

report() {
    echo "FAIL $1:$2: $3 ($4)" >&2
    failed=1
}

found_in() {
    # True when one of the directories holds the include name.
    local name="$1" dir
    shift
    for dir in "$@"; do
        [ -e "$dir/$name" ] && return 0
    done
    return 1
}

while IFS=: read -r file line text; do
    name="$(printf '%s' "$text" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/')"
    case "$name" in
        ursatz_composer/composer_api.h | cJSON.h) continue ;;
        ursatz_composer/*) report "$file" "$line" "$name" "a Composer header other than composer_api.h"; continue ;;
        ursatz_analyzer/* | ursatz/*) report "$file" "$line" "$name" "a header of Ursatz or the Analyzer"; continue ;;
    esac
    if found_in "$name" cli; then
        continue
    fi
    if found_in "$name" src src/include; then
        report "$file" "$line" "$name" "a private header of the Composer"
    elif found_in "$name" include external/ursatz external/ursatz-analyzer external/ursatz-analyzer/include external/ursatz/include; then
        report "$file" "$line" "$name" "a header of Ursatz or the Analyzer"
    elif find external/ursatz external/ursatz-analyzer -name "$(basename "$name")" -not -path '*/.git/*' 2>/dev/null |
        grep -q "/$name\$"; then
        report "$file" "$line" "$name" "a header of Ursatz or the Analyzer"
    fi
done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]' cli || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK the CLI includes only the composer API"
