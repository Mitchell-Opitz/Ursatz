#!/usr/bin/env bash
# Fails when src/ or cli/ hears music outside the Analyzer (R-257): a call of the library's form, texture or fragment
# detectors that the Analyzer's result now gives (standard_form_segment_phrases, standard_form_segment_ideas,
# standard_form_identify_themes, phrase_boundary_detection_*, standard_texture_analysis_*, analyzer_analyze_fragment,
# analyzer_fragment_*), or a call of plan_render outside hearing.c, export.c and variations.c (render.c defines it).
# Prints each offending file and line.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

detectors='standard_form_segment_phrases|standard_form_segment_ideas|standard_form_identify_themes|phrase_boundary_detection_|standard_texture_analysis_|analyzer_analyze_fragment|analyzer_fragment_'
render='plan_render(_range)?[[:space:]]*\('

failed=0
while IFS= read -r hit; do
    echo "FAIL $hit" >&2
    failed=1
done < <(
    grep -rnE "\b($detectors)" src cli --include='*.c' --include='*.h' || true
    grep -rnE "\b$render" src cli --include='*.c' --include='*.h' |
        grep -vE '^src/(check/hearing|render/export|render/render|fill/variations)\.c:|^src/include/ursatz_composer/render\.h:' || true
)

if [ "$failed" -ne 0 ]; then
    echo "Music is heard through the Analyzer once per run (src/check/hearing.h); a plan is rendered in hearing.c, export.c and variations.c only." >&2
    exit 1
fi
echo "OK src and cli hear music only through the Analyzer"
