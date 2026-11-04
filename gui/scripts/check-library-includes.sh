#!/usr/bin/env bash
# Fails when backend/src or backend/include includes an Ursatz header other than persistence/persistence.h in
# persistence_service.c and corpus/corpus_api.h in collection_service.c (the storage edge, R-218), an Analyzer header
# outside ursatz_analyzer/, a Composer header other than ursatz_composer/composer_api.h, or any *_internal.h. Prints each
# offending line. Run after the build; the build directory is the first argument (it must exist).
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

build="${1:-build}"
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
    find "$dir" -name "$base" -not -path '*/.git/*' -not -path "$dir/external/*" 2>/dev/null |
        grep -q "/$name\$"
}

while IFS=: read -r file line text; do
    name="$(printf '%s' "$text" | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/')"
    local_found=0
    for dir in backend/src backend/include "$(dirname "$file")" external/cjson; do
        if [ -e "$dir/$name" ]; then
            local_found=1
            break
        fi
    done
    [ "$local_found" = 1 ] && continue
    if submodule_has external/ursatz-composer "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of the Composer" ;;
            ursatz_composer/composer_api.h) ;;
            *) report "$file" "$line" "$name" "Composer header other than ursatz_composer/composer_api.h" ;;
        esac
        continue
    fi
    if submodule_has external/ursatz-analyzer "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of the Analyzer" ;;
            ursatz_analyzer/*) ;;
            *) report "$file" "$line" "$name" "Analyzer header outside ursatz_analyzer/" ;;
        esac
        continue
    fi
    if submodule_has external/ursatz "$name"; then
        case "$name" in
            *_internal.h) report "$file" "$line" "$name" "internal header of Ursatz" ;;
            persistence/persistence.h)
                [ "$(basename "$file")" = persistence_service.c ] || report "$file" "$line" "$name" "storage edge header outside persistence_service.c"
                ;;
            corpus/corpus_api.h)
                [ "$(basename "$file")" = collection_service.c ] || report "$file" "$line" "$name" "storage edge header outside collection_service.c"
                ;;
            *) report "$file" "$line" "$name" "Ursatz header outside the storage edge" ;;
        esac
    fi
done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]' backend/src backend/include || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK backend includes only app APIs and the storage edge"
