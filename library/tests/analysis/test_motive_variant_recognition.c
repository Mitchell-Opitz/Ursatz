#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_variant_recognition.h"

/*
 * Analysis-regression tests for the variant recognizer. Each relation is applied to a fixture, the result is
 * recognized, and the recovered chain is replayed: replayed through motive_relation_apply it must give the target
 * (the op names may differ from the chain that made it).
 */

static ExactRational q(unsigned long long n, unsigned long long d) {
    return er_make(n, d);
}

/* C D E G F E in C major, 4/4: q e e q e e, with two labelled features and the chords I and V. */
static MotiveSketch base(void) {
    static const int pitches[6] = {60, 62, 64, 67, 65, 64};
    static const unsigned long long den[6] = {4, 8, 8, 4, 8, 8};
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < 6; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = at;
        sketch.notes[i].duration = q(1, den[i]);
        at = msk_add(at, sketch.notes[i].duration);
    }
    sketch.note_count = 6;
    sketch.has_scale = true;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = at;
    sketch.feature_count = 2;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 2;
    strcpy(sketch.features[1].label, "b");
    sketch.features[1].first = 3;
    sketch.features[1].last = 5;
    sketch.chord_count = 2;
    sketch.chords[0].onset = q(0, 1);
    sketch.chords[0].degree = 1;
    sketch.chords[1].onset = q(1, 2);
    sketch.chords[1].degree = 5;
    return sketch;
}

static MotiveRelationStep step(MotiveRelationKind kind, long a, long b, long c) {
    MotiveRelationStep result;
    memset(&result, 0, sizeof(result));
    result.kind = kind;
    result.value[0] = a;
    result.value[1] = b;
    result.value[2] = c;
    return result;
}

static MotiveRelationStep ratio_step(MotiveRelationKind kind, long a, long b, long c, size_t which, long num,
                                     long den) {
    MotiveRelationStep result = step(kind, a, b, c);
    result.value[which] = num;
    result.den[which] = den;
    return result;
}

/* The sketch as every relation's result is: settled. */
static MotiveSketch settled(const MotiveSketch *sketch) {
    MotiveSketch out;
    MotiveRelationStep exact = step(MOTIVE_RELATION_EXACT, 0, 0, 0);
    assert(motive_relation_apply(sketch, &exact, &out) == MOTIVE_RELATION_OK);
    return out;
}

/* Replays a recognition from the source and checks it ends at the target. */
static void assert_replays(const MotiveSketch *source, const MotiveSketch *target, const MotiveRecognition *found) {
    assert(found->found);
    assert(found->step_count >= 1);
    MotiveSketch at = *source;
    int cost = 0;
    for (size_t i = 0; i < found->step_count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&at, &found->steps[i], &next) == MOTIVE_RELATION_OK);
        cost += motive_relation_cost(found->steps[i].kind);
        at = next;
    }
    MotiveSketch want = settled(target);
    assert(motive_sketch_equal(&at, &want));
    ExactRational distance = er_make((unsigned long long)cost, (unsigned long long)cost + 4);
    assert(msk_eq(found->distance, distance));
}

static MotiveRecognition recognize(const MotiveSketch *source, const MotiveSketch *target) {
    MotiveRecognition found;
    MotiveSketch before = *source;
    assert(motive_variant_recognize(source, target, NULL, &found) == MOTIVE_RECOGNIZE_OK);
    assert(memcmp(&before, source, sizeof(before)) == 0);
    return found;
}

/* Applies one relation to the source, recognizes the result and replays it. */
static MotiveRecognition recover_one(const MotiveSketch *source, MotiveRelationStep relation) {
    MotiveSketch target;
    MotiveRelationResult result = motive_relation_apply(source, &relation, &target);
    if (result != MOTIVE_RELATION_OK) {
        fprintf(stderr, "%s refused: %s\n", motive_relation_name(relation.kind), motive_relation_result_name(result));
    }
    assert(result == MOTIVE_RELATION_OK);
    MotiveRecognition found = recognize(source, &target);
    if (!found.found) {
        fprintf(stderr, "%s not recovered\n", motive_relation_name(relation.kind));
    }
    assert_replays(source, &target, &found);
    assert(found.step_count == 1);
    return found;
}

