#!/usr/bin/env bash
# Records the Composer requests the workshop's theme and piece translation builds today (P63 B15, R-650): every theme and piece
# body the GUI route tests and the migrated version 0 folder compose, with the request built from it, as
# requests/<kind>-<hash>.json {kind, body, request}. The folder of the version's files reads "@STAGE@" and the data folder "@DATA@".
# Run from the repository root after a build; it replaces requests/.
set -euo pipefail
out=$(cd "$(dirname "$0")" && pwd)/requests
rm -rf "$out"
mkdir -p "$out"
URSATZ_DUMP_REQUESTS="$out" ctest --test-dir build -R '^(theme_route|piece_route|migrated_routes|materials_route|piece_snapshot)$' -j1 --timeout 900
ls "$out" | wc -l
