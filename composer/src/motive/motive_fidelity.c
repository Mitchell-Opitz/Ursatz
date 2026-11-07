#include "motive/motive_fidelity.h"

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/variant_registry.h>

/*
 * Fidelity of an idea's realized notes to its source (R-354). Pure, on motives: the plan is read by grow_fidelity.c.
 * The heard chain is the recognizer's, searched to HEARD_DEPTH steps to bound its cost per candidate (R-307).
 */

#define HEARD_DEPTH 2
/* The transpose row's parameters: tonal (0/1), by, degree (variant_registry.c). */
#define PARAM_TONAL 0
#define PARAM_BY 1
#define OCTAVE_SEMITONES 12
#define OCTAVE_STEPS 7

/* value modulo m, never negative. */
static long modulo(long value, long m) {
    long r = value % m;
    return r < 0 ? r + m : r;
}

static bool is_transpose(const VariantStep *step) {
    return strcmp(step->op, "transpose") == 0;
}

static bool step_tonal(const VariantStep *step) {
    return step->values[PARAM_TONAL].set && step->values[PARAM_TONAL].value != 0;
}

static long step_by(const VariantStep *step) {
    return step->values[PARAM_BY].set ? step->values[PARAM_BY].value : 0;
}

/* Whether a transposition of `by` (tonal steps or semitones) is a whole number of octaves. */
static bool whole_octaves(bool tonal, long by) {
    return modulo(by, tonal ? OCTAVE_STEPS : OCTAVE_SEMITONES) == 0;
}

/* The chain without its octave transpositions and its exact steps (the recognizer's chain for a motive that is the source is one exact step). */
static VariantChain drop_octaves(const VariantChain *chain) {
    VariantChain kept;
    memset(&kept, 0, sizeof(kept));
    for (size_t i = 0; i < chain->step_count && i < VARIANT_CHAIN_MAX; i++) {
        const VariantStep *step = &chain->steps[i];
        bool octave = is_transpose(step) && whole_octaves(step_tonal(step), step_by(step));
        if (!octave && strcmp(step->op, "exact") != 0) {
            kept.steps[kept.step_count++] = *step;
        }
    }
    return kept;
}

/* Appends text to the heard string, cut at its size. */
static void append(char *heard, size_t size, const char *text) {
    size_t used = strlen(heard);
    if (used + 1 < size) {
        snprintf(heard + used, size - used, "%s", text);
    }
}

static void write_heard(const VariantChain *chain, char *heard, size_t size) {
    heard[0] = '\0';
    if (chain->step_count == 0) {
        snprintf(heard, size, "exact");
        return;
    }
    for (size_t i = 0; i < chain->step_count; i++) {
        const VariantStep *step = &chain->steps[i];
        const VariantOperation *row = variant_registry_find(step->op);
        char phrase[96];
        phrase[0] = '\0';
        if (row && row->describe) {
            row->describe(step->values, phrase, sizeof(phrase));
        }
        if (phrase[0] == '\0') {
            snprintf(phrase, sizeof(phrase), "%s", row && row->label ? row->label : step->op);
        }
        if (i > 0) {
            append(heard, size, ", then ");
        }
        append(heard, size, phrase);
    }
}

/* Whether the chain holds a step of the relation. */
static bool holds_relation(const VariantChain *chain, int relation) {
    for (size_t i = 0; i < chain->step_count; i++) {
        const VariantOperation *row = variant_registry_find(chain->steps[i].op);
        if (row && row->relation == relation) {
            return true;
        }
    }
    return false;
}

static bool holds_op(const VariantChain *chain, const char *op) {
    for (size_t i = 0; i < chain->step_count; i++) {
        if (strcmp(chain->steps[i].op, op) == 0) {
            return true;
        }
    }
    return false;
}

static bool holds_liquidation(const VariantChain *chain) {
    return holds_op(chain, "fragment") || holds_op(chain, "liquidate") || holds_op(chain, "liquidate-features") ||
           holds_op(chain, "condense");
}

/* A transposition by the same steps (tonal, mod 7) or semitones (real, mod 12) as the ref's, which is none when whole octaves. */
static bool agrees_transposition(const VariantChain *heard, const MotiveRelationStep *step) {
    bool tonal = step->value[0] != 0;
    if (whole_octaves(tonal, step->value[1])) {
        return heard->step_count == 0;
    }
    if (heard->step_count != 1 || !is_transpose(&heard->steps[0]) || step_tonal(&heard->steps[0]) != tonal) {
        return false;
    }
    long modulus = tonal ? OCTAVE_STEPS : OCTAVE_SEMITONES;
    return modulo(step_by(&heard->steps[0]), modulus) == modulo(step->value[1], modulus);
}

/* The agreement rule of R-354, on the chain without its octave transpositions. */
static bool agrees_with(const VariantChain *heard, const PlanMotiveRef *ref, bool from_segment) {
    if (ref->steps > 0 || (ref->has_step && ref->step.kind == MOTIVE_RELATION_LIQUIDATE)) {
        return holds_liquidation(heard);
    }
    if (!ref->has_step || plan_motive_ref_restates(ref) ||
        (from_segment && ref->step.kind == MOTIVE_RELATION_FRAGMENT)) {
        return heard->step_count == 0;
    }
    switch (ref->step.kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        return agrees_transposition(heard, &ref->step);
    case MOTIVE_RELATION_DOMINANT_FORM:
        return heard->step_count == 0 || (heard->step_count == 1 && is_transpose(&heard->steps[0]));
    default:
        return holds_relation(heard, (int)ref->step.kind);
    }
}

bool motive_fidelity(const Motive *source, const Motive *realized, const PlanMotiveRef *ref, bool from_segment,
                     MotiveFidelity *out) {
    MotiveRecognizability recognizability;
    VariantChain chain;
    VariantRecognition recognition;
    if (!source || !realized || !ref || !out || !motive_recognizability(source, realized, NULL, &recognizability)) {
        return false;
    }
    memset(&chain, 0, sizeof(chain));
    memset(&recognition, 0, sizeof(recognition));
    if (variant_recognize_depth(source, realized, HEARD_DEPTH, &chain, &recognition) != VARIANT_OK) {
        return false;
    }
    MotiveFidelity result;
    memset(&result, 0, sizeof(result));
    result.score = recognizability.total;
    result.found = recognition.found;
    if (result.found) {
        VariantChain heard = drop_octaves(&chain);
        write_heard(&heard, result.heard, sizeof(result.heard));
        result.agrees = agrees_with(&heard, ref, from_segment);
    } else {
        snprintf(result.heard, sizeof(result.heard), "not recognized");
    }
    *out = result;
    return true;
}
