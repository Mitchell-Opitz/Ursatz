#include <stdlib.h>

#include "grow/grow_internal.h"

bool grow_generate_specs(const Plan *plan, const void *request, const GrowContext *context, FillRng *rng, const GrowSpecKind *kind,
                         FillCandidateList *out) {
    size_t wanted = context && context->candidates > 0 ? context->candidates : GROW_DEFAULT_CANDIDATES;
    void *kept[16];
    size_t kept_count = 0;
    if (wanted > 16) {
        wanted = 16;
    }
    for (size_t draw = 0; kept_count < wanted && draw < wanted * DRAWS_PER_CANDIDATE; draw++) {
        void *spec = malloc(kind->size);
        if (!spec) {
            return false;
        }
        bool repeat = false;
        if (!kind->draw(plan, request, context, rng, spec)) {
            free(spec);
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || kind->same(kept[i], spec);
        }
        if (repeat) {
            free(spec);
            continue;
        }
        char label[FILL_LABEL_MAX];
        kind->label(spec, label, sizeof(label));
        if (!fill_candidates_add(out, label, spec, free)) {
            return false;
        }
        kept[kept_count++] = spec;
    }
    return true;
}
