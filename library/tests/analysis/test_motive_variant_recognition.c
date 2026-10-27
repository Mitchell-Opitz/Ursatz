#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_variant_recognition.h"
#include "semantics/non_chord_tone.h"

/*
 * Analysis-regression tests for the variant recognizer. Each relation is applied to a fixture, the
 * result is recognized, and the recovered chain is replayed: replayed through motive_relation_apply
 * it must give the target (the op names may differ from the chain that made it).
 */

static ExactRational q(unsigned long long n, unsigned long long d) { return er_make(n, d); }

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

static MotiveRelationStep ratio_step(MotiveRelationKind kind, long a, long b, long c, size_t which,
                                     long num, long den) {
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
static void assert_replays(const MotiveSketch *source, const MotiveSketch *target,
                           const MotiveRecognition *found) {
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
        fprintf(stderr, "%s refused: %s\n", motive_relation_name(relation.kind),
                motive_relation_result_name(result));
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

#define CASE_MAX 64
static MotiveRelationStep CASES[CASE_MAX];
static size_t CASE_COUNT;

static void add_case(MotiveRelationStep relation) {
    assert(CASE_COUNT < CASE_MAX);
    CASES[CASE_COUNT++] = relation;
}

/* Every relation, with parameters that apply to the base fixture. */
static void init_cases(void) {
    add_case(step(MOTIVE_RELATION_TRANSPOSE, 0, 5, 0));
    add_case(step(MOTIVE_RELATION_TRANSPOSE, 1, 2, 0));
    add_case(step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 5));
    add_case(step(MOTIVE_RELATION_INVERT, -1, 0, 0));
    add_case(step(MOTIVE_RELATION_INVERT, 62, 0, 0));
    add_case(step(MOTIVE_RELATION_INVERT, -1, 1, 0));
    add_case(step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0));
    add_case(step(MOTIVE_RELATION_INTERVAL_CHANGE, 2, 2, 0));
    add_case(step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 1, 0));
    add_case(step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0));
    add_case(step(MOTIVE_RELATION_FRAGMENT, 2, 4, 0));
    add_case(step(MOTIVE_RELATION_FRAGMENT, 3, 2, 0));
    add_case(step(MOTIVE_RELATION_LIQUIDATE, 2, 0, 0));
    add_case(step(MOTIVE_RELATION_LIQUIDATE, 1, 1, 0));
    add_case(step(MOTIVE_RELATION_AUGMENT, 0, 0, 0));
    add_case(step(MOTIVE_RELATION_DIMINISH, 0, 0, 0));
    add_case(ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0, 0, 0, 3, 2));
    add_case(step(MOTIVE_RELATION_NOTE_REPEAT, 0, 2, 0));
    add_case(step(MOTIVE_RELATION_NOTE_REPEAT, 3, 4, 0));
    add_case(step(MOTIVE_RELATION_RHYTHM_REPEAT, 1, 2, 3));
    add_case(ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, 1, 8));
    add_case(step(MOTIVE_RELATION_ADD_UPBEAT, 1, 1, 0));
    add_case(step(MOTIVE_RELATION_ADD_UPBEAT, 2, -1, 0));
    add_case(step(MOTIVE_RELATION_CHANGE_METER, 2, 4, 0));
    add_case(step(MOTIVE_RELATION_ADD_INTERVAL, 2, 1, 0));
    add_case(step(MOTIVE_RELATION_OMIT_INTERVAL, 2, 0, 0));
    add_case(step(MOTIVE_RELATION_ANCILLARY_FILL, 3, 0, 0));
    add_case(ratio_step(MOTIVE_RELATION_CONDENSE, 0, 2, 0, 2, 1, 2));
    add_case(step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 1));
    add_case(ratio_step(MOTIVE_RELATION_SHIFT_FEATURE, 1, 0, 0, 1, 1, 16));
    add_case(step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0));
    add_case(step(MOTIVE_RELATION_HARMONY_APPEND, 4, 0, 0));
    add_case(step(MOTIVE_RELATION_HARMONY_INSERT, 1, 4, 0));
    add_case(step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 1, 4, 0));
    add_case(step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0));
    add_case(step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0));
    add_case(step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0));
    add_case(step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0));
    add_case(step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 2, NON_CHORD_TONE_PASSING, 0));
    add_case(step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 1, 0, 0));
    add_case(step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 1, NON_CHORD_TONE_APPOGGIATURA, 0));
    add_case(step(MOTIVE_RELATION_REHARMONIZE_FRAME, 4, 0, 0));
}
/*
 * Each frame relation of the base fixture's cases in a chain of two with every stride-th case, in both orders. All must be
 * recovered. The full run (stride 1) is the slow test.
 */
