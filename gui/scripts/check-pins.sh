#!/usr/bin/env bash
# Fails when a submodule pin is not on its remote's main, or when one repository is pinned at two different commits
# anywhere in the tree.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

normalize() {
    local url
    url="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
    url="${url%.git}"
    printf '%s' "$url" | sed -E 's#^([a-z+]+://)[^/@]*@#\1#'
}

declare -A pins_by_url=()
failed=0

while IFS= read -r line; do
    [ -n "$line" ] || continue
    status="${line:0:1}"
    read -r sha path _ <<<"${line:1}"
    if [ "$status" = "-" ]; then
        echo "FAIL $path $sha is not checked out" >&2
        failed=1
        continue
    fi
    url="$(normalize "$(git -C "$path" remote get-url origin)")"
    git -C "$path" fetch -q origin main
    if ! git -C "$path" merge-base --is-ancestor "$sha" FETCH_HEAD; then
        echo "FAIL $path $sha is not on main" >&2
        failed=1
        continue
    fi
    pins_by_url["$url"]+="$sha $path"$'\n'
    echo "OK $path $sha"
done < <(git submodule status --recursive)

for url in "${!pins_by_url[@]}"; do
    if [ "$(printf '%s' "${pins_by_url[$url]}" | cut -d' ' -f1 | sort -u | wc -l)" -gt 1 ]; then
        echo "FAIL $url is pinned at different commits:" >&2
        printf '%s' "${pins_by_url[$url]}" | sed 's/^/  /' >&2
        failed=1
    fi
done

exit "$failed"
