#!/usr/bin/env bash
# Fails when src/ or include/ includes an Ursatz header that is not under a staged public root of the build, names a
# *_internal.h of Ursatz, or when a CMake file reads SOURCE_DIR of an Ursatz target or a path under
# external/ursatz/tests. Prints each offending line. Run after the build; the build directory is the first argument.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

build="${1:-build}"
manifest="$build/external/ursatz/public/manifest.txt"
if [ ! -f "$manifest" ]; then
    echo "FAIL $manifest not found (build first)" >&2
    exit 2
fi

roots=()
while IFS='|' read -r _ root _; do
    [ -n "$root" ] && roots+=("$root")
done <"$manifest"

failed=0

report() {
    echo "FAIL $1:$2: $3 ($4)" >&2
    failed=1
}

ursatz_has() {
    # True when external/ursatz holds a file whose path ends with the include name.
    local name="$1" base
    base="$(basename "$name")"
    find external/ursatz -name "$base" -not -path '*/.git/*' -not -path 'external/ursatz/external/*' 2>/dev/null |
        grep -q "/$name\$"
}

while IFS=: read -r file line text; do
    name="$(printf '%s' "$text" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/')"
    local_found=0
    for dir in src include "$(dirname "$file")" external/cjson tests/support; do
        if [ -e "$dir/$name" ]; then
            local_found=1
            break
        fi
    done
    [ "$local_found" = 1 ] && continue
    if ! ursatz_has "$name"; then
        continue # a system header
    fi
    case "$name" in
        *_internal.h)
            report "$file" "$line" "$name" "internal header of Ursatz"
            continue
            ;;
    esac
    staged=0
    for root in "${roots[@]}"; do
        if [ -e "$root/$name" ]; then
            staged=1
            break
        fi
    done
    [ "$staged" = 1 ] || report "$file" "$line" "$name" "not under a staged public root"
done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]' src include || true)

while IFS=: read -r file line text; do
    report "$file" "$line" "$(printf '%s' "$text" | sed -E 's/^[[:space:]]+//')" "reads Ursatz sources"
done < <(grep -rnE 'SOURCE_DIR|external/ursatz/tests' --include=CMakeLists.txt --include='*.cmake' . \
    --exclude-dir=external --exclude-dir=build --exclude-dir='build-*' |
    grep -E 'get_target_property|external/ursatz/tests' | grep -vE '^[^:]*:[0-9]+:[[:space:]]*#' || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK analyzer includes only public Ursatz headers"