static void test_frame_relation_chains(size_t stride, size_t required) {
    static const MotiveRelationKind frame[] = {
        MOTIVE_RELATION_REDUCE_TO_FRAME,       MOTIVE_RELATION_ADD_NON_CHORD_TONE,
        MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, MOTIVE_RELATION_CHANGE_NON_CHORD_TONE,
        MOTIVE_RELATION_REHARMONIZE_FRAME,
    };
    MotiveSketch source = base();
    size_t chains = 0;
    for (size_t i = 0; i < CASE_COUNT; i++) {
        bool is_frame = false;
        for (size_t k = 0; k < sizeof(frame) / sizeof(frame[0]); k++) {
            is_frame = is_frame || CASES[i].kind == frame[k];
        }
        if (!is_frame) {
            continue;
        }
        for (size_t j = 0; j < CASE_COUNT; j += stride) {
            for (int order = 0; order < 2; order++) {
                MotiveRelationStep first = order == 0 ? CASES[i] : CASES[j];
                MotiveRelationStep second = order == 0 ? CASES[j] : CASES[i];
                MotiveSketch middle;
                MotiveSketch target;
                if (motive_relation_apply(&source, &first, &middle) != MOTIVE_RELATION_OK ||
                    motive_relation_apply(&middle, &second, &target) != MOTIVE_RELATION_OK ||
                    motive_sketch_equal(&source, &target)) {
                    continue;
                }
                MotiveRecognition found = recognize(&source, &target);
                if (!found.found) {
                    fprintf(stderr, "chain %s then %s not recovered\n", motive_relation_name(first.kind),
                            motive_relation_name(second.kind));
                }
                assert(found.found);
                assert_replays(&source, &target, &found);
                chains++;
            }
        }
    }
    assert(chains >= required);
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
    bool seen[MOTIVE_RELATION_KIND_COUNT] = {false};
    for (size_t i = 0; i < CASE_COUNT; i++) {
        recover_one(&source, CASES[i]);
        seen[CASES[i].kind] = true;
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
    assert(found.changed[MOTIVE_DIMENSION_RHYTHM] &&
           found.similarity[MOTIVE_DIMENSION_RHYTHM] < 1.0);
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
    /* A pitch change is not a rhythm sketch's to make: against a pitched target nothing is found.
     */
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

static unsigned next_random(unsigned *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

/*
 * Seeded chains of min_length to max_length relations from the base fixture. Whatever is found
 * replays to the target. The search is a beam search guided by the nearness to the target, so a
 * chain whose first steps look no nearer than the beam's other states can be missed: chains of two
 * are all found, longer ones are found at least as often as required (measured: about 4 in 5).
 */
static void test_chains(size_t min_length, size_t max_length, size_t chains, size_t required) {
    unsigned seed = 14;
    size_t found_count = 0;
    for (size_t made = 0; made < chains;) {
        MotiveSketch source = base();
        MotiveSketch at = source;
        size_t length = min_length + next_random(&seed) % (max_length - min_length + 1);
        size_t applied = 0;
        for (size_t attempts = 0; applied < length && attempts < 40; attempts++) {
            MotiveRelationStep relation = CASES[next_random(&seed) % CASE_COUNT];
            MotiveSketch next;
            if (motive_relation_apply(&at, &relation, &next) == MOTIVE_RELATION_OK) {
                at = next;
                applied++;
            }
        }
        if (applied < length) {
            continue;
        }
        made++;
        MotiveRecognition found = recognize(&source, &at);
        if (found.found) {
            assert_replays(&source, &at, &found);
            found_count++;
        } else {
            assert(msk_eq(found.distance, q(1, 1)));
        }
    }
    assert(found_count >= required);
}

/* A pair with nothing in common is not found: distance 1, and the steps lead to the closest sketch
 * reached. */
static void test_unrelated_pair_is_not_found(void) {
    MotiveSketch source = base();
    MotiveSketch other = base();
    static const int pitches[6] = {71, 48, 77, 50, 70, 52};
    static const unsigned long long den[7] = {16, 4, 16, 2, 16, 8, 16};
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < 6; i++) {
        other.notes[i].pitch = pitches[i];
        other.notes[i].onset = at;
        other.notes[i].duration = q(1, den[i]);
        at = msk_add(at, other.notes[i].duration);
    }
    other.length = at;
    other.chord_count = 1;
    other.chords[0].degree = 6;
    MotiveRecognition found = recognize(&source, &other);
    assert(!found.found);
    assert(msk_eq(found.distance, q(1, 1)));
    assert(found.step_count <= MOTIVE_RECOGNIZE_MAX_DEPTH);
    MotiveSketch walk = source;
    for (size_t i = 0; i < found.step_count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&walk, &found.steps[i], &next) == MOTIVE_RELATION_OK);
        walk = next;
    }
    assert(!motive_sketch_equal(&walk, &other));

    /* The same search is the same answer. */
    MotiveRecognition again = recognize(&source, &other);
    assert(again.step_count == found.step_count);
    assert(memcmp(again.steps, found.steps, found.step_count * sizeof(found.steps[0])) == 0);
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
    assert(motive_variant_recognize(&source, &source, &options, &found) ==
           MOTIVE_RECOGNIZE_INVALID);
    options.max_depth = 0;
    assert(motive_variant_recognize(&source, &source, &options, &found) ==
           MOTIVE_RECOGNIZE_INVALID);
    motive_recognize_options_default(&options);
    options.beam = 0;
    assert(motive_variant_recognize(&source, &source, &options, &found) ==
           MOTIVE_RECOGNIZE_INVALID);
}

