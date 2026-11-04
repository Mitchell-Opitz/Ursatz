#!/usr/bin/env bash
# Lists the library headers the apps include, one per line:
#   <include as written> <library path of the header>
# Usage: scripts/app_includes.sh <app checkout>...
# Run from anywhere; the library is the repository this script lives in.
# A manual report only. Nothing here enforces anything: enforcement is each app's
# check-library-includes.sh in CI.
set -eu
lib="$(cd "$(dirname "$0")/.." && pwd)"
[ "$#" -ge 1 ] || { echo "usage: $0 <app checkout>..." >&2; exit 2; }

headers="$(cd "$lib" && find src frameworks chain -name '*.h' -not -path '*/tests/*' | sort)"

for app in "$@"; do
    # app sources only: not the pinned library submodules, not build trees
    grep -rhoE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]+[>"]' \
        --include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp' \
        --exclude-dir=external --exclude-dir=build --exclude-dir=.git "$app" 2>/dev/null
done | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"]/\1/' | sort -u |
while read -r inc; do
    case "$inc" in
        *.h) ;;
        *) continue ;;
    esac
    # a library header is one whose path ends with the include (src/ and chain/ roots are include roots)
    match="$(printf '%s\n' "$headers" | grep -E "(^|/)${inc//./\\.}\$" | head -3 | tr '\n' ' ')"
    [ -n "$match" ] && echo "$inc $match"
done
exit 0
