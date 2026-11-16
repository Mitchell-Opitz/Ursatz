#!/usr/bin/env bash
# Fails when backend/src includes an Ursatz header, an Analyzer header outside ursatz_analyzer/, a
# Composer header other than ursatz_composer/composer_api.h, or any *_internal.h. Prints each offending line. Run after
# the build; the build directory is the first argument (it must exist).
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root/gui"

build="$(cd "$root" && cd "${1:-build}" && pwd)"
if [ ! -d "$build" ]; then
    echo "FAIL $build not found (build first)" >&2
    exit 2
fi

failed=0

report() {
    echo "FAIL $1:$2: $3 ($4)" >&2
    failed=1
}

# True when the submodule $1 holds a file whose path ends with the include name $2.
submodule_has() {
    local dir="$1" name="$2" base
    base="$(basename "$name")"
    find "$dir" -name "$base" -not -path '*/.git/*' 2>/dev/null |
        grep -q "/$name\$"
}

while IFS=: read -r file line text; do
    name="$(printf '%s' "$text" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/')"
    local_found=0
    for dir in backend/src "$(dirname "$file")" ../third_party/cjson; do
        if [ -e "$dir/$name" ]; then
            local_found=1
            break
        fi
    done
    [ "$local_found" = 1 ] && continue
    if submodule_has ../composer "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of the Composer" ;;
            ursatz_composer/composer_api.h) ;;
            *) report "$file" "$line" "$name" "Composer header other than ursatz_composer/composer_api.h" ;;
        esac
        continue
    fi
    if submodule_has ../analyzer "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of the Analyzer" ;;
            ursatz_analyzer/*) ;;
            *) report "$file" "$line" "$name" "Analyzer header outside ursatz_analyzer/" ;;
        esac
        continue
    fi
    if submodule_has ../library "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of Ursatz" ;;
            *) report "$file" "$line" "$name" "Ursatz header" ;;
        esac
    fi
done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]' backend/src || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK backend includes only app APIs"
