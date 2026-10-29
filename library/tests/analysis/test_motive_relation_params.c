#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_relation.h"
#include "semantics/non_chord_tone.h"

/*
 * Motive relation parameter metadata: every kind and index, names equal motive_relation_param_name, ranges and
 * choices, the optional defaults accepted by motive_relation_apply, and a value out of range refused.
 */

static Rational q(unsigned long long n, unsigned long long d) {
    return rational_ratio(n, d);
}

/* Eight eighth notes C to C in 4/4, C major, two chords and two features: enough for every relation. */
static MotiveSketch rich(void) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    static const int pitches[8] = {60, 62, 64, 65, 67, 69, 71, 72};
    for (unsigned i = 0; i < 8; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = q(i, 8);
        sketch.notes[i].duration = q(1, 8);
    }
    sketch.note_count = 8;
    sketch.has_scale = true;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = q(1, 1);
    sketch.chords[0] = (MotiveSketchChord){q(0, 1), 1, 0, 0, false};
    sketch.chords[1] = (MotiveSketchChord){q(1, 2), 5, 0, 0, true};
    sketch.chord_count = 2;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    strcpy(sketch.features[1].label, "b");
    sketch.features[1].first = 2;
    sketch.features[1].last = 3;
    sketch.feature_count = 2;
    return sketch;
}

/* A step that applies to rich(), for the required parameters; ratios are given as value and den. */
static MotiveRelationStep base_step(MotiveRelationKind kind) {
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    long v[MOTIVE_RELATION_MAX_PARAMS] = {0};
    long d[MOTIVE_RELATION_MAX_PARAMS] = {0};
    switch (kind) {
    case MOTIVE_RELATION_TRANSPOSE: v[0] = 1; v[1] = 2; break;
    case MOTIVE_RELATION_INVERT: v[0] = -1; break;
    case MOTIVE_RELATION_INTERVAL_CHANGE: v[0] = 1; v[1] = 2; break;
    case MOTIVE_RELATION_FRAGMENT: v[0] = 1; v[1] = 4; break;
    case MOTIVE_RELATION_LIQUIDATE: v[0] = 1; break;
    case MOTIVE_RELATION_NOTE_REPEAT: v[0] = 0; v[1] = 2; break;
    case MOTIVE_RELATION_RHYTHM_REPEAT: v[0] = 0; v[1] = 2; v[2] = 4; break;
    case MOTIVE_RELATION_RHYTHM_SHIFT: v[0] = 1; d[0] = 8; break;
    case MOTIVE_RELATION_ADD_UPBEAT: v[0] = 1; v[1] = 1; break;
    case MOTIVE_RELATION_CHANGE_METER: v[0] = 3; v[1] = 4; break;
    case MOTIVE_RELATION_ADD_INTERVAL: v[0] = 1; v[1] = 1; break;
    case MOTIVE_RELATION_OMIT_INTERVAL: v[0] = 1; break;
    case MOTIVE_RELATION_ANCILLARY_FILL: v[0] = 1; break;
    case MOTIVE_RELATION_CONDENSE: v[0] = 0; v[1] = 1; v[2] = 1; d[2] = 2; break;
    case MOTIVE_RELATION_REPEAT_FEATURE: v[0] = 0; v[1] = 1; v[2] = 1; break;
    case MOTIVE_RELATION_SHIFT_FEATURE: v[0] = 0; v[1] = 1; d[1] = 8; break;
    case MOTIVE_RELATION_HARMONY_INVERSION: v[0] = 0; v[1] = 1; break;
    case MOTIVE_RELATION_HARMONY_APPEND: v[0] = 5; break;
    case MOTIVE_RELATION_HARMONY_INSERT: v[0] = 0; v[1] = 4; break;
    case MOTIVE_RELATION_HARMONY_SUBSTITUTE: v[0] = 0; v[1] = 4; break;
    case MOTIVE_RELATION_PASSING_HARMONY: v[0] = 0; break;
    case MOTIVE_RELATION_LIQUIDATE_FEATURES: v[0] = 1; break;
    case MOTIVE_RELATION_ADD_NON_CHORD_TONE: v[0] = 0; v[1] = NON_CHORD_TONE_NEIGHBOR; break;
    case MOTIVE_RELATION_REMOVE_NON_CHORD_TONE: v[0] = 1; break;
    case MOTIVE_RELATION_CHANGE_NON_CHORD_TONE: v[0] = 1; v[1] = NON_CHORD_TONE_NEIGHBOR; break;
    case MOTIVE_RELATION_REHARMONIZE_FRAME: v[0] = 5; break;
    default: break;
    }
    memcpy(step.value, v, sizeof(v));
    memcpy(step.den, d, sizeof(d));
    return step;
}

static void test_every_parameter(void) {
    for (size_t k = 0; k < MOTIVE_RELATION_KIND_COUNT; k++) {
        MotiveRelationKind kind = (MotiveRelationKind)k;
        size_t count = motive_relation_param_count(kind);
        MotiveRelationParamInfo info;
        for (size_t i = 0; i < count; i++) {
            assert(motive_relation_param_info(kind, i, &info));
            assert(info.name != NULL && strcmp(info.name, motive_relation_param_name(kind, i)) == 0);
            assert(info.description != NULL && info.description[0] != '\0');
            assert(info.min <= info.max);
            assert((int)info.kind >= MOTIVE_RELATION_PARAM_INT && info.kind <= MOTIVE_RELATION_PARAM_CHOICE);
            if (info.kind == MOTIVE_RELATION_PARAM_CHOICE) {
                assert(info.choices != NULL && info.choice_count > 0);
                assert((long)info.choice_count == info.max - info.min + 1);
                for (size_t c = 0; c < info.choice_count; c++) {
                    assert(info.choices[c] != NULL && info.choices[c][0] != '\0');
                }
            } else {
                assert(info.choices == NULL && info.choice_count == 0);
            }
            if (info.kind == MOTIVE_RELATION_PARAM_BOOL) {
                assert(info.min == 0 && info.max == 1 && info.optional);
            }
            if (info.kind != MOTIVE_RELATION_PARAM_RATIO) {
                assert(info.default_den == 0);
            }
        }
        /* Past the last parameter, and with no output. */
        assert(!motive_relation_param_info(kind, count, &info));
        assert(!motive_relation_param_info(kind, 0, NULL));
    }
    MotiveRelationParamInfo info;
    assert(!motive_relation_param_info(MOTIVE_RELATION_KIND_COUNT, 0, &info));
    assert(!motive_relation_param_info((MotiveRelationKind)-1, 0, &info));
    assert(!motive_relation_param_info(MOTIVE_RELATION_RETROGRADE, 0, &info));
}

