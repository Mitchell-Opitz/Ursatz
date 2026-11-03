#include <ursatz_composer/motive_develop.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_sketch_bridge.h>

#include "motive/motive_internal.h"
#include "util/plan_util.h"

/*
 * The developments of a motive. Each of the kinds applies the library relation of the same name
 * (analysis/motive_relation.h), so the registry, the recognizer and the develop operator share one implementation.
 */

/* The library relation of the development and its parameters, in the library's order (motive_relation.h). */
static bool relation_step(const MotiveDevSpec *spec, MotiveRelationStep *step) {
    memset(step, 0, sizeof(*step));
    switch (spec->kind) {
    case MOTIVE_DEV_EXACT:
        step->kind = MOTIVE_RELATION_EXACT;
        return true;
    case MOTIVE_DEV_TRANSPOSE:
        step->kind = MOTIVE_RELATION_TRANSPOSE;
        step->value[0] = spec->tonal ? 1 : 0;
        step->value[1] = spec->amount;
        return true;
    case MOTIVE_DEV_INVERT:
        step->kind = MOTIVE_RELATION_INVERT;
        step->value[0] = spec->has_axis ? spec->axis : -1;
        step->value[1] = spec->tonal ? 1 : 0;
        return true;
    case MOTIVE_DEV_RETROGRADE:
        step->kind = MOTIVE_RELATION_RETROGRADE;
        return true;
    case MOTIVE_DEV_INTERVAL_CHANGE:
        step->kind = MOTIVE_RELATION_INTERVAL_CHANGE;
        step->value[0] = spec->interval;
        step->value[1] = spec->amount;
        step->value[2] = spec->keep_contour ? 1 : 0;
        return true;
    case MOTIVE_DEV_FRAGMENT:
        step->kind = MOTIVE_RELATION_FRAGMENT;
        step->value[0] = spec->position;
        step->value[1] = (long)spec->length;
        step->value[2] = (long)spec->min_length;
        return true;
    case MOTIVE_DEV_LIQUIDATE:
        step->kind = MOTIVE_RELATION_LIQUIDATE;
        step->value[0] = (long)spec->drop;
        step->value[1] = spec->keep_rhythm ? 1 : 0;
        step->value[2] = (long)spec->min_length;
        return true;
    case MOTIVE_DEV_AUGMENT:
    case MOTIVE_DEV_DIMINISH:
        step->kind = spec->kind == MOTIVE_DEV_AUGMENT ? MOTIVE_RELATION_AUGMENT : MOTIVE_RELATION_DIMINISH;
        step->value[0] = (long)spec->ratio_num;
        step->den[0] = (long)spec->ratio_den;
        return true;
    }
    return false;
}

MotiveDevResult motive_develop(const Motive *source, const MotiveDevSpec *spec, Motive *out) {
    if (!source || !spec || !out) {
        return MOTIVE_DEV_ERR_NULL;
    }
    Rational length;
    if (motive_check_notes(source, &length, NULL) != MOTIVE_OK) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    MotiveRelationStep step;
    if (!relation_step(spec, &step)) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    /* The library refuses a run or a drop that is too short as not applicable; here it has its own result. */
    size_t minimum = spec->min_length > 0 ? spec->min_length : MOTIVE_DEV_MIN_LENGTH;
    if ((spec->kind == MOTIVE_DEV_FRAGMENT && spec->length < minimum) ||
        (spec->kind == MOTIVE_DEV_LIQUIDATE && spec->drop > 0 &&
         (spec->drop >= source->note_count || source->note_count - spec->drop < minimum))) {
        return MOTIVE_DEV_ERR_TOO_SHORT;
    }
    MotiveSketch sketch;
    MotiveSketch developed;
    if (!motive_to_sketch(source, &sketch)) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    MotiveDevResult status = motive_sketch_result(motive_relation_apply(&sketch, &step, &developed));
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    Motive result;
    if (!motive_from_sketch(&developed, source, &result)) {
        return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
    }
    /* The implied chord the sketch was given for the relation is not the variant's own harmony. */
    if (source->harmony_count == 0) {
        result.harmony_count = 0;
    }
    if (spec->kind != MOTIVE_DEV_EXACT && motive_equal(source, &result)) {
        return MOTIVE_DEV_ERR_UNCHANGED;
    }
    *out = result;
    return MOTIVE_DEV_OK;
}

/* ----------------------------------------------------------------- names */

void motive_dev_label(const MotiveDevSpec *spec, char *out, size_t size) {
    switch (spec->kind) {
    case MOTIVE_DEV_EXACT:
        snprintf(out, size, "restate exact");
        break;
    case MOTIVE_DEV_TRANSPOSE:
        snprintf(out, size, "transpose %+d %s", spec->amount, spec->tonal ? "steps (tonal)" : "semitones");
        break;
    case MOTIVE_DEV_INVERT:
        if (spec->has_axis) {
            snprintf(out, size, "invert about %d%s", spec->axis, spec->tonal ? " (tonal)" : "");
        } else {
            snprintf(out, size, "invert about the first note%s", spec->tonal ? " (tonal)" : "");
        }
        break;
    case MOTIVE_DEV_RETROGRADE:
        snprintf(out, size, "retrograde");
        break;
    case MOTIVE_DEV_FRAGMENT: {
        snprintf(out, size, "fragment %s %zu notes", plan_fragment_position_name(spec->position), spec->length);
        break;
    }
    case MOTIVE_DEV_AUGMENT:
    case MOTIVE_DEV_DIMINISH: {
        bool augment = spec->kind == MOTIVE_DEV_AUGMENT;
        unsigned long num = spec->ratio_num != 0 ? spec->ratio_num : (augment ? 2 : 1);
        unsigned long den = spec->ratio_den != 0 ? spec->ratio_den : (augment ? 1 : 2);
        snprintf(out, size, "%s %lu:%lu", augment ? "augment" : "diminish", num, den);
        break;
    }
    case MOTIVE_DEV_LIQUIDATE:
        snprintf(out, size, "liquidate drop %zu%s", spec->drop, spec->keep_rhythm ? " (keep rhythm)" : "");
        break;
    case MOTIVE_DEV_INTERVAL_CHANGE:
        if (spec->interval == 0) {
            snprintf(out, size, "interval change %+d on every interval%s", spec->amount,
                     spec->keep_contour ? " (keep contour)" : "");
        } else {
            snprintf(out, size, "interval change %+d on interval %d%s", spec->amount, spec->interval,
                     spec->keep_contour ? " (keep contour)" : "");
        }
        break;
    }
}

static const char *const RESULT_NAMES[] = {"ok",        "null",      "alloc",          "no key",
                                           "range",     "unchanged", "not applicable", "too short",
                                           "not expressible", "invalid",    "no feature", "no accompaniment"};

const char *motive_dev_result_name(MotiveDevResult result) {
    return (size_t)result < sizeof(RESULT_NAMES) / sizeof(RESULT_NAMES[0]) ? RESULT_NAMES[result] : "unknown";
}