/* A replay that normalizes to the contour after every step, as contour mode does. */
static MotiveSketch replay_contour(const MotiveSketch *source, const MotiveRecognition *found) {
    MotiveSketch at = *source;
    assert(motive_sketch_contour_normalize(&at, &at));
    for (size_t i = 0; i < found->step_count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&at, &found->steps[i], &next) == MOTIVE_RELATION_OK);
        assert(motive_sketch_contour_normalize(&next, &next));
        at = next;
    }
    return at;
}

/* Contour mode finds what only the ranks show: each step is normalized, so the target is its contour. */
static void test_contour_mode(void) {
    /* The contour 0 2 1 3 2: five quarters on the ranks, in C major. */
    MotiveSketch source = base();
    static const int ranks_as_pitches[5] = {60, 64, 62, 65, 64};
    for (size_t i = 0; i < 5; i++) {
        source.notes[i].pitch = ranks_as_pitches[i];
        source.notes[i].onset = q(i, 4);
        source.notes[i].duration = q(1, 4);
    }
    source.note_count = 5;
    source.length = q(5, 4);
    source.feature_count = 0;
    source.chord_count = 0;
    source.bar = q(1, 1);
    assert(motive_sketch_contour_normalize(&source, &source));
    MotiveRelationStep relations[5] = {step(MOTIVE_RELATION_INVERT, -1, 0, 0), step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0),
                                       step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0), step(MOTIVE_RELATION_ADD_INTERVAL, 1, -3, 0),
                                       step(MOTIVE_RELATION_OMIT_INTERVAL, 2, 0, 0)};
    for (size_t i = 0; i < 5; i++) {
        MotiveSketch target;
        assert(motive_relation_apply(&source, &relations[i], &target) == MOTIVE_RELATION_OK);
        assert(motive_sketch_contour_normalize(&target, &target));
        MotiveRecognizeOptions options;
        motive_recognize_options_default(&options);
        options.contour = true;
        MotiveRecognition found;
        assert(motive_variant_recognize(&source, &target, &options, &found) == MOTIVE_RECOGNIZE_OK);
        if (!found.found) {
            fprintf(stderr, "contour mode: %s not found\n", motive_relation_name(relations[i].kind));
        }
        assert(found.found);
        MotiveSketch want = settled(&target);
        MotiveSketch got = replay_contour(&source, &found);
        assert(motive_sketch_equal(&got, &want));
        /* The same pair without contour mode is not reachable exactly. */
        MotiveRecognizeOptions one;
        motive_recognize_options_default(&one);
        one.max_depth = 1;
        MotiveRecognition single;
        assert(motive_variant_recognize(&source, &target, &one, &single) == MOTIVE_RECOGNIZE_OK);
        /* Without contour mode one step cannot reach a target the normalization moved (retrograde and fragment keep
         * the ranks dense, so one exact step does reach them). */
        assert(single.found == (i == 1 || i == 2));
    }
    /* Unknown pitches are refused. */
    MotiveSketch rhythm = base();
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.contour = true;
    MotiveRecognition found;
    assert(motive_variant_recognize(&rhythm, &rhythm, &options, &found) == MOTIVE_RECOGNIZE_INVALID);
}