static void test_known_values(void) {
    MotiveRelationParamInfo info;
    assert(motive_relation_param_info(MOTIVE_RELATION_INVERT, 0, &info));
    assert(info.kind == MOTIVE_RELATION_PARAM_INT && info.default_value == -1 && info.optional);
    assert(motive_relation_param_info(MOTIVE_RELATION_TRANSPOSE, 2, &info));
    assert(info.min == 1 && info.max == 7 && info.default_value == 0 && info.optional);
    assert(motive_relation_param_info(MOTIVE_RELATION_FRAGMENT, 0, &info));
    assert(info.kind == MOTIVE_RELATION_PARAM_CHOICE && info.min == 1 && info.max == 3);
    assert(strcmp(info.choices[0], "leading") == 0 && strcmp(info.choices[2], "middle") == 0);
    assert(motive_relation_param_info(MOTIVE_RELATION_AUGMENT, 0, &info));
    assert(info.kind == MOTIVE_RELATION_PARAM_RATIO && info.optional && info.default_value == 0 && info.default_den == 0);
    assert(motive_relation_param_info(MOTIVE_RELATION_RHYTHM_SHIFT, 0, &info));
    assert(info.kind == MOTIVE_RELATION_PARAM_RATIO && !info.optional);
    assert(motive_relation_param_info(MOTIVE_RELATION_NOTE_REPEAT, 1, &info));
    assert(info.min == 2 && info.max == 4 && !info.optional);
    assert(motive_relation_param_info(MOTIVE_RELATION_ADD_UPBEAT, 1, &info));
    assert(info.min == -1 && info.max == 1);
    /* The non-chord tone choices are the names of the NonChordTone values. */
    assert(motive_relation_param_info(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, &info));
    assert(info.kind == MOTIVE_RELATION_PARAM_CHOICE && info.min == 0 && info.max == NON_CHORD_TONE_COUNT - 1);
    for (int t = 0; t < NON_CHORD_TONE_COUNT; t++) {
        assert(strcmp(info.choices[t], non_chord_tone_name((NonChordTone)t)) == 0);
    }
}

/* The base step with every optional parameter at its default is not refused as malformed. */
static void test_defaults_accepted(void) {
    MotiveSketch input = rich();
    for (size_t k = 0; k < MOTIVE_RELATION_KIND_COUNT; k++) {
        MotiveRelationKind kind = (MotiveRelationKind)k;
        MotiveRelationStep step = base_step(kind);
        for (size_t i = 0; i < motive_relation_param_count(kind); i++) {
            MotiveRelationParamInfo info;
            assert(motive_relation_param_info(kind, i, &info));
            if (info.optional) {
                step.value[i] = info.default_value;
                step.den[i] = info.default_den;
            }
        }
        MotiveSketch out;
        MotiveRelationResult result = motive_relation_apply(&input, &step, &out);
        if (result == MOTIVE_RELATION_INVALID) {
            printf("%s: default step refused as invalid\n", motive_relation_name(kind));
        }
        assert(result != MOTIVE_RELATION_INVALID);
    }
}

/* A value one above the maximum of a parameter, or below the minimum of a required one, is not applied. */
static void test_out_of_range_refused(void) {
    MotiveSketch input = rich();
    for (size_t k = 0; k < MOTIVE_RELATION_KIND_COUNT; k++) {
        MotiveRelationKind kind = (MotiveRelationKind)k;
        for (size_t i = 0; i < motive_relation_param_count(kind); i++) {
            MotiveRelationParamInfo info;
            assert(motive_relation_param_info(kind, i, &info));
            if (info.kind == MOTIVE_RELATION_PARAM_BOOL) {
                continue;
            }
            for (int side = 0; side < 2; side++) {
                if (info.kind == MOTIVE_RELATION_PARAM_RATIO && side == 1) {
                    continue; /* a ratio has no upper bound but the meter */
                }
                MotiveRelationStep step = base_step(kind);
                step.value[i] = side == 0 ? info.min - 1 : info.max + 1;
                if (info.optional && side == 0) {
                    continue; /* an optional parameter reads anything below its range as left out */
                }
                if (info.kind == MOTIVE_RELATION_PARAM_RATIO && step.den[i] == 0) {
                    step.den[i] = 1;
                }
                MotiveSketch out;
                MotiveRelationResult result = motive_relation_apply(&input, &step, &out);
                if (result == MOTIVE_RELATION_OK) {
                    printf("%s %s: %ld applied\n", motive_relation_name(kind), info.name, step.value[i]);
                }
                assert(result != MOTIVE_RELATION_OK);
            }
        }
    }
}

int main(void) {
    test_every_parameter();
    test_known_values();
    test_defaults_accepted();
    test_out_of_range_refused();
    printf("motive relation parameter tests passed\n");
    return 0;
}
