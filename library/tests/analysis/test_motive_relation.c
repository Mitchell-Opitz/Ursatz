#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_relation.h"
#include "analysis/motive_tone_roles.h"

/*
 * Analysis-regression tests for the motive relations. Each relation has a case that checks its result, the
 * refusals it makes, and that the input is never changed.
 */

static Rational q(unsigned long long n, unsigned long long d) {
    return rational_ratio(n, d);
}

/* Notes at consecutive onsets of one duration, in 4/4 in C major from the given pitches. */
static MotiveSketch line(const int *pitches, size_t count, Rational each) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    Rational at = q(0, 1);
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

static void assert_note(const MotiveSketch *sketch, size_t index, int pitch, Rational onset,
                        Rational duration) {
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

    assert(strcmp(motive_relation_name(MOTIVE_RELATION_REDUCE_TO_FRAME), "reduce-to-frame") == 0);
    assert(strcmp(motive_relation_name(MOTIVE_RELATION_ADD_NON_CHORD_TONE), "add-non-chord-tone") == 0);
    assert(strcmp(motive_relation_name(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE), "remove-non-chord-tone") == 0);
    assert(strcmp(motive_relation_name(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE), "change-non-chord-tone") == 0);
    assert(strcmp(motive_relation_name(MOTIVE_RELATION_REHARMONIZE_FRAME), "reharmonize-frame") == 0);
    assert(strcmp(motive_relation_param_name(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1), "type") == 0);
    assert(strcmp(motive_relation_param_name(MOTIVE_RELATION_REHARMONIZE_FRAME, 0), "degree") == 0);
    assert(motive_relation_cost(MOTIVE_RELATION_REDUCE_TO_FRAME) == 2);

    char text[80];
    MotiveRelationStep parsed;
    MotiveRelationStep add = step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 2, NON_CHORD_TONE_NEIGHBOR, 0);
    assert(motive_relation_step_text(&add, text, sizeof(text)));
    assert(strcmp(text, "add-non-chord-tone after=2 type=1") == 0);
    assert(motive_relation_step_parse(text, &parsed) && parsed.kind == add.kind && parsed.value[0] == 2 &&
           parsed.value[1] == 1);
    assert(motive_relation_step_parse("reharmonize-frame degree=5 seventh=1", &parsed) &&
           parsed.kind == MOTIVE_RELATION_REHARMONIZE_FRAME && parsed.value[0] == 5 && parsed.value[1] == 1);
    assert(motive_relation_step_parse("reduce-to-frame", &parsed) && parsed.kind == MOTIVE_RELATION_REDUCE_TO_FRAME);
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

/* E F B C and a raised F in 3/4, eighth notes, in C major. */
static MotiveSketch efbc_raised(void) {
    static const int pitches[5] = {64, 65, 71, 72, 66};
    MotiveSketch sketch = line(pitches, 5, q(1, 8));
    sketch.bar = q(3, 4);
    return sketch;
}

/* A scale degree keeps its own spelling: F is step 3 unraised, so a tonal move never sharpens it. */
static void test_tonal_relations_on_every_scale_note(void) {
    MotiveSketch sketch = efbc_raised();
    MotiveSketch out;
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    int up[5] = {65, 67, 72, 74, 68};
    assert_pitches(&out, up, 5);
    assert(apply(&sketch, step(MOTIVE_RELATION_TRANSPOSE, 1, -2, 0), &out) == MOTIVE_RELATION_OK);
    int down[5] = {60, 62, 67, 69, 63};
    assert_pitches(&out, down, 5);
    assert(apply(&sketch, step(MOTIVE_RELATION_INVERT, -1, 1, 0), &out) == MOTIVE_RELATION_OK);
    int mirrored[5] = {64, 62, 57, 55, 63};
    assert_pitches(&out, mirrored, 5);
}

