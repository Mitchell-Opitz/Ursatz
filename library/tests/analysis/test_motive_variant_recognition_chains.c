#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_variant_recognition.h"
#include "pitch/pitch_spelling.h"

/*
 * The three depth two variants the Composer's variant round trip did not recover. The notes are the
 * Composer's C major fixture ("C4:q E4:e G4:e F4:q E4:q", 4/4, features a = 0..1 and b = 3..4) and the
 * variants the Composer made from it on seeds 32, 41 and 42. The Composer's relations see the source with its
 * best implied chord (I) at 0, and each variant keeps that chord, so the recognizer is given the chordless source and
 * the chains replay from the source with the chord.
 *
 * Some notes are explained equally well by more than one chain, so each case lists the set of explanations the
 * recognizer may return and checks that the chain the Composer made is one of the ways to reach the variant:
 *   seed 32: fragment then omit-interval is the only chain of two found.
 *   seed 41: liquidate-features then liquidate (cost 4) and liquidate alone from the source with its implied chord
 *            (cost 2) give the same notes; the notes cannot tell whether the chord came from the step or from the
 *            source, so the cheaper one is returned.
 *   seed 42: interval-change then ancillary-fill and add-interval then interval-change (both cost 4) give the same
 *            notes; the first in relation order is returned.
 */

typedef struct {
    int pitch;
    unsigned long long onset_num, onset_den, duration_num, duration_den;
} Note;

typedef struct {
    const char *name;
    Note notes[8];
    size_t note_count;
    unsigned long long length_num, length_den;
    struct {
        const char *label;
        size_t first, last;
    } features[2];
    size_t feature_count;
    bool has_chord;
} Shape;

static MotiveSketch build(const Shape *shape) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    for (size_t i = 0; i < shape->note_count; i++) {
        sketch.notes[i].pitch = shape->notes[i].pitch;
        sketch.notes[i].onset = rational_ratio(shape->notes[i].onset_num, shape->notes[i].onset_den);
        sketch.notes[i].duration = rational_ratio(shape->notes[i].duration_num, shape->notes[i].duration_den);
    }
    sketch.note_count = shape->note_count;
    sketch.has_scale = true;
    sketch.tonic_pc = 0;
    sketch.minor = false;
    sketch.key_source = MOTIVE_SKETCH_KEY_GIVEN;
    sketch.has_tonic_spelling = true;
    sketch.tonic_letter = PITCH_LETTER_C;
    sketch.tonic_accidental = 0;
    sketch.bar = rational_ratio(1, 1);
    sketch.beat = rational_ratio(1, 4);
    sketch.pickup = rational_ratio(0, 1);
    sketch.length = rational_ratio(shape->length_num, shape->length_den);
    for (size_t i = 0; i < shape->feature_count; i++) {
        strcpy(sketch.features[i].label, shape->features[i].label);
        sketch.features[i].first = shape->features[i].first;
        sketch.features[i].last = shape->features[i].last;
    }
    sketch.feature_count = shape->feature_count;
    if (shape->has_chord) {
        sketch.chord_count = 1;
        sketch.chords[0].onset = rational_ratio(0, 1);
        sketch.chords[0].degree = 1;
    }
    assert(motive_sketch_validate(&sketch));
    return sketch;
}

static const Shape SOURCE = {
    "source",
    {{60, 0, 1, 1, 4}, {64, 1, 4, 1, 8}, {67, 3, 8, 1, 8}, {65, 1, 2, 1, 4}, {64, 3, 4, 1, 4}},
    5, 1, 1, {{"a", 0, 1}, {"b", 3, 4}}, 2, false};

/* fragment(position=3, length=4) then omit-interval(interval=1) */
static const Shape SEED_32 = {
    "seed 32 (fragment, omit-interval)",
    {{60, 0, 1, 3, 8}, {67, 3, 8, 1, 8}, {65, 1, 2, 1, 4}},
    3, 3, 4, {{"a", 0, 0}}, 1, true};

/* liquidate-features(steps=1, shorten=0) then liquidate(drop=2, keep-rhythm=1) */
static const Shape SEED_41 = {
    "seed 41 (liquidate-features, liquidate)",
    {{60, 0, 1, 1, 4}, {64, 1, 4, 3, 4}},
    2, 1, 1, {{"a", 0, 1}}, 1, true};

/* interval-change(interval=3, amount=2, keep-contour=0) then ancillary-fill(interval=1, kind=0) */
static const Shape SEED_42 = {
    "seed 42 (interval-change, ancillary-fill)",
    {{60, 0, 1, 1, 8}, {62, 1, 8, 1, 8}, {64, 1, 4, 1, 8}, {67, 3, 8, 1, 8}, {63, 1, 2, 1, 4}, {62, 3, 4, 1, 4}},
    6, 1, 1, {{"a", 0, 2}, {"b", 4, 5}}, 2, true};

