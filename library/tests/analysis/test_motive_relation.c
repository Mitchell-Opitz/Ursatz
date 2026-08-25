#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_relation.h"

/*
 * Analysis-regression tests for the motive relations. Each relation has a case that checks its result, the
 * refusals it makes, and that the input is never changed.
 */

static ExactRational q(unsigned long long n, unsigned long long d) {
    return er_make(n, d);
}

/* Notes at consecutive onsets of one duration, in 4/4 in C major from the given pitches. */
static MotiveSketch line(const int *pitches, size_t count, ExactRational each) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < count; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = at;
        sketch.notes[i].duration = each;
        at = msk_add(at, each);
    }
    sketch.note_count = count;
    sketch.has_scale = true;
    sketch.tonic_pc = 0;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = at;
    return sketch;
}

/* C D E G in quarter notes. */
static MotiveSketch cdeg(void) {
    static const int pitches[4] = {60, 62, 64, 67};
    return line(pitches, 4, q(1, 4));
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

static MotiveRelationStep ratio_step(MotiveRelationKind kind, long num, long den) {
    MotiveRelationStep result = step(kind, num, 0, 0);
    result.den[0] = den;
    return result;
}

/* A step whose ratio is the third parameter (condense). */
static MotiveRelationStep ratio_step3(MotiveRelationKind kind, long first, long last, long num, long den) {
    MotiveRelationStep result = step(kind, first, last, num);
    result.den[2] = den;
    return result;
}

/* A step whose ratio is the second parameter (shift-feature). */
static MotiveRelationStep ratio_step_at(MotiveRelationKind kind, long which, long num, long den) {
    MotiveRelationStep result = step(kind, which, num, 0);
    result.den[1] = den;
    return result;
}

/* Applies a step and checks the input did not change. */
static MotiveRelationResult apply(const MotiveSketch *in, MotiveRelationStep relation, MotiveSketch *out) {
    MotiveSketch before = *in;
    MotiveRelationResult result = motive_relation_apply(in, &relation, out);
    assert(memcmp(&before, in, sizeof(before)) == 0);
    return result;
}

static void assert_pitches(const MotiveSketch *sketch, const int *pitches, size_t count) {
    assert(sketch->note_count == count);
    for (size_t i = 0; i < count; i++) {
        assert(sketch->notes[i].pitch == pitches[i]);
    }
}

static void assert_note(const MotiveSketch *sketch, size_t index, int pitch, ExactRational onset,
                        ExactRational duration) {
    assert(sketch->notes[index].pitch == pitch);
    assert(msk_eq(sketch->notes[index].onset, onset));
    assert(msk_eq(sketch->notes[index].duration, duration));
}

/* --- the catalog ------------------------------------------------------------------------------ */

static void test_catalog(void) {
    assert(motive_relation_count() == MOTIVE_RELATION_KIND_COUNT);
    for (size_t i = 0; i < motive_relation_count(); i++) {
        const char *name = motive_relation_name((MotiveRelationKind)i);
        assert(name != NULL);
        MotiveRelationKind found;
        assert(motive_relation_find(name, &found));
        assert(found == (MotiveRelationKind)i);
        assert(motive_relation_param_count((MotiveRelationKind)i) <= MOTIVE_RELATION_MAX_PARAMS);
    }
    assert(motive_relation_name(MOTIVE_RELATION_KIND_COUNT) == NULL);
    MotiveRelationKind kind;
    assert(!motive_relation_find("no-such-relation", &kind));

    assert(motive_relation_cost(MOTIVE_RELATION_EXACT) == 0);
    assert(motive_relation_cost(MOTIVE_RELATION_TRANSPOSE) == 1);
    assert(motive_relation_cost(MOTIVE_RELATION_AUGMENT) == 1);
    assert(motive_relation_cost(MOTIVE_RELATION_DIMINISH) == 1);
    assert(motive_relation_cost(MOTIVE_RELATION_INVERT) == 2);
    assert(motive_relation_cost(MOTIVE_RELATION_RETROGRADE) == 2);
    assert(motive_relation_cost(MOTIVE_RELATION_RHYTHM_SHIFT) == 2);
    assert(motive_relation_cost(MOTIVE_RELATION_FRAGMENT) == 2);
    assert(motive_relation_cost(MOTIVE_RELATION_CHANGE_METER) == 3);

    assert(strcmp(motive_relation_name(MOTIVE_RELATION_INTERVAL_CHANGE), "interval-change") == 0);
    assert(motive_relation_param_count(MOTIVE_RELATION_TRANSPOSE) == 3);
    assert(motive_relation_param_count(MOTIVE_RELATION_RETROGRADE) == 0);
    assert(strcmp(motive_relation_result_name(MOTIVE_RELATION_NO_SCALE), "no scale") == 0);
}

static void test_invalid_input(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;
    MotiveRelationStep relation = step(MOTIVE_RELATION_EXACT, 0, 0, 0);
    assert(motive_relation_apply(NULL, &relation, &out) == MOTIVE_RELATION_INVALID);
    assert(motive_relation_apply(&sketch, NULL, &out) == MOTIVE_RELATION_INVALID);
    assert(motive_relation_apply(&sketch, &relation, NULL) == MOTIVE_RELATION_INVALID);
    relation.kind = MOTIVE_RELATION_KIND_COUNT;
    assert(motive_relation_apply(&sketch, &relation, &out) == MOTIVE_RELATION_INVALID);
    relation.kind = MOTIVE_RELATION_EXACT;
    sketch.note_count = 0;
    assert(motive_relation_apply(&sketch, &relation, &out) == MOTIVE_RELATION_INVALID);
}

/* --- the development relations ---------------------------------------------------------------------- */

static void test_exact(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;
    assert(apply(&sketch, step(MOTIVE_RELATION_EXACT, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(motive_sketch_equal(&sketch, &out));
}

static void test_transpose(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;

    /* Real: by semitones. */
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 3, 0), &out) == MOTIVE_RELATION_OK);
    int real[4] = {63, 65, 67, 70};
    assert_pitches(&out, real, 4);

    /* Tonal: by steps of the scale, so the quality of the intervals is kept to the scale. */
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 1, 2, 0), &out) == MOTIVE_RELATION_OK);
    int tonal[4] = {64, 65, 67, 71};
    assert_pitches(&out, tonal, 4);

    /* To a degree: the first note moves to the scale degree by the nearest way. */
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 5), &out) == MOTIVE_RELATION_OK);
    int to_five[4] = {55, 57, 59, 62}; /* down a fourth: 4 steps up is farther than 3 down */
    assert_pitches(&out, to_five, 4);
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 3), &out) == MOTIVE_RELATION_OK);
    int to_three[4] = {64, 65, 67, 71};
    assert_pitches(&out, to_three, 4);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 80, 0), &out) == MOTIVE_RELATION_RANGE);
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, -61, 0), &out) == MOTIVE_RELATION_RANGE);
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 9), &out) == MOTIVE_RELATION_INVALID);
    MotiveSketch no_scale = sketch;
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_TRANSPOSE, 1, 2, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    assert(apply(&no_scale, step(MOTIVE_RELATION_TRANSPOSE, 0, 0, 5), &out) == MOTIVE_RELATION_NO_SCALE);
    assert(apply(&no_scale, step(MOTIVE_RELATION_TRANSPOSE, 0, 2, 0), &out) == MOTIVE_RELATION_OK);
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, step(MOTIVE_RELATION_TRANSPOSE, 0, 2, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_invert(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;

    /* About the first note, in semitones. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, -1, 0, 0), &out) == MOTIVE_RELATION_OK);
    int real[4] = {60, 58, 56, 53};
    assert_pitches(&out, real, 4);

    /* About the first note, in steps of the scale: C D E G go to C B(flat not used) A F. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, -1, 1, 0), &out) == MOTIVE_RELATION_OK);
    int tonal[4] = {60, 59, 57, 53};
    assert_pitches(&out, tonal, 4);

    /* About another pitch. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, 64, 0, 0), &out) == MOTIVE_RELATION_OK);
    int about_e[4] = {68, 66, 64, 61};
    assert_pitches(&out, about_e, 4);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, 20, 0, 0), &out) == MOTIVE_RELATION_RANGE);
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, 200, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    MotiveSketch level = sketch;
    for (size_t i = 0; i < level.note_count; i++) {
        level.notes[i].pitch = 60;
    }
    assert(apply(&level, step(MOTIVE_RELATION_INVERT, -1, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    MotiveSketch no_scale = sketch;
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_INVERT, -1, 1, 0), &out) == MOTIVE_RELATION_NO_SCALE);
}

static void test_retrograde(void) {
    int pitches[4] = {60, 62, 64, 67};
    MotiveSketch sketch = line(pitches, 4, q(1, 4));
    sketch.notes[1].duration = q(1, 8); /* D is short, then a gap */
    sketch.notes[2].onset = q(3, 8);
    sketch.notes[2].duration = q(3, 8);
    sketch.notes[3].onset = q(3, 4);
    sketch.feature_count = 1;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    MotiveSketch out;
    assert(apply(&sketch, step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    /* The span is 0 to 1; a note at [a, b) goes to [1 - b, 1 - a). */
    assert_note(&out, 0, 67, q(0, 1), q(1, 4));
    assert_note(&out, 1, 64, q(1, 4), q(3, 8));
    assert_note(&out, 2, 62, q(5, 8), q(1, 8));
    assert_note(&out, 3, 60, q(3, 4), q(1, 4));
    assert(out.features[0].first == 2 && out.features[0].last == 3);

    MotiveSketch palindrome = cdeg();
    palindrome.notes[0].pitch = 60;
    palindrome.notes[1].pitch = 62;
    palindrome.notes[2].pitch = 62;
    palindrome.notes[3].pitch = 60;
    assert(apply(&palindrome, step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);

    /* A rhythm sketch has a retrograde too. */
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, step(MOTIVE_RELATION_RETROGRADE, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
}

static void test_interval_change(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;

    /* Widen the third interval (E up to G) by 2: G becomes A. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 3, 2, 0), &out) == MOTIVE_RELATION_OK);
    int third[4] = {60, 62, 64, 69};
    assert_pitches(&out, third, 4);

    /* Every interval: the changes add up along the line. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 1, 0), &out) == MOTIVE_RELATION_OK);
    int every[4] = {60, 63, 66, 70};
    assert_pitches(&out, every, 4);

    /* Narrowing the first interval pulls the rest along. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 1, -1, 0), &out) == MOTIVE_RELATION_OK);
    int narrow[4] = {60, 61, 63, 66};
    assert_pitches(&out, narrow, 4);

    /* A repeated note has no direction to widen in. */
    int repeated[3] = {60, 60, 64};
    MotiveSketch with_repeat = line(repeated, 3, q(1, 4));
    assert(apply(&with_repeat, step(MOTIVE_RELATION_INTERVAL_CHANGE, 1, 3, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&with_repeat, step(MOTIVE_RELATION_INTERVAL_CHANGE, 2, 3, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[2].pitch == 67);

    /* keep-contour refuses a result that changes a direction. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 1, -3, 1), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 1, -1, 1), &out) == MOTIVE_RELATION_OK);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 4, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 100, 0), &out) == MOTIVE_RELATION_RANGE);
    MotiveSketch single = line(repeated, 1, q(1, 4));
    assert(apply(&single, step(MOTIVE_RELATION_INTERVAL_CHANGE, 0, 1, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_fragment(void) {
    int pitches[6] = {60, 62, 64, 65, 67, 69};
    MotiveSketch sketch = line(pitches, 6, q(1, 8));
    sketch.feature_count = 2;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    strcpy(sketch.features[1].label, "b");
    sketch.features[1].first = 3;
    sketch.features[1].last = 4;
    sketch.chord_count = 2;
    sketch.chords[0] = (MotiveSketchChord){q(0, 1), 1, 0, 0, false};
    sketch.chords[1] = (MotiveSketchChord){q(3, 8), 4, 0, 0, false};
    MotiveSketch out;

    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 1, 3, 0), &out) == MOTIVE_RELATION_OK);
    int leading[3] = {60, 62, 64};
    assert_pitches(&out, leading, 3);
    assert(out.feature_count == 1 && out.features[0].first == 0 && out.features[0].last == 1);
    assert(out.chord_count == 1 && out.chords[0].degree == 1);

    /* A trailing fragment starts at 0 again; the chord in force at its start is its first. */
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 2, 3, 0), &out) == MOTIVE_RELATION_OK);
    int trailing[3] = {65, 67, 69};
    assert_pitches(&out, trailing, 3);
    assert_note(&out, 0, 65, q(0, 1), q(1, 8));
    assert_note(&out, 2, 69, q(1, 4), q(1, 8));
    assert(out.feature_count == 1 && out.features[0].first == 0 && out.features[0].last == 1);
    assert(out.chord_count == 1 && out.chords[0].degree == 4 && msk_is_zero(out.chords[0].onset));
    assert(msk_eq(out.length, q(3, 8)));

    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 3, 2, 0), &out) == MOTIVE_RELATION_OK);
    int middle[2] = {64, 65};
    assert_pitches(&out, middle, 2);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 1, 1, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 1, 1, 1), &out) == MOTIVE_RELATION_OK);
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 1, 6, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 0, 3, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_FRAGMENT, 4, 3, 0), &out) == MOTIVE_RELATION_INVALID);
}

