#!/usr/bin/env bash
# Fails when a file under frontend/public (not vendor/) defines one of the music-theory names that moved to the apps
# (R-218, R-219) or one of the Composer vocabulary tables it now reads from the vocabulary (R-270): a function, class, const/let/var, object member or method with that name. Prints each offending line.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

names='playbackFromMidi|randomSettings|defaultCadence|parseHarmonyToken|HARMONY_TOKEN|KEY_TONICS|RANDOM_METERS|chordProblems|numeralsOk|MOTIF_ROLES|MOTIF_FORMS|FUNCTIONS|DISTANCES|SUITS_FILTERS'
pattern="(\\b(function|class|const|let|var)[[:space:]]+($names)\\b)|(^|[^.[:alnum:]_\$])($names)[[:space:]]*(:|=[^=>]|=\$|\\([^)]*\\)[[:space:]]*\\{)"

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit (a removed theory name is defined in the frontend)" >&2
    failed=1
done < <(grep -rnE --include='*.js' --include='*.mjs' --include='*.html' --exclude-dir=vendor "$pattern" frontend/public || true)

if [ "$failed" = 1 ]; then
    exit 1
fi
echo "OK frontend holds none of the removed theory"