/* Augment and diminish scale the pickup with the notes, and refuse a pickup that reaches the bar. */
static void test_scaled_pickup(void) {
    MotiveSketch sketch = efbc_raised();
    sketch.pickup = q(1, 8);
    MotiveSketch out;
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 2, 1), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(1, 4)));
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_DIMINISH, 1, 2), &out) == MOTIVE_RELATION_OK);
    assert(msk_eq(out.pickup, q(1, 16)));
    sketch.pickup = q(1, 2);
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 2, 1), &out) != MOTIVE_RELATION_OK);
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
    assert_note(&out, 2, 64, q(1, 4), q(1, 4));
    assert(msk_eq(out.length, q(1, 2)));

    /* Two dropped, keeping the rhythm: the time of each goes to the note before it, no rest is left. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 2, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 2);
    assert_note(&out, 0, 60, q(0, 1), q(1, 4));
    assert_note(&out, 1, 64, q(1, 4), q(1, 4));

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

static void test_liquidate_keeps_time(void) {
    static const int pitches[5] = {81, 81, 83, 84, 83};
    static const unsigned long long den[5] = {4, 4, 8, 8, 2};
    MotiveSketch sketch = line(pitches, 5, q(1, 4));
    Rational at = q(0, 1);
    for (size_t i = 0; i < 5; i++) {
        sketch.notes[i].onset = at;
        sketch.notes[i].duration = q(1, den[i]);
        at = msk_add(at, sketch.notes[i].duration);
    }
    sketch.length = at;
    MotiveSketch out;

    /* A A B C B (q q e e h): the C, the last B and the second B go; each note's time runs to the next kept note and the
       last kept note's to the end, so the rhythm has no rest. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 3, 1, 0), &out) == MOTIVE_RELATION_OK);
    int kept[2] = {81, 81};
    assert_pitches(&out, kept, 2);
    assert_note(&out, 0, 81, q(0, 1), q(1, 4));
    assert_note(&out, 1, 81, q(1, 4), q(1, 1));
    assert(msk_eq(out.length, q(5, 4)));
    for (size_t i = 0; i + 1 < out.note_count; i++) {
        assert(msk_eq(msk_add(out.notes[i].onset, out.notes[i].duration), out.notes[i + 1].onset));
    }

    /* A dropped first note gives its time to the next kept note, which starts where the first did. */
    int eighths[4] = {60, 62, 64, 67};
    MotiveSketch upbeat = line(eighths, 4, q(1, 8));
    upbeat.pickup = q(1, 8);
    assert(apply(&upbeat, step(MOTIVE_RELATION_LIQUIDATE, 2, 1, 0), &out) == MOTIVE_RELATION_OK);
    int after_first[2] = {62, 67};
    assert_pitches(&out, after_first, 2);
    assert_note(&out, 0, 62, q(0, 1), q(3, 8));
    assert_note(&out, 1, 67, q(3, 8), q(1, 8));

    /* Without keep-rhythm nothing changes: the notes follow one another from 0 with their own durations. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE, 3, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 81, q(0, 1), q(1, 4));
    assert_note(&out, 1, 81, q(1, 4), q(1, 4));

    /* A time that is not a written value is refused: five eighths. */
    int run[7] = {60, 62, 62, 62, 62, 62, 64};
    MotiveSketch long_gap = line(run, 7, q(1, 8));
    assert(apply(&long_gap, step(MOTIVE_RELATION_LIQUIDATE, 5, 1, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
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

/* --- harmony and the dominant form ---------------------------------------------------------------------- */

/* C E G E in quarter notes, with the chord given: I at 0, and optionally another. */
static MotiveSketch triad_line(void) {
    static const int pitches[4] = {60, 64, 67, 64};
    return line(pitches, 4, q(1, 4));
}

static void set_chords(MotiveSketch *sketch, const int *degrees, const Rational *onsets, size_t count) {
    sketch->chord_count = count;
    for (size_t i = 0; i < count; i++) {
        sketch->chords[i] = (MotiveSketchChord){onsets[i], degrees[i], 0, 0, false};
    }
}

static void test_implied_chord(void) {
    MotiveSketch sketch = triad_line();
    MotiveSketch out;
    /* No chords: the best implied chord, here the tonic, becomes the one chord at 0. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 1 && out.chords[0].degree == 1 && msk_is_zero(out.chords[0].onset));
    assert(out.chords[0].inversion == 1);

    MotiveSketch no_scale = sketch;
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    MotiveSketch rhythm = sketch;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    assert(apply(&rhythm, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_harmony_inversion(void) {
    MotiveSketch sketch = triad_line();
    int degrees[2] = {1, 5};
    Rational onsets[2] = {q(0, 1), q(1, 2)};
    set_chords(&sketch, degrees, onsets, 2);
    sketch.chords[1].seventh = true;
    MotiveSketch out;

    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 1, 2, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chords[1].inversion == 2);
    int same[4] = {60, 64, 67, 64};
    assert_pitches(&out, same, 4);

    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 1, 3, 0), &out) == MOTIVE_RELATION_OK);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 3, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 2, 1, 0), &out) == MOTIVE_RELATION_NO_HARMONY);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 4, 0), &out) == MOTIVE_RELATION_INVALID);
}

static void test_harmony_append(void) {
    MotiveSketch sketch = triad_line();
    MotiveSketch out;

    /* A V at the last note; the E on that beat is not a V tone, so it moves to the D. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 5, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[1].degree == 5 && msk_eq(out.chords[1].onset, q(3, 4)));
    int adapted[4] = {60, 64, 67, 62};
    assert_pitches(&out, adapted, 4);

    /* With a seventh the F is a chord tone too, and the E goes to the nearer D or F: the F is a step off. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 5, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chords[1].seventh);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 8, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 5, 2, 0), &out) == MOTIVE_RELATION_INVALID);
    int degrees[2] = {1, 5};
    Rational onsets[2] = {q(0, 1), q(3, 4)};
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_APPEND, 1, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_harmony_insert(void) {
    MotiveSketch sketch = triad_line();
    MotiveSketch out;

    /* The one chord's span is 0 to 1; the note onset nearest its middle is 1/2. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 0, 4, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[1].degree == 4 && msk_eq(out.chords[1].onset, q(1, 2)));
    /* The G on the IV beat is not a IV tone, and moves to the A (arrived from E, so up first). */
    assert(out.notes[2].pitch == 69);

    int degrees[2] = {1, 5};
    Rational onsets[2] = {q(0, 1), q(1, 4)};
    set_chords(&sketch, degrees, onsets, 2);
    /* The first chord spans 0 to 1/4, and no note starts inside it. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 0, 4, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 1, 4, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 3 && msk_eq(out.chords[2].onset, q(1, 2)));
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 2, 4, 0), &out) == MOTIVE_RELATION_NO_HARMONY);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_INSERT, 1, 0, 0), &out) == MOTIVE_RELATION_INVALID);

    /* A full chord list takes no more. */
    MotiveSketch full = triad_line();
    full.chord_count = MOTIVE_SKETCH_MAX_CHORDS;
    for (size_t c = 0; c < MOTIVE_SKETCH_MAX_CHORDS; c++) {
        full.chords[c] = (MotiveSketchChord){q(c, 16), 1, 0, 0, false};
    }
    assert(apply(&full, step(MOTIVE_RELATION_HARMONY_INSERT, 7, 4, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_harmony_substitute(void) {
    MotiveSketch sketch = triad_line();
    MotiveSketch out;

    /* The tonic becomes V, then I: the C and E of the first half move to B and D. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 5, 1), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[0].degree == 5 && out.chords[1].degree == 1);
    assert(msk_eq(out.chords[1].onset, q(1, 2)));
    assert(out.notes[0].pitch == 59 && out.notes[1].pitch == 62);

    /* Replaced by one chord without a succession. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 4, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 1 && out.chords[0].degree == 4);

    /* The same chord is no change; with none given, the implied chord is a change to the sketch. */
    int tonic[1] = {1};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch with_chord = sketch;
    set_chords(&with_chord, tonic, at_zero, 1);
    assert(apply(&with_chord, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 1, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 1, 0), &out) == MOTIVE_RELATION_OK);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 1, 5, 0), &out) == MOTIVE_RELATION_NO_HARMONY);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 8, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 5, 9), &out) == MOTIVE_RELATION_INVALID);
    int one[1] = {60};
    MotiveSketch one_note = line(one, 1, q(1, 4));
    assert(apply(&one_note, step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 5, 1), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_passing_harmony(void) {
    MotiveSketch sketch = triad_line();
    int degrees[2] = {1, 4};
    Rational onsets[2] = {q(0, 1), q(1, 2)};
    set_chords(&sketch, degrees, onsets, 2);
    MotiveSketch out;

    /* I to IV: a step between, the II, on the weak note before the change; its E moves to the F. */
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 3);
    assert(out.chords[1].degree == 2 && msk_eq(out.chords[1].onset, q(1, 4)));
    assert(out.chords[2].degree == 4);
    assert(out.notes[1].pitch == 65);

    /* Roots a step apart: the dominant of the second, here the VI before the II. */
    degrees[1] = 2;
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chords[1].degree == 6);

    /* Going down a step lower. */
    degrees[0] = 4;
    degrees[1] = 1;
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chords[1].degree == 3);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 1, 0, 0), &out) == MOTIVE_RELATION_NO_HARMONY);
    degrees[0] = 1;
    degrees[1] = 1;
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    degrees[1] = 4;
    onsets[1] = q(1, 4);
    set_chords(&sketch, degrees, onsets, 2);
    /* No note starts between the first chord and the change. */
    assert(apply(&sketch, step(MOTIVE_RELATION_PASSING_HARMONY, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
}

/* Hollow Knight 1 (A A B C B) in quarter notes over the given chords. */
static MotiveSketch hollow_knight(const int *degrees, const Rational *onsets, size_t count) {
    static const int pitches[5] = {81, 81, 83, 84, 83};
    MotiveSketch sketch = line(pitches, 5, q(1, 4));
    set_chords(&sketch, degrees, onsets, count);
    return sketch;
}

static void test_tone_role_adaptation(void) {
    int vi[1] = {6};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    MotiveSketch out;

    /* A V at the middle: the C (a chord tone of the vi) moves to the D, and the passing B before it follows by a
       scale step, so it still steps into its target. The last B is unclassified and stays. */
    MotiveRelationStep insert_v = step(MOTIVE_RELATION_HARMONY_INSERT, 0, 5, 0);
    assert(apply(&sketch, insert_v, &out) == MOTIVE_RELATION_OK);
    int re_resolved[5] = {81, 81, 84, 86, 83};
    assert_pitches(&out, re_resolved, 5);
    assert(motive_relation_harmony_changes_notes(&sketch, &insert_v));

    /* A I at the middle holds the C already: the chords change, the notes do not. */
    MotiveRelationStep insert_i = step(MOTIVE_RELATION_HARMONY_INSERT, 0, 1, 0);
    assert(apply(&sketch, insert_i, &out) == MOTIVE_RELATION_OK);
    int same[5] = {81, 81, 83, 84, 83};
    assert_pitches(&out, same, 5);
    assert(out.chord_count == 2);
    assert(!motive_relation_harmony_changes_notes(&sketch, &insert_i));

    /* A V appended at the last note: the B is a chord tone of it and was no chord tone before, so nothing moves. */
    MotiveRelationStep append_v = step(MOTIVE_RELATION_HARMONY_APPEND, 5, 0, 0);
    assert(apply(&sketch, append_v, &out) == MOTIVE_RELATION_OK);
    assert_pitches(&out, same, 5);
    assert(!motive_relation_harmony_changes_notes(&sketch, &append_v));

    /* Substituting a V for the vi moves the A A to the nearest V tone, B. */
    MotiveRelationStep substitute = step(MOTIVE_RELATION_HARMONY_SUBSTITUTE, 0, 5, 0);
    assert(apply(&sketch, substitute, &out) == MOTIVE_RELATION_OK);
    int fifth[5] = {83, 83, 84, 86, 83};
    assert_pitches(&out, fifth, 5);
    assert(motive_relation_harmony_changes_notes(&sketch, &substitute));

    /* The first inversion puts the C in the bass: the lowest note, the A, moves to the nearest C. */
    MotiveRelationStep first_inversion = step(MOTIVE_RELATION_HARMONY_INVERSION, 0, 1, 0);
    assert(apply(&sketch, first_inversion, &out) == MOTIVE_RELATION_OK);
    assert(out.chords[0].inversion == 1);
    int bass[5] = {84, 81, 83, 84, 83};
    assert_pitches(&out, bass, 5);
    assert(motive_relation_harmony_changes_notes(&sketch, &first_inversion));

    /* A bass that is not the old bass chord tone stays: here the lowest note is the C, not the vi's root A. */
    int lowest_c[5] = {84, 81, 83, 72, 83};
    MotiveSketch other = line(lowest_c, 5, q(1, 4));
    set_chords(&other, vi, at_zero, 1);
    assert(apply(&other, first_inversion, &out) == MOTIVE_RELATION_OK);
    assert_pitches(&out, lowest_c, 5);
    assert(!motive_relation_harmony_changes_notes(&other, &first_inversion));

    /* A refused step changes nothing. */
    assert(!motive_relation_harmony_changes_notes(&sketch, &(MotiveRelationStep){.kind = MOTIVE_RELATION_HARMONY_INSERT,
                                                                                .value = {5, 5, 0}}));
}

/* --- frame relations (R-197) -------------------------------------------------------------------- */

/* Whether the tone roles label a note as a non-chord tone of the type. */
static bool is_figure(const MotiveSketch *sketch, size_t note, NonChordTone type) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    assert(motive_tone_roles(sketch, NULL, 0, labels) == MOTIVE_TONE_OK);
    return labels[note].role == MOTIVE_TONE_NON_CHORD && labels[note].type == type;
}

static bool is_chord_tone(const MotiveSketch *sketch, size_t note) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    assert(motive_tone_roles(sketch, NULL, 0, labels) == MOTIVE_TONE_OK);
    return labels[note].role == MOTIVE_TONE_CHORD;
}