static void test_liquidate(void) {
    int pitches[4] = {60, 62, 64, 67};
    MotiveSketch sketch = line(pitches, 4, q(1, 8)); /* C D E G in eighths: the 1st and 3rd on beats */
    MotiveSketch out;

    /* The weakest go first: off the beat, the later of equals. D and G are off the beat; G goes. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 3);
    assert_note(&out, 0, 60, q(0, 1), q(1, 8));
    assert_note(&out, 1, 62, q(1, 8), q(1, 8));
    assert_note(&out, 2, 64, q(1, 4), q(1, 8));
    assert(msk_eq(out.length, q(3, 8)));

    /* Two dropped, keeping the rhythm: the rests stay. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 2, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 2);
    assert_note(&out, 0, 60, q(0, 1), q(1, 8));
    assert_note(&out, 1, 64, q(1, 4), q(1, 8));

    /* Without keep-rhythm the notes left follow one another from 0. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 2, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(0, 1), q(1, 8));
    assert_note(&out, 1, 64, q(1, 8), q(1, 8));

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 3, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 3, 0, 1), &out) == MOTIVE_RELATION_OK);
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 4, 0, 1), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_augment_and_diminish(void) {
    MotiveSketch sketch = cdeg();
    MotiveSketch out;

    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 1, 62, q(1, 2), q(1, 2));
    assert(msk_eq(out.length, q(2, 1)));

    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_DIMINISH, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 1, 62, q(1, 8), q(1, 8));
    assert(msk_eq(out.length, q(1, 2)));

    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 3, 2), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 1, 62, q(3, 8), q(3, 8));
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_DIMINISH, 2, 3), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);

    /* The wrong side of 1, and a ratio that is not one. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 1, 2), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_DIMINISH, 2, 1), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 1, 1), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 2, 0), &out) == MOTIVE_RELATION_INVALID);

    /* A duration that cannot be written: 1/4 times 5/4 is 5/16. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 5, 4), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);

    /* A source of whole beats stays whole beats: one half-note beat, 3:2 would end on a beat and a half. */
    int pitches[2] = {60, 62};
    MotiveSketch one_beat = line(pitches, 2, q(1, 4));
    one_beat.beat = q(1, 2);
    assert(apply(&one_beat, ratio_step(MOTIVE_RELATION_AUGMENT, 3, 2), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(apply(&one_beat, ratio_step(MOTIVE_RELATION_AUGMENT, 2, 1), &out) == MOTIVE_RELATION_OK);

    /* Pitches do not matter to a rhythm. */
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, ratio_step(MOTIVE_RELATION_AUGMENT, 0, 0), &out) == MOTIVE_RELATION_OK);
}