static MotiveRelationStep make(MotiveRelationKind kind, long a, long b, long c) {
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    step.value[0] = a;
    step.value[1] = b;
    step.value[2] = c;
    return step;
}

static MotiveSketch settled(const MotiveSketch *sketch) {
    MotiveRelationStep exact = make(MOTIVE_RELATION_EXACT, 0, 0, 0);
    MotiveSketch out;
    assert(motive_relation_apply(sketch, &exact, &out) == MOTIVE_RELATION_OK);
    return out;
}

static MotiveSketch replay(const MotiveSketch *from, const MotiveRelationStep *steps, size_t count) {
    MotiveSketch at = *from;
    for (size_t i = 0; i < count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&at, &steps[i], &next) == MOTIVE_RELATION_OK);
        at = next;
    }
    return at;
}

typedef struct {
    MotiveRelationKind kinds[2];
    size_t count;
} Explanation;

static bool is_one_of(const MotiveRecognition *found, const Explanation *set, size_t set_count) {
    for (size_t e = 0; e < set_count; e++) {
        bool same = found->step_count == set[e].count;
        for (size_t i = 0; same && i < set[e].count; i++) {
            same = found->steps[i].kind == set[e].kinds[i];
        }
        if (same) {
            return true;
        }
    }
    return false;
}

/*
 * The recognizer's chain from the chordless source must replay from the source with its implied chord to the
 * variant and be one of the explanations; the chain the Composer made must reach the same variant.
 */
static void check(const MotiveSketch *source, const MotiveSketch *given, const Shape *variant,
                  const MotiveRelationStep made[2], const Explanation *set, size_t set_count) {
    MotiveSketch target = build(variant);
    MotiveSketch want = settled(&target);
    MotiveSketch made_result = replay(given, made, 2);
    assert(motive_sketch_equal(&made_result, &want));

    MotiveRecognition found;
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    assert(motive_variant_recognize(source, &target, &options, &found) == MOTIVE_RECOGNIZE_OK);
    if (!found.found) {
        fprintf(stderr, "%s: not recovered\n", variant->name);
    }
    assert(found.found);
    if (!is_one_of(&found, set, set_count)) {
        fprintf(stderr, "%s: unexpected chain of %zu steps\n", variant->name, found.step_count);
    }
    assert(is_one_of(&found, set, set_count));
    MotiveSketch result = replay(given, found.steps, found.step_count);
    assert(motive_sketch_equal(&result, &want));
}

int main(void) {
    MotiveSketch source = build(&SOURCE);
    /* The source as the Composer's relations see it: with its best implied chord (I) at 0. */
    Shape with_chord = SOURCE;
    with_chord.has_chord = true;
    MotiveSketch given = build(&with_chord);
    const MotiveRelationStep made_32[2] = {make(MOTIVE_RELATION_FRAGMENT, 1, 4, 0), make(MOTIVE_RELATION_OMIT_INTERVAL, 1, 0, 0)};
    const Explanation set_32[] = {{{MOTIVE_RELATION_FRAGMENT, MOTIVE_RELATION_OMIT_INTERVAL}, 2}};
    check(&source, &given, &SEED_32, made_32, set_32, 1);

    const MotiveRelationStep made_41[2] = {make(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), make(MOTIVE_RELATION_LIQUIDATE, 2, 1, 0)};
    const Explanation set_41[] = {{{MOTIVE_RELATION_LIQUIDATE_FEATURES, MOTIVE_RELATION_LIQUIDATE}, 2},
                                  {{MOTIVE_RELATION_LIQUIDATE}, 1}};
    check(&source, &given, &SEED_41, made_41, set_41, 2);

    const MotiveRelationStep made_42[2] = {make(MOTIVE_RELATION_INTERVAL_CHANGE, 3, 2, 0), make(MOTIVE_RELATION_ANCILLARY_FILL, 1, 0, 0)};
    const Explanation set_42[] = {{{MOTIVE_RELATION_INTERVAL_CHANGE, MOTIVE_RELATION_ANCILLARY_FILL}, 2},
                                  {{MOTIVE_RELATION_ADD_INTERVAL, MOTIVE_RELATION_INTERVAL_CHANGE}, 2}};
    check(&source, &given, &SEED_42, made_42, set_42, 2);
    printf("motive_variant_recognition_chains: ok\n");
    return 0;
}