/* Chains of two relations are all found at beam 1: the first level is never cut. */
static void test_two_relations_at_beam_one(void) {
    unsigned seed = 77;
    for (size_t made = 0; made < 25;) {
        MotiveSketch source = base();
        MotiveSketch at = source;
        size_t applied = 0;
        for (size_t attempts = 0; applied < 2 && attempts < 40; attempts++) {
            MotiveRelationStep relation = CASES[next_random(&seed) % CASE_COUNT];
            MotiveSketch next;
            if (motive_relation_apply(&at, &relation, &next) == MOTIVE_RELATION_OK) {
                at = next;
                applied++;
            }
        }
        if (applied < 2) {
            continue;
        }
        made++;
        MotiveRecognizeOptions options;
        motive_recognize_options_default(&options);
        options.beam = 1;
        MotiveRecognition found;
        assert(motive_variant_recognize(&source, &at, &options, &found) == MOTIVE_RECOGNIZE_OK);
        assert_replays(&source, &at, &found);
    }
}

/* A rhythm sketch of q e e q q, for the rhythm relations a chain draws. */
static MotiveSketch rhythm_five(void) {
    MotiveSketch sketch = rhythm_only();
    return sketch;
}

/* Each parameter the registry draws is proposed: the relation, then a second one, is recovered. */
static void test_parameters_the_registry_draws(void) {
    MotiveSketch pitched = base();
    MotiveSketch rhythm = rhythm_five();
    MotiveRelationStep second = step(MOTIVE_RELATION_TRANSPOSE, 0, 2, 0);
    const MotiveRelationStep rhythm_steps[] = {
        step(MOTIVE_RELATION_NOTE_REPEAT, 0, 4, 0),
        step(MOTIVE_RELATION_NOTE_REPEAT, 3, 2, 0),
        ratio_step(MOTIVE_RELATION_CONDENSE, 0, 0, 0, 2, 1, 2),
        ratio_step(MOTIVE_RELATION_CONDENSE, 1, 2, 0, 2, 1, 2),
        ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0, 0, 0, 2, 1),
        ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0, 0, 0, 3, 2),
        ratio_step(MOTIVE_RELATION_DIMINISH, 0, 0, 0, 0, 1, 2),
    };
    for (size_t i = 0; i < sizeof(rhythm_steps) / sizeof(rhythm_steps[0]); i++) {
        MotiveRecognition found = recover_one(&rhythm, rhythm_steps[i]);
        (void)found;
        /* Then a second relation after it. */
        MotiveRelationStep upbeat = step(MOTIVE_RELATION_ADD_UPBEAT, 2, -1, 0);
        MotiveSketch middle;
        MotiveSketch target;
        assert(motive_relation_apply(&rhythm, &rhythm_steps[i], &middle) == MOTIVE_RELATION_OK);
        if (motive_relation_apply(&middle, &upbeat, &target) == MOTIVE_RELATION_OK) {
            MotiveRecognition two = recognize(&rhythm, &target);
            assert_replays(&rhythm, &target, &two);
        }
    }
    const MotiveRelationStep pitched_steps[] = {
        step(MOTIVE_RELATION_HARMONY_INSERT, 0, 4, 0),
        step(MOTIVE_RELATION_HARMONY_APPEND, 6, 1, 0),
        step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 3),
        step(MOTIVE_RELATION_REPEAT_FEATURE, 1, 1, -2),
        step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 2, 5),
    };
    for (size_t i = 0; i < sizeof(pitched_steps) / sizeof(pitched_steps[0]); i++) {
        recover_one(&pitched, pitched_steps[i]);
        MotiveSketch middle;
        MotiveSketch target;
        assert(motive_relation_apply(&pitched, &pitched_steps[i], &middle) == MOTIVE_RELATION_OK);
        if (motive_relation_apply(&middle, &second, &target) == MOTIVE_RELATION_OK) {
            MotiveRecognition two = recognize(&pitched, &target);
            assert_replays(&pitched, &target, &two);
        }
    }
}