/* --- rhythm ------------------------------------------------------------------------------------------ */

/* C G E D in quarter notes, with one feature "a" on G E. */
static MotiveSketch cged(void) {
    static const int pitches[4] = {60, 67, 64, 62};
    MotiveSketch sketch = line(pitches, 4, q(1, 4));
    sketch.feature_count = 1;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 1;
    sketch.features[0].last = 2;
    return sketch;
}

static void test_note_repeat(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 1, 2, 0), &out) == MOTIVE_RELATION_OK);
    int twice[5] = {60, 67, 67, 64, 62};
    assert_pitches(&out, twice, 5);
    assert_note(&out, 1, 67, q(1, 4), q(1, 8));
    assert_note(&out, 2, 67, q(3, 8), q(1, 8));
    assert_note(&out, 3, 64, q(1, 2), q(1, 4));
    assert(out.features[0].first == 1 && out.features[0].last == 3);

    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 0, 4, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 7);
    assert_note(&out, 3, 60, q(3, 16), q(1, 16));
    assert(out.features[0].first == 4 && out.features[0].last == 5);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 1, 3, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 1, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 1, 5, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 4, 2, 0), &out) == MOTIVE_RELATION_INVALID);
}

static void test_rhythm_repeat(void) {
    /* Quarter, two eighths, quarter, eighth, eighth... */
    MotiveSketch sketch = cged();
    sketch.notes[1].duration = q(1, 8);
    sketch.notes[2].onset = q(3, 8);
    sketch.notes[2].duration = q(1, 8);
    sketch.notes[3].onset = q(1, 2);
    MotiveSketch out;

    /* The rhythm of notes 1 and 2 (two eighths) on notes 2 and 3: the last note gets an eighth. */
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 1, 2, 2), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 2, 64, q(3, 8), q(1, 8));
    assert_note(&out, 3, 62, q(1, 2), q(1, 8));
    assert(msk_eq(out.length, q(5, 8)));

    /* The first note's quarter onto the second: the notes after it move later. */
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 0, 1, 1), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 1, 67, q(1, 4), q(1, 4));
    assert_note(&out, 2, 64, q(1, 2), q(1, 8));
    assert_note(&out, 3, 62, q(5, 8), q(1, 4));

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 1, 2, 1), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 2, 3, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 0, 2, 3), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_RHYTHM_REPEAT, 0, 0, 1), &out) == MOTIVE_RELATION_INVALID);
}