/* Eight eighth notes, four on C and four on B, in I then V: an accompaniment. */
static MotiveSketch repeated_notes(void) {
    MotiveSketch sketch = base();
    for (size_t i = 0; i < 6; i++) {
        sketch.notes[i].pitch = i < 3 ? 60 : 59;
        sketch.notes[i].onset = q(i, 8);
        sketch.notes[i].duration = q(1, 8);
    }
    sketch.notes[6] = sketch.notes[5];
    sketch.notes[6].onset = q(6, 8);
    sketch.notes[7] = sketch.notes[5];
    sketch.notes[7].onset = q(7, 8);
    sketch.note_count = 8;
    sketch.length = q(1, 1);
    sketch.feature_count = 0;
    sketch.accompaniment = true;
    return sketch;
}

static void test_every_relation(void) {
    MotiveSketch source = base();
    const MotiveRelationStep cases[] = {
        step(MOTIVE_RELATION_TRANSPOSE, 0, 5, 0),
        step(MOTIVE_RELATION_TRANSPOSE, 1, 2, 0),
        step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 5),
        step(MOTIVE_RELATION_INVERT, -1, 0, 0),
        step(MOTIVE_RELATION_INVERT, 62, 0, 0),
        step(MOTIVE_RELATION_INVERT, -1, 1, 0),
        step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0),
        step(MOTIVE_RELATION_INTERVAL_CHANGE, 2, 2, 0),
        step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 1, 0),
        step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0),
        step(MOTIVE_RELATION_FRAGMENT, 2, 4, 0),
        step(MOTIVE_RELATION_FRAGMENT, 3, 2, 0),
        step(MOTIVE_RELATION_LIQUIDATE, 2, 0, 0),
        step(MOTIVE_RELATION_LIQUIDATE, 1, 1, 0),
        step(MOTIVE_RELATION_AUGMENT, 0, 0, 0),
        step(MOTIVE_RELATION_DIMINISH, 0, 0, 0),
        ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0, 0, 0, 3, 2),
        step(MOTIVE_RELATION_NOTE_REPEAT, 0, 2, 0),
        step(MOTIVE_RELATION_NOTE_REPEAT, 3, 4, 0),
        step(MOTIVE_RELATION_RHYTHM_REPEAT, 1, 2, 3),
        ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, 1, 8),
        step(MOTIVE_RELATION_ADD_UPBEAT, 1, 1, 0),
        step(MOTIVE_RELATION_ADD_UPBEAT, 2, -1, 0),
        step(MOTIVE_RELATION_CHANGE_METER, 2, 4, 0),
        step(MOTIVE_RELATION_ADD_INTERVAL, 2, 1, 0),
        step(MOTIVE_RELATION_OMIT_INTERVAL, 2, 0, 0),
        step(MOTIVE_RELATION_ANCILLARY_FILL, 3, 0, 0),
        ratio_step(MOTIVE_RELATION_CONDENSE, 0, 2, 0, 2, 1, 2),
        step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 1),
        ratio_step(MOTIVE_RELATION_SHIFT_FEATURE, 1, 0, 0, 1, 1, 16),
        step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0),
        step(MOTIVE_RELATION_HARMONY_APPEND, 4, 0, 0),
        step(MOTIVE_RELATION_HARMONY_INSERT, 1, 4, 0),
        step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 1, 4, 0),
        step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0),
        step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0),
        step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0),
    };
    bool seen[MOTIVE_RELATION_KIND_COUNT] = {false};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        recover_one(&source, cases[i]);
        seen[cases[i].kind] = true;
    }
    /* The counter-line works on an accompaniment of repeated notes. */
    MotiveSketch accompaniment = repeated_notes();
    recover_one(&accompaniment, step(MOTIVE_RELATION_COUNTER_LINE, 0, 0, 0));
    seen[MOTIVE_RELATION_COUNTER_LINE] = true;
    for (int kind = 1; kind < MOTIVE_RELATION_KIND_COUNT; kind++) {
        assert(seen[kind]);
    }
}

static void test_equal_sketches_are_exact(void) {
    MotiveSketch source = base();
    MotiveRecognition found = recognize(&source, &source);
    assert(found.found);
    assert(found.step_count == 1);
    assert(found.steps[0].kind == MOTIVE_RELATION_EXACT);
    assert(msk_eq(found.distance, q(0, 1)));
    for (size_t d = 0; d < MOTIVE_DIMENSION_COUNT; d++) {
        assert(found.kept[d] && !found.changed[d] && found.similarity[d] == 1.0);
    }
}

