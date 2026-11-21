#include <stdlib.h>
#include <string.h>

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

size_t grow_draw_drop(FillRng *rng, size_t count) {
    size_t most = count / 2 > 1 ? count / 2 : 1;
    size_t drop = 1 + (size_t)fill_rng_below(rng, most);
    return drop >= count ? count - 1 : drop;
}

bool grow_draw_residues(const Motive *source, FillRng *rng, Rational bar, Motive *liquidated, Motive *repeated,
                        MotiveRelationStep *liquidation, MotiveRelationStep *repetition, size_t *drop) {
    size_t count = source->note_count;
    size_t dropped = grow_draw_drop(rng, count);
    MotiveRelationStep first;
    memset(&first, 0, sizeof(first));
    first.kind = MOTIVE_RELATION_LIQUIDATE;
    first.value[0] = (long)dropped;
    first.value[2] = 1;
    MotiveRelationStep second = first;
    second.value[0] = (long)dropped + 1;
    second.value[2] = 0;
    if (motive_develop(source, &first, liquidated) != MOTIVE_DEV_OK || liquidated->note_count >= count ||
        rational_cmp(liquidated->length, bar) > 0 || !grow_motive_in_form_range(liquidated) ||
        motive_develop(source, &second, repeated) != MOTIVE_DEV_OK || repeated->note_count >= liquidated->note_count ||
        rational_cmp(repeated->length, bar) > 0 || !grow_motive_in_form_range(repeated)) {
        return false;
    }
    *liquidation = first;
    liquidation->value[2] = 0;
    *repetition = second;
    *drop = dropped;
    return true;
}