static void test_rhythm_shift(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* The notes stay; the motive starts a beat later in the bar, so its first downbeat is 3/4 in. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 1, 4), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(3, 4)));
    assert(motive_sketch_equal(&sketch, &out) == false);
    assert_note(&out, 1, 67, q(1, 4), q(1, 4));
    /* The metric strength of the first note changes with it: it is now a beat, not the downbeat. */
    assert(motive_sketch_strength(&out, out.notes[0].onset) == 1);

    /* Shifts add up inside the bar. */
    MotiveSketch shifted = out;
    assert(apply(&shifted, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 1, 2), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(1, 4)));

    /* A bar or more is the same place in the bar, and so is refused. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 1, 1), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 2, 1), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 5, 4), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(3, 4)));
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 1, 64), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_RHYTHM_SHIFT, 0, 4), &out) == MOTIVE_RELATION_INVALID);
}

static void test_add_upbeat(void) {
    MotiveSketch sketch = cged();
    sketch.chord_count = 2;
    sketch.chords[0] = (MotiveSketchChord){q(0, 1), 1, 0, 0, false};
    sketch.chords[1] = (MotiveSketchChord){q(1, 2), 5, 0, 0, false};
    MotiveSketch out;

    /* Two eighths stepping up into the C: A and B. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 2, 1, 0), &out) == MOTIVE_RELATION_OK);
    int below[6] = {57, 59, 60, 67, 64, 62};
    assert_pitches(&out, below, 6);
    assert_note(&out, 0, 57, q(0, 1), q(1, 8));
    assert_note(&out, 2, 60, q(1, 4), q(1, 4));
    assert(msk_eq(out.pickup, q(1, 4)));
    assert(out.features[0].first == 3 && out.features[0].last == 4);
    assert(msk_is_zero(out.chords[0].onset) && msk_eq(out.chords[1].onset, q(3, 4)));

    /* From above, stepping down. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 1, -1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[0].pitch == 62);
    assert(msk_eq(out.pickup, q(1, 8)));

    /* A rhythm sketch gets rhythm upbeats. */
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, step(MOTIVE_RELATION_ADD_UPBEAT, 3, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[0].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN);
    assert(msk_eq(out.pickup, q(3, 8)));

    /* Refusals: a pickup of a bar or more is no pickup. */
    sketch.pickup = q(3, 4);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 3, 1, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(7, 8)));
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 0, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 4, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_UPBEAT, 1, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    MotiveSketch low = cged();
    low.notes[0].pitch = 1;
    assert(apply(&low, step(MOTIVE_RELATION_ADD_UPBEAT, 3, 1, 0), &out) == MOTIVE_RELATION_RANGE);
}

