#include <ursatz_composer/variant_registry.h>

#include <stdio.h>
#include <string.h>

#include "analysis/motive_relation.h"
#include "analysis/motive_variant_recognition.h"

#include <ursatz_composer/motive_sketch_bridge.h>

#include "motive/variant_internal.h"

/* The registry row of a relation, NULL when none maps to it. */
static const VariantOperation *operation_of(MotiveRelationKind kind) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        if (operation->relation == (int)kind) {
            return operation;
        }
    }
    return NULL;
}

/*
 * A relation step as a registry step: the row's parameters are the relation's, in order. A value the registry range does
 * not hold is a parameter that was not given (0, or -1 for the invert axis), which an optional parameter leaves open.
 */
static bool step_from_relation(const MotiveRelationStep *relation, VariantStep *step) {
    const VariantOperation *operation = operation_of(relation->kind);
    if (!operation || !variant_step_init(step, operation->name)) {
        return false;
    }
    for (size_t p = 0; p < operation->param_count; p++) {
        const VariantParam *param = &operation->params[p];
        if (variant_step_set(step, param->name, relation->value[p], relation->den[p])) {
            continue;
        }
        if (!param->optional) {
            return false;
        }
    }
    return true;
}

VariantResult variant_recognize_depth(const Motive *source, const Motive *target, size_t depth, VariantChain *chain,
                                      VariantRecognition *out) {
    if (!source || !target || !chain || !out) {
        return VARIANT_ERR_NULL;
    }
    memset(chain, 0, sizeof(*chain));
    memset(out, 0, sizeof(*out));
    out->distance_num = 1;
    out->distance_den = 1;
    if (depth < 1 || depth > VARIANT_CHAIN_MAX) {
        return VARIANT_ERR_PARAM;
    }
    MotiveSketch from;
    MotiveSketch to;
    if (!motive_to_sketch(source, &from) || !motive_to_sketch(target, &to)) {
        return VARIANT_ERR_PARAM;
    }
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.max_depth = depth;
    MotiveRecognition recognition;
    MotiveRecognizeResult searched = motive_variant_recognize(&from, &to, &options, &recognition);
    if (searched == MOTIVE_RECOGNIZE_NO_MEMORY) {
        return VARIANT_ERR_MEMORY;
    }
    if (searched != MOTIVE_RECOGNIZE_OK) {
        return VARIANT_ERR_PARAM;
    }

    VariantRecord record;
    memset(&record, 0, sizeof(record));
    bool replayed = false;
    if (recognition.found) {
        for (size_t s = 0; s < recognition.step_count; s++) {
            if (!step_from_relation(&recognition.steps[s], &chain->steps[s])) {
                memset(chain, 0, sizeof(*chain));
                break;
            }
            chain->step_count = s + 1;
        }
        Motive replay;
        replayed = chain->step_count == recognition.step_count &&
                   variant_apply_chain(source, chain, 0, &replay, &record) == VARIANT_OK && motive_equal(&replay, target);
    }

    if (replayed) {
        out->found = true;
        out->distance_num = recognition.distance.n;
        out->distance_den = recognition.distance.d;
    } else {
        /* Nothing found, or a chain that does not replay: the target against the source, with no steps. */
        memset(&record, 0, sizeof(record));
        variant_record_difference(source, target, &record);
    }
    memcpy(out->kept, record.kept, sizeof(out->kept));
    out->kept_count = record.kept_count;
    memcpy(out->changed, record.changed, sizeof(out->changed));
    out->changed_count = record.changed_count;
    out->rhythm_scaled = record.rhythm_scaled;
    if (replayed) {
        snprintf(out->derivation, sizeof(out->derivation), "%s", record.derivation);
    } else {
        variant_record_derivation(&record); /* ". Kept the rhythm; changed ..." */
        snprintf(out->derivation, sizeof(out->derivation), "No chain of up to %zu steps turns the source into the target%.400s",
                 depth, record.derivation);
    }
    return VARIANT_OK;
}

VariantResult variant_recognize(const Motive *source, const Motive *target, VariantChain *chain,
                                VariantRecognition *out) {
    return variant_recognize_depth(source, target, VARIANT_CHAIN_MAX, chain, out);
}