/* The steps a registry replays (R-152): a transpose by at most 12, and no liquidate or interval-change. */
static bool admit_registry(const MotiveRelationStep *relation, const void *context) {
    const bool *contour = context;
    if (relation->kind == MOTIVE_RELATION_TRANSPOSE && (relation->value[1] > 12 || relation->value[1] < -12)) {
        return false;
    }
    if (*contour && (relation->kind == MOTIVE_RELATION_LIQUIDATE || relation->kind == MOTIVE_RELATION_INTERVAL_CHANGE)) {
        return false;
    }
    return true;
}

/* D4 F4 A4 D5 C5 in D minor, q e e q q, with a quarter pickup. */
static MotiveSketch d_minor_with_pickup(void) {
    static const int pitches[5] = {62, 65, 69, 74, 72};
    static const unsigned long long den[5] = {4, 8, 8, 4, 4};
    MotiveSketch sketch = base();
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < 5; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = at;
        sketch.notes[i].duration = q(1, den[i]);
        at = msk_add(at, sketch.notes[i].duration);
    }
    sketch.note_count = 5;
    sketch.tonic_pc = 2;
    sketch.minor = true;
    sketch.pickup = q(1, 4);
    sketch.length = at;
    sketch.feature_count = 0;
    sketch.chord_count = 0;
    return sketch;
}