static void test_change_meter(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* Each old bar onto one new bar: 4/4 to 3/4 scales the times by 3/4. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 3, 4, 0), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.bar, q(3, 4)) && msk_eq(out.beat, q(1, 4)));
    assert_note(&out, 1, 67, q(3, 16), q(3, 16));
    assert(msk_eq(out.length, q(3, 4)));

    /* 4/4 to 2/2 keeps the times and changes the beat. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 2, 2, 0), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.beat, q(1, 2)));
    assert_note(&out, 1, 67, q(1, 4), q(1, 4));

    /* 6/8 is 3/4 of a whole note. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 6, 8, 0), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.bar, q(3, 4)) && msk_eq(out.beat, q(1, 8)));

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 4, 4, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 5, 8, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 3, 3, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 0, 4, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_METER, 65, 4, 0), &out) == MOTIVE_RELATION_INVALID);
}

/* --- intervals ----------------------------------------------------------------------------------------- */

static void test_add_interval(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* Two steps above the C, in the second half of the C. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 1, 2, 0), &out) == MOTIVE_RELATION_OK);
    int added[5] = {60, 64, 67, 64, 62};
    assert_pitches(&out, added, 5);
    assert_note(&out, 0, 60, q(0, 1), q(1, 8));
    assert_note(&out, 1, 64, q(1, 8), q(1, 8));
    assert_note(&out, 2, 67, q(1, 4), q(1, 4));
    assert(out.features[0].first == 2 && out.features[0].last == 3);

    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 2, -1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[2].pitch == 65);
    assert(out.features[0].first == 1 && out.features[0].last == 3);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 1, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 0, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 4, 1, 0), &out) == MOTIVE_RELATION_INVALID);
    sketch.notes[0].duration = q(1, 32);
    sketch.notes[1].onset = q(1, 32);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 1, 1, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    sketch = cged();
    sketch.notes[0].pitch = 126;
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_INTERVAL, 1, 5, 0), &out) == MOTIVE_RELATION_RANGE);
}

