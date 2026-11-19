#!/usr/bin/env bash
# Fails when src/ or cli/ hears music outside the library chain (R-257, R-618): a call of the library's form, texture or fragment
# detectors that the Analyzer's result now gives (standard_form_segment_phrases, standard_form_segment_ideas,
# standard_form_identify_themes, phrase_boundary_detection_*, standard_texture_analysis_*, analyzer_analyze_fragment,
# analyzer_fragment_*), a call of plan_render outside hearing.c, export.c and variations.c (render.c defines it), a call of
# reading_build or of a chain result accessor (analysis_chain_result, analysis_chain_cadences, analysis_chain_themes,
# analysis_chain_phrases, analysis_chain_key_regions, chain_result_voice_faults) outside check/hearing.c (the critic reads the
# hearing's readings, R-633), or of analysis_chain_texture outside texture/accompaniment_hear.c (the accompaniment judge keeps its own
# TEXTURE chain run).
# Prints each offending file and line.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root/composer"

detectors='standard_form_segment_phrases|standard_form_segment_ideas|standard_form_identify_themes|phrase_boundary_detection_|standard_texture_analysis_|analyzer_analyze_fragment|analyzer_fragment_'
render='plan_render(_range)?[[:space:]]*\('
run='analysis_chain_run(_score)?[[:space:]]*\('
results='reading_build|analysis_chain_(result|cadences|themes|phrases|key_regions)|chain_result_voice_faults'
texture='analysis_chain_texture'

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit" >&2
    failed=1
done < <(
    grep -rnE "\b($detectors)" src cli --include='*.c' --include='*.h' || true
    grep -rnE "\b$render" src cli --include='*.c' --include='*.h' |
        grep -vE '^src/(check/hearing|render/export|render/render|candidates/variations)\.c:|^src/include/ursatz_composer/render\.h:' || true
    grep -rnE "\b$run" src cli --include='*.c' --include='*.h' |
        grep -vE '^src/(check/hearing|texture/accompaniment_hear)\.c:' || true
    grep -rnE "\b($results)[[:space:]]*\(" src cli --include='*.c' --include='*.h' |
        grep -vE '^src/check/hearing\.c:' || true
    grep -rnE "\b$texture[[:space:]]*\(" src cli --include='*.c' --include='*.h' |
        grep -vE '^src/texture/accompaniment_hear\.c:' || true
)

if [ "$failed" -ne 0 ]; then
    echo "Music is heard through the library chain once per run (src/check/hearing.h); a plan is rendered in hearing.c, export.c and variations.c only, the chain runs in hearing.c and accompaniment_hear.c only, and its results and readings are read in hearing.c (the texture result in accompaniment_hear.c)." >&2
    exit 1
fi
echo "OK src and cli hear music only through the library chain"