/* C E E G E C C in quarter notes over the tonic triad: chord tones only. */
static MotiveSketch frame_line(void) {
    static const int pitches[7] = {60, 64, 64, 67, 64, 60, 60};
    return line(pitches, 7, q(1, 4));
}

static void test_reduce_to_frame(void) {
    int vi[1] = {6};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    sketch.feature_count = 2;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    strcpy(sketch.features[1].label, "b");
    sketch.features[1].first = 2;
    sketch.features[1].last = 2;
    MotiveSketch out;

    /* The passing B and the unclassified last B go; their time goes to the A and the C before them. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    int frame[3] = {81, 81, 84};
    assert_pitches(&out, frame, 3);
    assert_note(&out, 0, 81, q(0, 1), q(1, 4));
    assert_note(&out, 1, 81, q(1, 4), q(1, 2));
    assert_note(&out, 2, 84, q(3, 4), q(1, 2));
    assert(msk_eq(out.length, q(5, 4)));
    assert(out.chord_count == 1);
    /* The feature on the B alone goes; the one on the two As stays. */
    assert(out.feature_count == 1 && out.features[0].first == 0 && out.features[0].last == 1);

    /* A leading non-chord note gives its time to the first kept note, which then starts at 0. */
    int leading[4] = {83, 81, 81, 84};
    MotiveSketch lead = line(leading, 4, q(1, 4));
    set_chords(&lead, vi, at_zero, 1);
    assert(apply(&lead, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    int after_lead[3] = {81, 81, 84};
    assert_pitches(&out, after_lead, 3);
    assert_note(&out, 0, 81, q(0, 1), q(1, 2));
    assert_note(&out, 1, 81, q(1, 2), q(1, 4));
    assert_note(&out, 2, 84, q(3, 4), q(1, 4));

    /* No chord is given: the tonic triad frames it. C D E G is C E G with the D's time on the C. */
    MotiveSketch plain = cdeg();
    assert(apply(&plain, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    int triad[3] = {60, 64, 67};
    assert_pitches(&out, triad, 3);
    assert_note(&out, 0, 60, q(0, 1), q(1, 2));
    assert(out.chord_count == 0);

    /* Refusals. */
    MotiveSketch chord_tones = frame_line();
    assert(apply(&chord_tones, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    int none_in_frame[3] = {62, 65, 62};
    MotiveSketch no_frame = line(none_in_frame, 3, q(1, 4));
    assert(apply(&no_frame, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    MotiveSketch no_scale = cdeg();
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    MotiveSketch rhythm = cdeg();
    rhythm.notes[1].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    assert(apply(&rhythm, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    /* An eighth and four more on one note (five eighths) are not a written value. */
    int eighths[6] = {60, 62, 62, 62, 62, 64};
    MotiveSketch long_run = line(eighths, 6, q(1, 8));
    assert(apply(&long_run, step(MOTIVE_RELATION_REDUCE_TO_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(motive_relation_param_count(MOTIVE_RELATION_REDUCE_TO_FRAME) == 0);
}

static void test_add_non_chord_tone(void) {
    MotiveSketch sketch = frame_line();
    MotiveSketch out;

    /* A passing tone between the E and the G: half of the E's quarter. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 2, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_OK);
    int passing[8] = {60, 64, 64, 65, 67, 64, 60, 60};
    assert_pitches(&out, passing, 8);
    assert_note(&out, 2, 64, q(1, 2), q(1, 8));
    assert_note(&out, 3, 65, q(5, 8), q(1, 8));
    assert_note(&out, 4, 67, q(3, 4), q(1, 4));
    assert(is_figure(&out, 3, NON_CHORD_TONE_PASSING));
    assert(msk_eq(out.length, sketch.length));

    /* Every type that can be formed there is labelled so by the tone roles. */
    static const struct {
        long after;
        NonChordTone type;
    } formed[] = {
        {1, NON_CHORD_TONE_NEIGHBOR},     {1, NON_CHORD_TONE_DOUBLE_NEIGHBOR}, {1, NON_CHORD_TONE_CHANGING_TONE},
        {0, NON_CHORD_TONE_APPOGGIATURA}, {0, NON_CHORD_TONE_ESCAPE_TONE},     {5, NON_CHORD_TONE_NEIGHBOR},
    };
    for (size_t i = 0; i < sizeof(formed) / sizeof(formed[0]); i++) {
        assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, formed[i].after, formed[i].type, 0), &out) ==
               MOTIVE_RELATION_OK);
        size_t first = (size_t)formed[i].after + 1;
        size_t added = formed[i].type == NON_CHORD_TONE_DOUBLE_NEIGHBOR || formed[i].type == NON_CHORD_TONE_CHANGING_TONE ? 2 : 1;
        assert(out.note_count == sketch.note_count + added);
        for (size_t k = 0; k < added; k++) {
            assert(is_figure(&out, first + k, formed[i].type));
        }
        assert(is_chord_tone(&out, (size_t)formed[i].after));
        assert(msk_eq(out.length, sketch.length));
    }
    /* The pair splits half of the E: C E [F D] E. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_CHANGING_TONE, 0), &out) ==
           MOTIVE_RELATION_OK);
    int changing[9] = {60, 64, 65, 62, 64, 67, 64, 60, 60};
    assert_pitches(&out, changing, 9);
    assert_note(&out, 1, 64, q(1, 4), q(1, 8));
    assert_note(&out, 2, 65, q(3, 8), q(1, 16));
    assert_note(&out, 3, 62, q(7, 16), q(1, 16));
    /* A double neighbor is a semitone each side (a third apart is a changing tone). */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_DOUBLE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_OK);
    assert(out.notes[2].pitch - out.notes[3].pitch == 2 || out.notes[3].pitch - out.notes[2].pitch == 2);

    /* A suspension needs a chord change where the figure starts: a V from the middle of the C. */
    int held[2] = {60, 59};
    MotiveSketch suspended = line(held, 2, q(1, 2));
    int degrees[2] = {1, 5};
    Rational onsets[2] = {q(0, 1), q(1, 4)};
    MotiveSketch no_change = suspended;
    assert(apply(&no_change, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_SUSPENSION, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    set_chords(&suspended, degrees, onsets, 2);
    assert(apply(&suspended, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_SUSPENSION, 0), &out) ==
           MOTIVE_RELATION_OK);
    int suspension[3] = {60, 60, 59};
    assert_pitches(&out, suspension, 3);
    assert(is_figure(&out, 1, NON_CHORD_TONE_SUSPENSION));
    /* An anticipation needs the chord change where the next note starts. */
    int early[2] = {67, 71};
    MotiveSketch arriving = line(early, 2, q(1, 2));
    Rational at_half[2] = {q(0, 1), q(1, 2)};
    set_chords(&arriving, degrees, at_half, 2);
    assert(apply(&arriving, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_ANTICIPATION, 0), &out) ==
           MOTIVE_RELATION_OK);
    int anticipation[3] = {67, 71, 71};
    assert_pitches(&out, anticipation, 3);
    assert(is_figure(&out, 1, NON_CHORD_TONE_ANTICIPATION));

    /* Features: one that holds the note before keeps its start and takes the new note. */
    MotiveSketch featured = frame_line();
    featured.feature_count = 1;
    strcpy(featured.features[0].label, "a");
    featured.features[0].first = 1;
    featured.features[0].last = 3;
    assert(apply(&featured, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_OK);
    assert(out.features[0].first == 1 && out.features[0].last == 4);

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 7, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, -1, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_COUNT, 0), &out) ==
           MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, -1, 0), &out) == MOTIVE_RELATION_INVALID);
    /* No note after the last. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 6, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    /* The note before must be a chord tone: the passing F added above is not. */
    MotiveSketch embellished;
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 2, NON_CHORD_TONE_PASSING, 0), &embellished) ==
           MOTIVE_RELATION_OK);
    assert(apply(&embellished, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 3, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    /* A passing tone cannot join a C to the same C, nor a pedal be formed by one note. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 5, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_PEDAL, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    /* A double neighbor needs the same note again after it: E then G is not. */
    assert(apply(&sketch, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 2, NON_CHORD_TONE_DOUBLE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    MotiveSketch no_scale = frame_line();
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NO_SCALE);
    /* Half of a thirty-second is not a written value, nor is a quarter of a sixteenth. */
    static const int three[3] = {60, 64, 64};
    MotiveSketch fine = line(three, 3, q(1, 32));
    assert(apply(&fine, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_NOT_EXPRESSIBLE);
    MotiveSketch sixteenths = line(three, 3, q(1, 16));
    assert(apply(&sixteenths, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_OK);
    assert(apply(&sixteenths, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_DOUBLE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_EXPRESSIBLE);
    /* The sketch cannot hold another note. */
    int alternating[MOTIVE_SKETCH_MAX_NOTES];
    for (size_t i = 0; i < MOTIVE_SKETCH_MAX_NOTES; i++) {
        alternating[i] = i % 2 ? 64 : 60;
    }
    MotiveSketch full = line(alternating, MOTIVE_SKETCH_MAX_NOTES, q(1, 8));
    assert(apply(&full, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 0, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
}

static void test_remove_non_chord_tone(void) {
    int vi[1] = {6};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    MotiveSketch out;

    /* The passing B goes; its quarter goes to the A before it. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 2, 0, 0), &out) == MOTIVE_RELATION_OK);
    int without_passing[4] = {81, 81, 84, 83};
    assert_pitches(&out, without_passing, 4);
    assert_note(&out, 1, 81, q(1, 4), q(1, 2));
    assert_note(&out, 2, 84, q(3, 4), q(1, 4));
    /* The unclassified last B goes too. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 4, 0, 0), &out) == MOTIVE_RELATION_OK);
    int without_last[4] = {81, 81, 83, 84};
    assert_pitches(&out, without_last, 4);
    assert_note(&out, 3, 84, q(3, 4), q(1, 2));

    /* A first note that is not a chord tone gives its time to the next. */
    int leading[4] = {83, 81, 81, 84};
    MotiveSketch lead = line(leading, 4, q(1, 4));
    set_chords(&lead, vi, at_zero, 1);
    assert(apply(&lead, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    int after_lead[3] = {81, 81, 84};
    assert_pitches(&out, after_lead, 3);
    assert_note(&out, 0, 81, q(0, 1), q(1, 2));

    /* Removing one of a pair leaves the other. */
    MotiveSketch pair;
    MotiveSketch plain = frame_line();
    assert(apply(&plain, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_CHANGING_TONE, 0), &pair) ==
           MOTIVE_RELATION_OK);
    assert(apply(&pair, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 2, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == pair.note_count - 1);
    assert_note(&out, 1, 64, q(1, 4), q(3, 16));

    /* Refusals: a chord tone, a note out of range, a sketch with no scale. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 5, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, -1, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    sketch.has_scale = false;
    assert(apply(&sketch, step(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, 2, 0, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    assert(motive_relation_param_count(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE) == 1);
}

static void test_change_non_chord_tone(void) {
    int vi[1] = {6};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    MotiveSketch out;

    /* The passing B between A and C becomes an appoggiatura (the D above the C) or an escape tone (the G below). */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_APPOGGIATURA, 0), &out) ==
           MOTIVE_RELATION_OK);
    int appoggiatura[5] = {81, 81, 86, 84, 83};
    assert_pitches(&out, appoggiatura, 5);
    assert(is_figure(&out, 2, NON_CHORD_TONE_APPOGGIATURA));
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_ESCAPE_TONE, 0), &out) ==
           MOTIVE_RELATION_OK);
    int escape[5] = {81, 81, 79, 84, 83};
    assert_pitches(&out, escape, 5);
    assert(is_figure(&out, 2, NON_CHORD_TONE_ESCAPE_TONE));
    /* The frame around it, the chord tones, is the same. */
    assert(is_chord_tone(&out, 0) && is_chord_tone(&out, 1) && is_chord_tone(&out, 3));

    /* Same type: nothing to do. A neighbor between A and C cannot be formed. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_PASSING, 0), &out) ==
           MOTIVE_RELATION_UNCHANGED);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    /* A pair type needs a non-chord note beside it. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_CHANGING_TONE, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    /* A chord tone, and the last note (no note after it), are refused; out of range is invalid. */
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 0, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 4, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_NOT_APPLICABLE);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 5, NON_CHORD_TONE_NEIGHBOR, 0), &out) ==
           MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_COUNT, 0), &out) ==
           MOTIVE_RELATION_INVALID);

    /* A double neighbor becomes a changing tone: both notes move, the chord tones stay. */
    MotiveSketch pair;
    MotiveSketch plain = frame_line();
    assert(apply(&plain, step(MOTIVE_RELATION_ADD_NON_CHORD_TONE, 1, NON_CHORD_TONE_DOUBLE_NEIGHBOR, 0), &pair) ==
           MOTIVE_RELATION_OK);
    assert(apply(&pair, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 3, NON_CHORD_TONE_CHANGING_TONE, 0), &out) ==
           MOTIVE_RELATION_OK);
    assert(out.note_count == pair.note_count);
    assert(is_figure(&out, 2, NON_CHORD_TONE_CHANGING_TONE) && is_figure(&out, 3, NON_CHORD_TONE_CHANGING_TONE));
    assert(out.notes[1].pitch == pair.notes[1].pitch && out.notes[4].pitch == pair.notes[4].pitch);
    /* ... and back, from either note of the pair. */
    MotiveSketch back;
    assert(apply(&out, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 2, NON_CHORD_TONE_DOUBLE_NEIGHBOR, 0), &back) ==
           MOTIVE_RELATION_OK);
    assert(is_figure(&back, 2, NON_CHORD_TONE_DOUBLE_NEIGHBOR) && is_figure(&back, 3, NON_CHORD_TONE_DOUBLE_NEIGHBOR));

    /* A single figure beside another non-chord note becomes a pair. */
    int two[6] = {60, 65, 62, 60, 64, 67};
    MotiveSketch beside = line(two, 6, q(1, 4));
    assert(apply(&beside, step(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, 1, NON_CHORD_TONE_CHANGING_TONE, 0), &out) ==
           MOTIVE_RELATION_OK);
    assert(is_figure(&out, 1, NON_CHORD_TONE_CHANGING_TONE) && is_figure(&out, 2, NON_CHORD_TONE_CHANGING_TONE));
    assert(motive_relation_param_count(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE) == 2);
}