static void test_omit_interval(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* The G goes; the C is lengthened to hold its time. */
    assert(apply(&sketch, step(MOTIVE_RELATION_OMIT_INTERVAL, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    int omitted[3] = {60, 64, 62};
    assert_pitches(&out, omitted, 3);
    assert_note(&out, 0, 60, q(0, 1), q(1, 2));
    assert_note(&out, 1, 64, q(1, 2), q(1, 4));
    assert(out.feature_count == 1 && out.features[0].first == 1 && out.features[0].last == 1);

    /* A feature of the note that went goes with it. */
    sketch.features[0].last = 1;
    assert(apply(&sketch, step(MOTIVE_RELATION_OMIT_INTERVAL, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.feature_count == 0);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_OMIT_INTERVAL, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_OMIT_INTERVAL, 4, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    int two[2] = {60, 62};
    MotiveSketch short_line = line(two, 2, q(1, 4));
    assert(apply(&short_line, step(MOTIVE_RELATION_OMIT_INTERVAL, 1, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    MotiveSketch tied = cged();
    tied.notes[2].duration = q(3, 8);
    tied.notes[3].onset = q(7, 8);
    tied.notes[3].duration = q(1, 8);
    /* Lengthening the G over the E would make it 5/8 long: not a written value. */
    assert(apply(&tied, step(MOTIVE_RELATION_OMIT_INTERVAL, 2, 0, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
}

static void test_ancillary_fill(void) {
    int leap[3] = {60, 64, 67};
    MotiveSketch sketch = line(leap, 3, q(1, 4));
    MotiveSketch out;

    /* The leap C to E is filled by the passing D, in the second half of the C. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    int passing[4] = {60, 62, 64, 67};
    assert_pitches(&out, passing, 4);
    assert_note(&out, 0, 60, q(0, 1), q(1, 8));
    assert_note(&out, 1, 62, q(1, 8), q(1, 8));

    /* A neighbor: the upper one above an ascending interval... */
    assert(apply(&sketch, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 4 && out.notes[1].pitch == 62);
    /* ...and the lower one below a descending interval. */
    int down[3] = {67, 64, 60};
    MotiveSketch descending = line(down, 3, q(1, 4));
    assert(apply(&descending, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[1].pitch == 65);

    /* A fifth needs three passing notes in an eighth: not a written value. */
    MotiveSketch fifth = cged();
    assert(apply(&fifth, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 0, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);

    /* A step has nothing to fill. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ANCILLARY_FILL, 2, 0, 0), &out) == MOTIVE_RELATION_OK);
    int stepwise[3] = {60, 62, 64};
    MotiveSketch scale = line(stepwise, 3, q(1, 4));
    assert(apply(&scale, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_ANCILLARY_FILL, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ANCILLARY_FILL, 1, 2, 0), &out) == MOTIVE_RELATION_INVALID);
}

static void test_condense(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* G and E at half length; the D moves earlier. */
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 1, 2, 1, 2), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(0, 1), q(1, 4));
    assert_note(&out, 1, 67, q(1, 4), q(1, 8));
    assert_note(&out, 2, 64, q(3, 8), q(1, 8));
    assert_note(&out, 3, 62, q(1, 2), q(1, 4));
    assert(msk_eq(out.length, q(3, 4)));

    /* The last notes condensed leave nothing after them. */
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 2, 3, 1, 2), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 2, 64, q(1, 2), q(1, 8));
    assert_note(&out, 3, 62, q(5, 8), q(1, 8));

    /* Refusals. */
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 2, 1, 1, 2), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 0, 4, 1, 2), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 1, 2, 3, 2), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step3(MOTIVE_RELATION_CONDENSE, 1, 2, 1, 3), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
}

static void test_repeat_feature(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* G E repeated once, a step lower: F D, then the D. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, -1), &out) == MOTIVE_RELATION_OK);
    int once[6] = {60, 67, 64, 65, 62, 62};
    assert_pitches(&out, once, 6);
    assert_note(&out, 3, 65, q(3, 4), q(1, 4));
    assert_note(&out, 5, 62, q(5, 4), q(1, 4));
    assert(out.features[0].first == 1 && out.features[0].last == 2);

    /* Twice: each repeat a step further. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 2, -1), &out) == MOTIVE_RELATION_OK);
    int twice[8] = {60, 67, 64, 65, 62, 64, 60, 62};
    assert_pitches(&out, twice, 8);

    /* Without a step it is an exact repeat. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.notes[3].pitch == 67 && out.notes[4].pitch == 64);

    /* A rhythm sketch repeats the rhythm. */
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 3), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 6 && out.notes[3].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 1, 1, 0), &out) == MOTIVE_RELATION_NO_FEATURE);
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 3, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 90), &out) == MOTIVE_RELATION_RANGE);
    sketch.feature_count = 0;
    assert(apply(&sketch, step(MOTIVE_RELATION_REPEAT_FEATURE, 0, 1, 0), &out) == MOTIVE_RELATION_NO_FEATURE);
}

