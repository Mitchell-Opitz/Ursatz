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
 * not hold is a parameter that was not given (0, or -1 for the invert axis). An optional parameter stays open, which the
 * row does without; the invert axis is not optional (a chain would draw it), so it is set to the pitch of the first note
 * of motive, the motive the step is applied to.
 */
static bool step_from_relation(const MotiveRelationStep *relation, const Motive *motive, VariantStep *step) {
    const VariantOperation *operation = operation_of(relation->kind);
    if (!operation || !variant_step_init(step, operation->name)) {
        return false;
    }
    for (size_t p = 0; p < operation->param_count; p++) {
        const VariantParam *param = &operation->params[p];
        long value = relation->value[p];
        if (relation->kind == MOTIVE_RELATION_INVERT && p == 0 && value == -1) {
            if (motive->note_count == 0) {
                return false;
            }
            value = motive->notes[0].midi_pitch;
        }
        if (variant_step_set(step, param->name, value, relation->den[p])) {
            continue;
        }
        if (!param->optional) {
            return false;
        }
    }
    return true;
}

/* Replays a recognized chain without the step at skip (none when skip is the step count) and compares with target. */
static bool replay_chain(const Motive *source, const Motive *target, const MotiveRecognition *recognition, size_t skip,
                         VariantChain *chain, VariantRecord *record) {
    memset(chain, 0, sizeof(*chain));
    Motive current = *source;
    for (size_t s = 0; s < recognition->step_count; s++) {
        if (s == skip) {
            continue;
        }
        if (!step_from_relation(&recognition->steps[s], &current, &chain->steps[chain->step_count])) {
            return false;
        }
        chain->step_count++;
        /* The motive the next step is applied to. */
        VariantChain prefix = *chain;
        if (variant_apply_chain(source, &prefix, 0, &current, NULL) != VARIANT_OK) {
            return false;
        }
    }
    Motive replay;
    return chain->step_count > 0 && variant_apply_chain(source, chain, 0, &replay, record) == VARIANT_OK &&
           motive_equal(&replay, target);
}

/*
 * The steps the registry can replay (R-152): the relation has a row, the row takes the source's form and every parameter
 * is in the row's range. The recognizer skips any other proposal, so a chain it finds is one the registry replays.
 */
static bool admit_step(const MotiveRelationStep *relation, const void *context) {
    const Motive *source = context;
    const VariantOperation *operation = operation_of(relation->kind);
    VariantStep step;
    return operation && (operation->forms & VARIANT_FORM_BIT(source->form)) != 0 &&
           step_from_relation(relation, source, &step);
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
    /* motive_to_sketch gives a motive without chords of its own the best implied chord. A variant keeps no chord it did not
       get from a harmony row, so the implied chords would stop a transposition from matching a target that implies
       another chord; a harmony relation adds the implied chord to a source without chords itself. */
    if (source->harmony_count == 0) {
        from.chord_count = 0;
    }
    if (target->harmony_count == 0) {
        to.chord_count = 0;
    }
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.max_depth = depth;
    /* A contour form compares by rank (R-148): the Composer normalizes a contour after every step, so exact pitches would not
       reach it. */
    options.contour = source->form == MOTIVE_FORM_CONTOUR;
    options.admit = admit_step;
    options.admit_context = source;
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
        /* A chain that does not replay may hold a harmony step that only adds the implied chord to a motive without chords of
           its own; the registry refuses that step as unchanged, so each such step is tried left out (R-139). */
        for (size_t skip = 0; skip <= recognition.step_count && !replayed; skip++) {
            if (skip < recognition.step_count && recognition.steps[skip].kind != MOTIVE_RELATION_HARMONY_INVERSION &&
                recognition.steps[skip].kind != MOTIVE_RELATION_HARMONY_SUBSTITUTE) {
                continue;
            }
            replayed = replay_chain(source, target, &recognition, skip, chain, &record);
        }
        if (!replayed) {
            memset(chain, 0, sizeof(*chain));
        }
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