/* Admitted steps only: the chromatic -19 the search finds first is refused, so the tonal -11 comes back (R-152). */
static void test_admit_refuses_steps_the_registry_cannot_replay(void) {
    MotiveSketch source = d_minor_with_pickup();
    MotiveRelationStep relation = step(MOTIVE_RELATION_TRANSPOSE, 1, -11, 0);
    MotiveSketch target;
    assert(motive_relation_apply(&source, &relation, &target) == MOTIVE_RELATION_OK);
    MotiveRecognition plain = recognize(&source, &target);
    assert(plain.found);
    assert(plain.step_count != 1 || plain.steps[0].value[1] > 12 || plain.steps[0].value[1] < -12);

    bool contour = false;
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.admit = admit_registry;
    options.admit_context = &contour;
    MotiveRecognition found;
    assert(motive_variant_recognize(&source, &target, &options, &found) == MOTIVE_RECOGNIZE_OK);
    assert(found.found);
    assert(found.step_count == 1);
    assert(found.steps[0].kind == MOTIVE_RELATION_TRANSPOSE);
    assert(found.steps[0].value[0] == 1 && found.steps[0].value[1] == -11);
    assert_replays(&source, &target, &found);

    /* A contour pair: omit-interval 3, where the library would give liquidate. */
    MotiveSketch contour_source = base();
    static const int ranks_as_pitches[5] = {60, 64, 62, 65, 64};
    for (size_t i = 0; i < 5; i++) {
        contour_source.notes[i].pitch = ranks_as_pitches[i];
        contour_source.notes[i].onset = q(i, 4);
        contour_source.notes[i].duration = q(1, 4);
    }
    contour_source.note_count = 5;
    contour_source.length = q(5, 4);
    contour_source.feature_count = 0;
    contour_source.chord_count = 0;
    assert(motive_sketch_contour_normalize(&contour_source, &contour_source));
    MotiveRelationStep omit = step(MOTIVE_RELATION_OMIT_INTERVAL, 3, 0, 0);
    MotiveSketch contour_target;
    assert(motive_relation_apply(&contour_source, &omit, &contour_target) == MOTIVE_RELATION_OK);
    assert(motive_sketch_contour_normalize(&contour_target, &contour_target));
    contour = true;
    motive_recognize_options_default(&options);
    options.contour = true;
    options.admit = admit_registry;
    options.admit_context = &contour;
    assert(motive_variant_recognize(&contour_source, &contour_target, &options, &found) == MOTIVE_RECOGNIZE_OK);
    assert(found.found);
    assert(found.step_count == 1);
    assert(found.steps[0].kind == MOTIVE_RELATION_OMIT_INTERVAL && found.steps[0].value[0] == 3);

    /* Contour mode, an added note that becomes the lowest: add-interval 3, steps -7, found in one step. */
    MotiveRelationStep add = step(MOTIVE_RELATION_ADD_INTERVAL, 3, -7, 0);
    assert(motive_relation_apply(&contour_source, &add, &contour_target) == MOTIVE_RELATION_OK);
    assert(motive_sketch_contour_normalize(&contour_target, &contour_target));
    motive_recognize_options_default(&options);
    options.contour = true;
    options.max_depth = 1;
    assert(motive_variant_recognize(&contour_source, &contour_target, &options, &found) == MOTIVE_RECOGNIZE_OK);
    assert(found.found);
    assert(found.step_count == 1);
    MotiveSketch want = settled(&contour_target);
    MotiveSketch got = replay_contour(&contour_source, &found);
    assert(motive_sketch_equal(&got, &want));
}

/* A repeated feature whose first step the target cannot show (R-153): repeat-feature, then add-interval, at beam 1. */
static void test_repeat_feature_under_a_second_step(void) {
    MotiveSketch source = base();
    static const int pitches[5] = {60, 64, 67, 65, 64};
    static const unsigned long long den[5] = {4, 8, 8, 4, 4};
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < 5; i++) {
        source.notes[i].pitch = pitches[i];
        source.notes[i].onset = at;
        source.notes[i].duration = q(1, den[i]);
        at = msk_add(at, source.notes[i].duration);
    }
    source.note_count = 5;
    source.length = at;
    source.chord_count = 0;
    source.feature_count = 2;
    source.features[0].first = 0;
    source.features[0].last = 1;
    source.features[1].first = 3;
    source.features[1].last = 4;
    MotiveRelationStep repeat = step(MOTIVE_RELATION_REPEAT_FEATURE, 1, 1, 5);
    MotiveRelationStep add = step(MOTIVE_RELATION_ADD_INTERVAL, 6, 2, 0);
    MotiveSketch middle;
    MotiveSketch target;
    assert(motive_relation_apply(&source, &repeat, &middle) == MOTIVE_RELATION_OK);
    assert(motive_relation_apply(&middle, &add, &target) == MOTIVE_RELATION_OK);
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.beam = 1;
    MotiveRecognition found;
    assert(motive_variant_recognize(&source, &target, &options, &found) == MOTIVE_RECOGNIZE_OK);
    assert(found.found);
    assert(found.step_count == 2);
    assert_replays(&source, &target, &found);
}