static void test_shift_feature(void) {
    MotiveSketch sketch = cged();
    MotiveSketch out;

    /* G E move an eighth later: the C is lengthened and the E shortened. */
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 0, 1, 8), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(0, 1), q(3, 8));
    assert_note(&out, 1, 67, q(3, 8), q(1, 4));
    assert_note(&out, 2, 64, q(5, 8), q(1, 8));
    assert_note(&out, 3, 62, q(3, 4), q(1, 4));

    /* A feature on the first note starts the motive later. */
    sketch.features[0].first = 0;
    sketch.features[0].last = 0;
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 0, 1, 8), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(1, 8), q(1, 8));

    /* Refusals. */
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 0, 1, 4), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 1, 1, 8), &out) == MOTIVE_RELATION_NO_FEATURE);
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 0, 0, 8), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, ratio_step_at(MOTIVE_RELATION_SHIFT_FEATURE, 0, 1, 64), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
}

int main(void) {
    test_catalog();
    test_invalid_input();
    test_exact();
    test_transpose();
    test_invert();
    test_retrograde();
    test_interval_change();
    test_fragment();
    test_liquidate();
    test_augment_and_diminish();
    test_note_repeat();
    test_rhythm_repeat();
    test_rhythm_shift();
    test_add_upbeat();
    test_change_meter();
    test_add_interval();
    test_omit_interval();
    test_ancillary_fill();
    test_condense();
    test_repeat_feature();
    test_shift_feature();
    printf("motive_relation: ok\n");
    return 0;
}
