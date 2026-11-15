#include <ursatz_composer/motive_develop.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_internal.h"
#include "util/plan_util.h"

/*
 * The developments of a motive. Each of the kinds applies the library relation of the same name
 * (analysis/motive_relation.h), so the registry, the recognizer and the develop operator share one implementation.
 */

/* Times the Composer holds: a numerator and a denominator up to a billion. */
#define TIME_LIMIT 1000000000LL

static bool time_fits(Rational time) {
    return msk_ok(time) && time.num >= 0 && time.num <= TIME_LIMIT && time.den <= TIME_LIMIT;
}

static MotiveDevResult relation_result(MotiveRelationResult result) {
    switch (result) {
    case MOTIVE_RELATION_OK:
        return MOTIVE_DEV_OK;
    case MOTIVE_RELATION_UNCHANGED:
        return MOTIVE_DEV_ERR_UNCHANGED;
    case MOTIVE_RELATION_RANGE:
        return MOTIVE_DEV_ERR_RANGE;
    case MOTIVE_RELATION_NOT_EXPRESSIBLE:
        return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
    case MOTIVE_RELATION_NO_SCALE:
        return MOTIVE_DEV_ERR_NO_KEY;
    case MOTIVE_RELATION_INVALID:
        return MOTIVE_DEV_ERR_INVALID;
    default:
        return MOTIVE_DEV_ERR_NOT_APPLICABLE;
    }
}

bool motive_overlay_sketch(const MotiveSketch *developed, const Motive *template_motive, Motive *out) {
    if (!developed || !template_motive || !out) {
        return false;
    }
    Motive result = *template_motive;
    PlanMeter meter;
    if (!motive_sketch_validate(developed) || !motive_meter_of(developed, &meter) || !time_fits(developed->pickup) ||
        32 % developed->pickup.den != 0) {
        return false;
    }
    result.bar = developed->bar;
    result.beat = developed->beat;
    result.pickup = developed->pickup;
    result.note_count = developed->note_count;
    for (size_t i = 0; i < developed->note_count; i++) {
        if (!time_fits(developed->notes[i].onset) || !time_fits(developed->notes[i].duration)) {
            return false;
        }
        result.notes[i] = developed->notes[i];
    }
    if (developed->has_scale && (!template_motive->has_scale || developed->tonic_pc != template_motive->tonic_pc ||
                                 developed->minor != template_motive->minor)) {
        AnalysisKey moved = plan_key_of_pitch_class(developed->tonic_pc, developed->minor ? KEY_STRUCTURE_MODE_MINOR
                                                                                           : KEY_STRUCTURE_MODE_MAJOR);
        motive_set_key(&result, &moved, MOTIVE_SKETCH_KEY_GIVEN);
    }
    result.chord_count = developed->chord_count;
    for (size_t i = 0; i < developed->chord_count; i++) {
        if (!time_fits(developed->chords[i].onset)) {
            return false;
        }
        result.chords[i] = developed->chords[i];
    }
    result.feature_count = developed->feature_count;
    memcpy(result.features, developed->features, sizeof(result.features));
    result.accompaniment = developed->accompaniment;
    /* A motive ends where its last note ends, whatever length the sketch gives. */
    if (motive_check_notes(&result, &result.length, NULL) != MOTIVE_OK || motive_check_extras(&result, NULL) != MOTIVE_OK) {
        return false;
    }
    *out = result;
    return true;
}

MotiveDevResult motive_relation_develop(const Motive *source, const MotiveRelationStep *step, bool keep_implied,
                                        Motive *out, MotiveRelationResult *applied) {
    if (!source || !step || !out) {
        return MOTIVE_DEV_ERR_NULL;
    }
    if (!motive_sketch_validate(source)) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    /* A full motive with a key and no harmony of its own is given its best implied chord at 0, so a relation that works from
       chords has one; the chord is not the variant's own and is left out of the result unless keep_implied (a harmony row). */
    MotiveSketch input = *source;
    MotiveSketchChord implied;
    if (input.chord_count == 0 && input.has_scale && input.form == MOTIVE_SKETCH_FORM_FULL &&
        motive_implied_chord(source, &implied)) {
        input.chords[0] = implied;
        input.chord_count = 1;
    }
    MotiveSketch developed;
    MotiveRelationResult library = motive_relation_apply(&input, step, &developed);
    if (applied) {
        *applied = library;
    }
    MotiveDevResult status = relation_result(library);
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    if (!keep_implied && source->chord_count == 0 && developed.chord_count == 1 && developed.has_scale &&
        msk_is_zero(developed.chords[0].onset) && motive_implied_chord(&developed, &implied) &&
        implied.degree == developed.chords[0].degree && implied.alteration == developed.chords[0].alteration &&
        implied.inversion == developed.chords[0].inversion && implied.seventh == developed.chords[0].seventh) {
        developed.chord_count = 0;
    }
    return motive_overlay_sketch(&developed, source, out) ? MOTIVE_DEV_OK : MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
}