/* A filter that admits everything gives the same chains as none (R-152). */
static void test_admit_null_and_all_agree(void) {
    MotiveSketch source = base();
    for (size_t i = 0; i < CASE_COUNT; i++) {
        MotiveSketch target;
        if (motive_relation_apply(&source, &CASES[i], &target) != MOTIVE_RELATION_OK) {
            continue;
        }
        MotiveRecognition plain = recognize(&source, &target);
        MotiveRecognizeOptions options;
        motive_recognize_options_default(&options);
        assert(options.admit == NULL && options.admit_context == NULL);
        options.admit = admit_registry;
        bool contour = false;
        options.admit_context = &contour;
        MotiveRecognition admitted;
        assert(motive_variant_recognize(&source, &target, &options, &admitted) == MOTIVE_RECOGNIZE_OK);
        /* The case table stays inside the registry's ranges, so the filter changes nothing. */
        assert(admitted.found == plain.found);
        assert(admitted.step_count == plain.step_count);
        assert(memcmp(admitted.steps, plain.steps, plain.step_count * sizeof(plain.steps[0])) == 0);
    }
}

/* Hollow Knight 1 (A A B C B) in quarter notes over the given chords. */
static MotiveSketch hollow_knight(const int *degrees, const ExactRational *onsets, size_t count) {
    static const int pitches[5] = {81, 81, 83, 84, 83};
    MotiveSketch sketch = base();
    sketch.note_count = 5;
    for (size_t i = 0; i < 5; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = q(i, 4);
        sketch.notes[i].duration = q(1, 4);
    }
    sketch.length = q(5, 4);
    sketch.feature_count = 0;
    sketch.chord_count = count;
    for (size_t c = 0; c < count; c++) {
        memset(&sketch.chords[c], 0, sizeof(sketch.chords[c]));
        sketch.chords[c].onset = onsets[c];
        sketch.chords[c].degree = degrees[c];
    }
    return sketch;
}

/* Every harmony row on Hollow Knight 1, with chords that change the notes and with chords that do not. */
static void test_harmony_rows_on_tone_roles(void) {
    int vi[1] = {6};
    ExactRational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0));
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 5, 0, 0));
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 0, 5, 0));
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 0, 1, 0));
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 5, 0));
    recover_one(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 2, 4));

    int tonic_subdominant[2] = {1, 4};
    ExactRational onsets[2] = {q(0, 1), q(1, 2)};
    MotiveSketch two = hollow_knight(tonic_subdominant, onsets, 2);
    recover_one(&two, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0));
    recover_one(&two, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0));
}

int main(int argc, char **argv) {
    init_cases();
    if (argc > 1 && strcmp(argv[1], "frame-chains") == 0) {
        test_frame_relation_chains(1, 100);
        printf("motive_variant_recognition frame chains: ok\n");
        return 0;
    }
    test_every_relation();
    test_harmony_rows_on_tone_roles();
    test_equal_sketches_are_exact();
    test_dimensions();
    test_rhythm_sketch_takes_time_relations();
    test_without_a_scale_pitches_move_by_semitones();
    test_frame_relation_chains(11, 10);
    test_chains(2, 2, 40, 40);
    test_chains(3, 4, 40, 30);
    test_unrelated_pair_is_not_found();
    test_bad_input();
    test_contour_mode();
    test_two_relations_at_beam_one();
    test_parameters_the_registry_draws();
    test_admit_refuses_steps_the_registry_cannot_replay();
    test_repeat_feature_under_a_second_step();
    test_admit_null_and_all_agree();
    printf("motive_variant_recognition: ok\n");
    return 0;
}