static void test_reharmonize_frame(void) {
    int vi[1] = {6};
    Rational at_zero[1] = {q(0, 1)};
    MotiveSketch sketch = hollow_knight(vi, at_zero, 1);
    MotiveSketch out;

    /* The A A move to the nearest tone of the V, the C to the D (keeping the contour), and the passing B follows its
       target by a scale step. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 5, 0, 0), &out) == MOTIVE_RELATION_OK);
    int fifth[5] = {83, 83, 84, 86, 83};
    assert_pitches(&out, fifth, 5);
    assert(out.chord_count == 1 && out.chords[0].degree == 5 && !out.chords[0].seventh);
    assert(msk_eq(out.chords[0].onset, q(0, 1)));
    assert(is_chord_tone(&out, 0) && is_chord_tone(&out, 1) && is_chord_tone(&out, 3));
    assert(is_figure(&out, 2, NON_CHORD_TONE_PASSING));
    assert(out.chords[0].inversion == 0 && out.chords[0].alteration == 0);

    /* With a seventh. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 5, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chords[0].seventh);

    /* Several chords become the one: a vi then a V is a I. The A A fall to the G, the B to the C, the last B to the G
       (the nearest I tone that keeps its fall); the neighbor C, whose B moved, no longer fits and is unclassified. */
    int degrees[2] = {6, 5};
    Rational onsets[2] = {q(0, 1), q(1, 2)};
    MotiveSketch two = hollow_knight(degrees, onsets, 2);
    assert(apply(&two, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 1 && out.chords[0].degree == 1);
    int tonic[5] = {79, 79, 84, 81, 79};
    assert_pitches(&out, tonic, 5);
    assert(is_chord_tone(&out, 0) && is_chord_tone(&out, 1) && is_chord_tone(&out, 2) && is_chord_tone(&out, 4));
    assert(!is_chord_tone(&out, 3));

    /* A sketch with no chords gets the one. */
    MotiveSketch bare = frame_line();
    assert(apply(&bare, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 4, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 1 && out.chords[0].degree == 4);
    for (size_t i = 0; i < out.note_count; i++) {
        assert(is_chord_tone(&out, i));
    }

    /* The chord it already has changes nothing. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 6, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 8, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 5, 2, 0), &out) == MOTIVE_RELATION_INVALID);
    sketch.has_scale = false;
    assert(apply(&sketch, step(MOTIVE_RELATION_REHARMONIZE_FRAME, 5, 0, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    assert(motive_relation_param_count(MOTIVE_RELATION_REHARMONIZE_FRAME) == 2);
}

static void test_counter_line(void) {
    int pitches[4] = {60, 60, 60, 60};
    MotiveSketch accompaniment = line(pitches, 4, q(1, 4));
    accompaniment.accompaniment = true;
    int degrees[2] = {1, 5};
    Rational onsets[2] = {q(0, 1), q(1, 2)};
    set_chords(&accompaniment, degrees, onsets, 2);
    MotiveSketch out;

    /* Repeated notes inside a chord step toward the nearest tone of the next chord: the B of the V. */
    assert(apply(&accompaniment, step(MOTIVE_RELATION_COUNTER_LINE, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    int stepping[4] = {60, 59, 60, 59};
    assert_pitches(&out, stepping, 4);

    /* A melody is not an accompaniment. */
    accompaniment.accompaniment = false;
    assert(apply(&accompaniment, step(MOTIVE_RELATION_COUNTER_LINE, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);

    /* Notes that do not repeat are left alone. */
    MotiveSketch moving = triad_line();
    moving.accompaniment = true;
    set_chords(&moving, degrees, onsets, 2);
    assert(apply(&moving, step(MOTIVE_RELATION_COUNTER_LINE, 0, 0, 0), &out) == MOTIVE_RELATION_UNCHANGED);
}

static void test_dominant_form(void) {
    MotiveSketch out;

    /* I-V becomes V-I: the melody a tonal fourth down, adapted to the chords, in the same rhythm. */
    MotiveSketch sketch = triad_line();
    int degrees[3] = {1, 5, 1};
    Rational onsets[3] = {q(0, 1), q(1, 2), q(3, 4)};
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[0].degree == 5 && out.chords[1].degree == 1);
    assert(msk_eq(out.chords[1].onset, q(1, 2)));
    /* The last E was unclassified (it ends the motive), so it is left where the fourth put it. */
    int fourth_down[4] = {55, 59, 67, 59};
    assert_pitches(&out, fourth_down, 4);
    for (size_t i = 0; i < 4; i++) {
        assert(msk_eq(out.notes[i].onset, sketch.notes[i].onset));
        assert(msk_eq(out.notes[i].duration, sketch.notes[i].duration));
    }

    /* I alone becomes V. */
    set_chords(&sketch, degrees, onsets, 1);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 1 && out.chords[0].degree == 5);
    int plain[4] = {55, 59, 62, 59};
    assert_pitches(&out, plain, 4);

    /* I-V-I becomes V-I-V. */
    set_chords(&sketch, degrees, onsets, 3);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 3 && out.chords[0].degree == 5 && out.chords[1].degree == 1 && out.chords[2].degree == 5);

    /* I-IV, I-II and any other harmony starting on I become V-I. */
    degrees[1] = 4;
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[0].degree == 5 && out.chords[1].degree == 1);
    degrees[1] = 6;
    degrees[2] = 4;
    set_chords(&sketch, degrees, onsets, 3);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.chord_count == 2 && out.chords[0].degree == 5 && out.chords[1].degree == 1);

    /* Refusals: a harmony not starting on I; a fourth down out of range goes up a fifth. */
    degrees[0] = 4;
    set_chords(&sketch, degrees, onsets, 2);
    assert(apply(&sketch, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    degrees[0] = 1;
    set_chords(&sketch, degrees, onsets, 1);
    MotiveSketch low = sketch;
    low.notes[0].pitch = 2;
    assert(apply(&low, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_OK);
    /* The fourth down is out of range, so the fifth up (A). The D was no chord tone of the I, so the V leaves it. */
    assert(out.notes[0].pitch == 9);
    MotiveSketch no_scale = sketch;
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_DOMINANT_FORM, 0, 0, 0), &out) == MOTIVE_RELATION_NO_SCALE);
}

/* --- liquidation by characteristic features -------------------------------------------------------------- */

/* Eight eighths: C E G C' B G E C. The beat groups are CE, GC', BG and EC. */
static MotiveSketch arch(void) {
    static const int pitches[8] = {60, 64, 67, 72, 71, 67, 64, 60};
    return line(pitches, 8, q(1, 8));
}

static void test_liquidate_features(void) {
    MotiveSketch out;

    /* Without labelled features each beat is one. GC' has the only leap (a fourth), so it goes first, replaced by one
     * note of its span on the nearest tone of its chord (the implied tonic): the G. */
    MotiveSketch sketch = arch();
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    int one[7] = {60, 64, 67, 71, 67, 64, 60};
    assert_pitches(&out, one, 7);
    assert_note(&out, 2, 67, q(1, 4), q(1, 4));
    assert_note(&out, 3, 71, q(1, 2), q(1, 8));
    assert(out.chord_count == 1 && out.chords[0].degree == 1);

    /* Two steps: of the beats left, all weigh 0 (the G now has a duration of its own but is one note), and of equals
     * the later goes: the E and C become the E. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 2, 0, 0), &out) == MOTIVE_RELATION_OK);
    int two[6] = {60, 64, 67, 71, 67, 64};
    assert_pitches(&out, two, 6);
    assert_note(&out, 5, 64, q(3, 4), q(1, 4));

    /* Shortened: the result diminished 1:2. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 1, 0), &out) == MOTIVE_RELATION_OK);
    assert_pitches(&out, one, 7);
    assert_note(&out, 2, 67, q(1, 8), q(1, 8));
    assert_note(&out, 3, 71, q(1, 4), q(1, 16));
    assert(msk_eq(out.length, q(1, 2)));
}

static void test_liquidate_labelled_features(void) {
    MotiveSketch out;

    /* The leap makes the second feature the heavier. */
    MotiveSketch sketch = arch();
    sketch.feature_count = 2;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    strcpy(sketch.features[1].label, "b");
    sketch.features[1].first = 2;
    sketch.features[1].last = 3;
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 7 && out.notes[2].pitch == 67);
    assert(out.feature_count == 1 && strcmp(out.features[0].label, "a") == 0);

    /* Equal weights: the later feature first. */
    sketch.features[1].first = 6;
    sketch.features[1].last = 7;
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 7 && out.notes[6].pitch == 64);
    assert(out.feature_count == 1 && strcmp(out.features[0].label, "a") == 0);

    /* A note whose duration is found once weighs 1. */
    static const int pitches[4] = {60, 62, 64, 65};
    MotiveSketch durations = line(pitches, 4, q(1, 8));
    durations.notes[3].duration = q(1, 4);
    durations.length = q(5, 8);
    durations.feature_count = 2;
    strcpy(durations.features[0].label, "a");
    durations.features[0].first = 0;
    durations.features[0].last = 1;
    strcpy(durations.features[1].label, "b");
    durations.features[1].first = 2;
    durations.features[1].last = 3;
    assert(apply(&durations, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 3);
    /* The implied chord of C D E F is the IV (F A C: two notes and the most time), whose nearest tone is the F. */
    assert_note(&out, 2, 65, q(1, 4), q(3, 8));

    /* Refusals. */
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 0, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 4, 0, 0), &out) == MOTIVE_RELATION_INVALID);
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 2, 0), &out) == MOTIVE_RELATION_INVALID);
    /* Three steps, but only two features of two notes. */
    sketch.feature_count = 2;
    sketch.features[1].first = 2;
    sketch.features[1].last = 3;
    assert(apply(&sketch, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 3, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    /* One note to a beat: no feature of two notes. */
    MotiveSketch quarters = cged();
    quarters.feature_count = 0;
    assert(apply(&quarters, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_NOT_APPLICABLE);
    MotiveSketch no_scale = arch();
    no_scale.has_scale = false;
    assert(apply(&no_scale, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_NO_SCALE);
    /* Shortening a 32nd note: half of it is not a written value. */
    MotiveSketch fine = arch();
    fine.notes[7].duration = q(1, 32);
    fine.length = q(29, 32);
    assert(apply(&fine, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 1, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
}

/* --- triplet motives ------------------------------------------------------------------------- */

/* Three eighth triplets, then a quarter, in 4/4 in C major. */
static MotiveSketch triplet_motive(void) {
    static const int pitches[3] = {60, 64, 67};
    MotiveSketch sketch = line(pitches, 3, q(1, 12));
    sketch.notes[3].pitch = 72;
    sketch.notes[3].onset = q(1, 4);
    sketch.notes[3].duration = q(1, 4);
    sketch.note_count = 4;
    sketch.length = q(1, 2);
    return sketch;
}

static void test_triplet_motive_relations(void) {
    MotiveSketch sketch = triplet_motive();
    MotiveSketch out;

    /* Augment 2:1 doubles every value: three 1/6 notes, then a half note. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_AUGMENT, 2, 1), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(0, 1), q(1, 6));
    assert_note(&out, 1, 64, q(1, 6), q(1, 6));
    assert_note(&out, 2, 67, q(1, 3), q(1, 6));
    assert_note(&out, 3, 72, q(1, 2), q(1, 2));

    /* Diminish 1:2 halves them: a 1/12 note, then a 1/24 and so on down to a triplet sixteenth. */
    assert(apply(&sketch, ratio_step(MOTIVE_RELATION_DIMINISH, 1, 2), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 0, 60, q(0, 1), q(1, 24));
    assert_note(&out, 1, 64, q(1, 24), q(1, 24));
    assert_note(&out, 3, 72, q(1, 8), q(1, 8));

    /* A binary motive is as before: note repeat of a quarter in three needs a triplet, which it does not hold. */
    MotiveSketch quarters = cdeg();
    assert(apply(&quarters, step(MOTIVE_RELATION_NOTE_REPEAT, 1, 3, 0), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);
    assert(apply(&quarters, ratio_step(MOTIVE_RELATION_AUGMENT, 5, 4), &out) == MOTIVE_RELATION_NOT_EXPRESSIBLE);

    /* A triplet motive may repeat its quarter in three. */
    assert(apply(&sketch, step(MOTIVE_RELATION_NOTE_REPEAT, 3, 3, 0), &out) == MOTIVE_RELATION_OK);
    assert_note(&out, 3, 72, q(1, 4), q(1, 12));
    assert_note(&out, 5, 72, q(5, 12), q(1, 12));

    /* Liquidating features keeps the triplets it does not touch and replaces each beat by one note. */
    static const int six[6] = {60, 64, 67, 72, 71, 67};
    MotiveSketch two_beats = line(six, 6, q(1, 12));
    two_beats.notes[3].onset = q(1, 4);
    two_beats.notes[4].onset = q(1, 3);
    two_beats.notes[5].onset = q(5, 12);
    two_beats.length = q(1, 2);
    assert(apply(&two_beats, step(MOTIVE_RELATION_LIQUIDATE_FEATURES, 1, 0, 0), &out) == MOTIVE_RELATION_OK);
    assert(out.note_count == 4);
}

int main(void) {
    test_catalog();
    test_invalid_input();
    test_exact();
    test_transpose();
    test_invert();
    test_tonal_relations_on_every_scale_note();
    test_scaled_pickup();
    test_retrograde();
    test_interval_change();
    test_fragment();
    test_liquidate();
    test_liquidate_keeps_time();
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
    test_implied_chord();
    test_harmony_inversion();
    test_harmony_append();
    test_harmony_insert();
    test_harmony_substitute();
    test_passing_harmony();
    test_tone_role_adaptation();
    test_reduce_to_frame();
    test_add_non_chord_tone();
    test_remove_non_chord_tone();
    test_change_non_chord_tone();
    test_reharmonize_frame();
    test_counter_line();
    test_dominant_form();
    test_liquidate_features();
    test_liquidate_labelled_features();
    test_triplet_motive_relations();
    printf("motive_relation: ok\n");
    return 0;
}