MotiveDevResult motive_develop(const Motive *source, const MotiveRelationStep *step, Motive *out) {
    if (!source || !step || !out) {
        return MOTIVE_DEV_ERR_NULL;
    }
    Rational length;
    if (motive_check_notes(source, &length, NULL) != MOTIVE_OK) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    /* The library refuses a run or a drop that is too short as not applicable; here it has its own result. */
    if (step->kind == MOTIVE_RELATION_FRAGMENT || step->kind == MOTIVE_RELATION_LIQUIDATE) {
        bool fragment = step->kind == MOTIVE_RELATION_FRAGMENT;
        long wanted = step->value[fragment ? 1 : 0];
        long min = step->value[2];
        size_t minimum = min > 0 ? (size_t)min : MOTIVE_DEV_MIN_LENGTH;
        if (wanted >= 0 && (fragment ? (size_t)wanted < minimum
                                     : (wanted > 0 && ((size_t)wanted >= source->note_count ||
                                                       source->note_count - (size_t)wanted < minimum)))) {
            return MOTIVE_DEV_ERR_TOO_SHORT;
        }
    }
    Motive result;
    MotiveDevResult status = motive_relation_develop(source, step, false, &result, NULL);
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    /* The development kinds do not change the harmony; a source without chords of its own has none. */
    if (source->chord_count == 0) {
        result.chord_count = 0;
    }
    if (step->kind != MOTIVE_RELATION_EXACT && motive_equal(source, &result)) {
        return MOTIVE_DEV_ERR_UNCHANGED;
    }
    *out = result;
    return MOTIVE_DEV_OK;
}

/* ----------------------------------------------------------------- names */

void motive_dev_label(const MotiveRelationStep *step, char *out, size_t size) {
    const long *v = step->value;
    switch (step->kind) {
    case MOTIVE_RELATION_EXACT:
        snprintf(out, size, "restate exact");
        break;
    case MOTIVE_RELATION_TRANSPOSE:
        snprintf(out, size, "transpose %+d %s", (int)v[1], v[0] != 0 ? "steps (tonal)" : "semitones");
        break;
    case MOTIVE_RELATION_INVERT:
        if (v[0] >= 0) {
            snprintf(out, size, "invert about %d%s", (int)v[0], v[1] != 0 ? " (tonal)" : "");
        } else {
            snprintf(out, size, "invert about the first note%s", v[1] != 0 ? " (tonal)" : "");
        }
        break;
    case MOTIVE_RELATION_RETROGRADE:
        snprintf(out, size, "retrograde");
        break;
    case MOTIVE_RELATION_FRAGMENT:
        snprintf(out, size, "fragment %s %zu notes", plan_fragment_position_name((int)v[0]), (size_t)v[1]);
        break;
    case MOTIVE_RELATION_AUGMENT:
    case MOTIVE_RELATION_DIMINISH: {
        bool augment = step->kind == MOTIVE_RELATION_AUGMENT;
        long num = v[0] != 0 ? v[0] : (augment ? 2 : 1);
        long den = step->den[0] != 0 ? step->den[0] : (augment ? 1 : 2);
        snprintf(out, size, "%s %ld:%ld", augment ? "augment" : "diminish", num, den);
        break;
    }
    case MOTIVE_RELATION_LIQUIDATE:
        snprintf(out, size, "liquidate drop %zu%s", (size_t)v[0], v[1] != 0 ? " (keep rhythm)" : "");
        break;
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        if (v[0] == 0) {
            snprintf(out, size, "interval change %+d on every interval%s", (int)v[1], v[2] != 0 ? " (keep contour)" : "");
        } else {
            snprintf(out, size, "interval change %+d on interval %d%s", (int)v[1], (int)v[0],
                     v[2] != 0 ? " (keep contour)" : "");
        }
        break;
    default:
        if (!motive_relation_step_text(step, out, size) && size > 0) {
            out[0] = '\0';
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
