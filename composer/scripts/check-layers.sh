#!/usr/bin/env bash
# Fails when a file outside src/api/ includes api/api_internal.h (or api_internal.h by its bare name). The engine reads
# its job through util/job.h; API internals stay inside the API layer.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit" >&2
    failed=1
done < <(grep -rnE '#include[ \t]+["<][^">]*api_internal\.h[">]' src cli include tests 2>/dev/null | grep -v '^src/api/' || true)

if [ "$failed" -ne 0 ]; then
    echo "api_internal.h is for src/api/ only." >&2
    exit 1
fi