static void test_dimensions(void) {
    MotiveSketch source = base();
    MotiveSketch target;
    MotiveRelationStep up = step(MOTIVE_RELATION_TRANSPOSE, 1, 2, 0);
    assert(motive_relation_apply(&source, &up, &target) == MOTIVE_RELATION_OK);
    MotiveRecognition found = recognize(&source, &target);
    assert(found.kept[MOTIVE_DIMENSION_RHYTHM] && found.kept[MOTIVE_DIMENSION_INTERVALS] &&
           found.kept[MOTIVE_DIMENSION_CONTOUR] && found.kept[MOTIVE_DIMENSION_HARMONY]);

    MotiveRelationStep invert = step(MOTIVE_RELATION_INVERT, -1, 0, 0);
    assert(motive_relation_apply(&source, &invert, &target) == MOTIVE_RELATION_OK);
    found = recognize(&source, &target);
    assert(found.kept[MOTIVE_DIMENSION_RHYTHM] && found.changed[MOTIVE_DIMENSION_INTERVALS] &&
           found.changed[MOTIVE_DIMENSION_CONTOUR]);

    MotiveRelationStep augment = step(MOTIVE_RELATION_AUGMENT, 0, 0, 0);
    assert(motive_relation_apply(&source, &augment, &target) == MOTIVE_RELATION_OK);
    found = recognize(&source, &target);
    assert(found.kept[MOTIVE_DIMENSION_RHYTHM] && found.kept[MOTIVE_DIMENSION_INTERVALS]);

    MotiveRelationStep shorter = step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0);
    assert(motive_relation_apply(&source, &shorter, &target) == MOTIVE_RELATION_OK);
    found = recognize(&source, &target);
    assert(found.changed[MOTIVE_DIMENSION_RHYTHM] && found.similarity[MOTIVE_DIMENSION_RHYTHM] < 1.0);
}

static MotiveSketch rhythm_only(void) {
    MotiveSketch sketch = base();
    for (size_t i = 0; i < sketch.note_count; i++) {
        sketch.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    sketch.has_scale = false;
    sketch.chord_count = 0;
    sketch.feature_count = 0;
    return sketch;
}

static void test_rhythm_sketch_takes_time_relations(void) {
    MotiveSketch source = rhythm_only();
    recover_one(&source, step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0));
    recover_one(&source, step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0));
    recover_one(&source, step(MOTIVE_RELATION_AUGMENT, 0, 0, 0));
    recover_one(&source, step(MOTIVE_RELATION_NOTE_REPEAT, 0, 2, 0));
    recover_one(&source, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, 1, 8));
    /* A pitch change is not a rhythm sketch's to make: against a pitched target nothing is found. */
    MotiveSketch pitched = base();
    pitched.has_scale = false;
    pitched.chord_count = 0;
    pitched.feature_count = 0;
    MotiveRecognition found = recognize(&source, &pitched);
    assert(!found.found);
}

static void test_without_a_scale_pitches_move_by_semitones(void) {
    MotiveSketch source = base();
    source.has_scale = false;
    source.chord_count = 0;
    recover_one(&source, step(MOTIVE_RELATION_TRANSPOSE, 0, 3, 0));
    recover_one(&source, step(MOTIVE_RELATION_INVERT, -1, 0, 0));
    recover_one(&source, step(MOTIVE_RELATION_INTERVAL_CHANGE, 2, 1, 0));
}

static void test_bad_input(void) {
    MotiveSketch source = base();
    MotiveSketch broken = base();
    broken.note_count = 0;
    MotiveRecognition found;
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    assert(options.max_depth == 4 && options.beam == 64);
    assert(motive_variant_recognize(&source, &broken, NULL, &found) == MOTIVE_RECOGNIZE_INVALID);
    assert(motive_variant_recognize(&broken, &source, NULL, &found) == MOTIVE_RECOGNIZE_INVALID);
    assert(motive_variant_recognize(&source, &source, NULL, NULL) == MOTIVE_RECOGNIZE_INVALID);
    options.max_depth = 7;
    assert(motive_variant_recognize(&source, &source, &options, &found) == MOTIVE_RECOGNIZE_INVALID);
    options.max_depth = 0;
    assert(motive_variant_recognize(&source, &source, &options, &found) == MOTIVE_RECOGNIZE_INVALID);
    motive_recognize_options_default(&options);
    options.beam = 0;
    assert(motive_variant_recognize(&source, &source, &options, &found) == MOTIVE_RECOGNIZE_INVALID);
}

int main(void) {
    test_every_relation();
    test_equal_sketches_are_exact();
    test_dimensions();
    test_rhythm_sketch_takes_time_relations();
    test_without_a_scale_pitches_move_by_semitones();
    test_bad_input();
    printf("motive_variant_recognition: ok\n");
    return 0;
}
