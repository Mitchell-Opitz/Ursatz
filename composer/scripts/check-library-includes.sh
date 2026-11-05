#!/usr/bin/env bash
# Fails when src/, include/ or cli/ includes an Ursatz header that is not under a staged public root of the build, an
# Analyzer header outside external/ursatz-analyzer/include/ursatz_analyzer/, or any *_internal.h of either, or when the
# public include directory holds anything but composer_api.h, composer_api.h includes a non-standard header, or
# src/CMakeLists.txt links a library PUBLIC (a consumer must see no Ursatz or Analyzer header through the Composer), or when a
# CMake file reads SOURCE_DIR of an Ursatz or Analyzer target or a path under external/ursatz/tests or
# external/ursatz-analyzer/src. Prints each offending line. Run after the build; the build directory is the first argument.
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

analyzer_has() {
    # True when external/ursatz-analyzer holds a file whose path ends with the include name.
    local name="$1" base
    base="$(basename "$name")"
    find external/ursatz-analyzer -name "$base" -not -path '*/.git/*' -not -path 'external/ursatz-analyzer/external/*' 2>/dev/null |
        grep -q "/$name\$"
}

# The public include directory holds only the API header, and that header includes only standard headers.
for header in include/ursatz_composer/*; do
    [ "$(basename "$header")" = composer_api.h ] || report "$header" 1 "$(basename "$header")" "public header other than composer_api.h"
done
while IFS=: read -r line text; do
    case "$text" in
        *"<stdbool.h>"* | *"<stddef.h>"* | *"<stdint.h>"*) ;;
        *) report include/ursatz_composer/composer_api.h "$line" "$text" "composer_api.h includes only standard headers" ;;
    esac
done < <(grep -nE '^[[:space:]]*#[[:space:]]*include' include/ursatz_composer/composer_api.h || true)
if grep -E 'target_link_libraries\(ursatz_composer[[:space:]]*$' -A3 src/CMakeLists.txt | grep -qE '(PUBLIC|INTERFACE)[[:space:]]'; then
    report src/CMakeLists.txt 1 "target_link_libraries(ursatz_composer" "libraries are linked PRIVATE"
fi

while IFS=: read -r file line text; do
    name="$(printf '%s' "$text" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/')"
    local_found=0
    for dir in src src/include include cli "$(dirname "$file")" external/cjson; do
        if [ -e "$dir/$name" ]; then
            local_found=1
            break
        fi
    done
    [ "$local_found" = 1 ] && continue
    if analyzer_has "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of the Analyzer" ;;
            ursatz_analyzer/*) [ -e "external/ursatz-analyzer/include/$name" ] || report "$file" "$line" "$name" "not a public Analyzer header" ;;
            *) report "$file" "$line" "$name" "Analyzer header outside include/ursatz_analyzer" ;;
        esac
        continue
    fi
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
done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]' src include cli || true)

while IFS=: read -r file line text; do
    report "$file" "$line" "$(printf '%s' "$text" | sed -E 's/^[[:space:]]+//')" "reads Ursatz sources"
done < <(grep -rnE 'SOURCE_DIR|external/ursatz/tests|external/ursatz-analyzer/src' --include=CMakeLists.txt --include='*.cmake' . \
    --exclude-dir=external --exclude-dir=build --exclude-dir='build-*' |
    grep -E 'get_target_property|external/ursatz/tests|external/ursatz-analyzer/src' | grep -vE '^[^:]*:[0-9]+:[[:space:]]*#' || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK composer includes only public Ursatz and Analyzer headers"
