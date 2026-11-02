#!/usr/bin/env bash
# Fails when a file under src/ or include/ calls a library chain builder (chain_build_*): the Analyzer reads the chain's
# stage inputs through the analysis_chain_* accessors and never builds them again. Prints each offending line.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit (read the chain's stage input instead of building it)" >&2
    failed=1
done < <(grep -rnE 'chain_build_' src include 2>/dev/null || true)

exit "$failed"
