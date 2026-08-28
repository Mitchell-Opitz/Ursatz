#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_sketch.h"

/*
 * Analysis-regression tests for MotiveSketch: validation (ordering, overlap, pickup, chords, features), equality,
 * scale steps in major, minor and without a scale, chord tones, expressible durations and the metric strength of an
 * onset.
 */

static ExactRational q(unsigned long long n, unsigned long long d) {
    return er_make(n, d);
}

/* Four quarter notes, C D E G, in 4/4 in C major. */
static MotiveSketch four_quarters(void) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    static const int pitches[4] = {60, 62, 64, 67};
    for (unsigned i = 0; i < 4; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = q(i, 4);
        sketch.notes[i].duration = q(1, 4);
    }
    sketch.note_count = 4;
    sketch.has_scale = true;
    sketch.tonic_pc = 0;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = q(1, 1);
    return sketch;
}

static void test_valid_sketch(void) {
    MotiveSketch sketch = four_quarters();
    assert(motive_sketch_validate(&sketch));
    assert(!motive_sketch_validate(NULL));

    sketch.notes[1].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    assert(motive_sketch_validate(&sketch));
}

static void test_refused_sketches(void) {
    MotiveSketch sketch = four_quarters();
    sketch.note_count = 0;
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.notes[2].pitch = 128;
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.notes[1].onset = q(1, 8); /* overlaps the first note */
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.notes[3].duration = q(0, 1);
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.pickup = q(1, 1); /* not below the bar */
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.length = q(3, 4); /* before the end of the last note */
    assert(!motive_sketch_validate(&sketch));

    sketch = four_quarters();
    sketch.notes[2].onset = msk_bad();
    assert(!motive_sketch_validate(&sketch));
}

static void test_chords_and_features(void) {
    MotiveSketch sketch = four_quarters();
    sketch.chord_count = 2;
    sketch.chords[0] = (MotiveSketchChord){q(0, 1), 1, 0, 0, false};
    sketch.chords[1] = (MotiveSketchChord){q(1, 2), 5, 0, 1, true};
    assert(motive_sketch_validate(&sketch));

    MotiveSketch bad = sketch;
    bad.chords[1].onset = q(0, 1); /* not ascending */
    assert(!motive_sketch_validate(&bad));
    bad = sketch;
    bad.chords[1].onset = q(1, 1); /* not inside the length */
    assert(!motive_sketch_validate(&bad));
    bad = sketch;
    bad.chords[0].degree = 8;
    assert(!motive_sketch_validate(&bad));
    bad = sketch;
    bad.chords[0].inversion = 4;
    assert(!motive_sketch_validate(&bad));

    sketch.feature_count = 1;
    strcpy(sketch.features[0].label, "a");
    sketch.features[0].first = 1;
    sketch.features[0].last = 2;
    assert(motive_sketch_validate(&sketch));

    bad = sketch;
    bad.features[0].last = 4; /* past the notes */
    assert(!motive_sketch_validate(&bad));
    bad = sketch;
    bad.features[0].label[0] = '\0';
    assert(!motive_sketch_validate(&bad));
    bad = sketch;
    memcpy(bad.features[0].label, "ninechars", 9); /* no terminator */
    assert(!motive_sketch_validate(&bad));
}

static void test_equality(void) {
    MotiveSketch a = four_quarters();
    MotiveSketch b = four_quarters();
    assert(motive_sketch_equal(&a, &b));

    b.notes[3].pitch = 69;
    assert(!motive_sketch_equal(&a, &b));

    b = four_quarters();
    b.pickup = q(1, 4);
    assert(!motive_sketch_equal(&a, &b));

    b = four_quarters();
    b.chord_count = 1;
    b.chords[0] = (MotiveSketchChord){q(0, 1), 1, 0, 0, false};
    assert(!motive_sketch_equal(&a, &b));
    a = b;
    assert(motive_sketch_equal(&a, &b));
    b.chords[0].inversion = 1;
    assert(!motive_sketch_equal(&a, &b));

    /* Notes past the count are not compared. */
    a = four_quarters();
    b = four_quarters();
    b.notes[10].pitch = 99;
    assert(motive_sketch_equal(&a, &b));
}

static void test_steps_in_major(void) {
    MotiveSketch sketch = four_quarters();
    int alteration = 9;
    /* Steps count from MIDI 0: seven to an octave, so middle C (60) is step 35. */
    assert(motive_sketch_step(&sketch, 60, &alteration) == 35);
    assert(alteration == 0);
    assert(motive_sketch_step(&sketch, 67, NULL) == 39);
    assert(motive_sketch_step(&sketch, 72, NULL) == 42);
    assert(motive_sketch_step(&sketch, 59, NULL) == 34);
    /* C sharp is on the step of C, raised. */
    assert(motive_sketch_step(&sketch, 61, &alteration) == 35);
    assert(alteration == 1);

    assert(motive_sketch_pitch_at_step(&sketch, 35, 0) == 60);
    assert(motive_sketch_pitch_at_step(&sketch, 39, 0) == 67);
    assert(motive_sketch_pitch_at_step(&sketch, 34, 0) == 59);
    assert(motive_sketch_pitch_at_step(&sketch, 35, 1) == 61);
    assert(motive_sketch_degree_of_step(&sketch, 39) == 5);
    assert(motive_sketch_degree_of_step(&sketch, 34) == 7);
}

static void test_steps_in_minor(void) {
    MotiveSketch sketch = four_quarters();
    sketch.tonic_pc = 9; /* A natural minor */
    sketch.minor = true;
    assert(motive_sketch_step(&sketch, 69, NULL) == 35);
    assert(motive_sketch_step(&sketch, 72, NULL) == 37); /* C is the third */
    assert(motive_sketch_step(&sketch, 77, NULL) == 40); /* F is the sixth */
    assert(motive_sketch_step(&sketch, 79, NULL) == 41); /* G is the seventh */
    assert(motive_sketch_pitch_at_step(&sketch, 37, 0) == 72);
    assert(motive_sketch_pitch_at_step(&sketch, 41, 0) == 79);
    assert(motive_sketch_pitch_at_step(&sketch, 42, 0) == 81);
}

static void test_steps_without_a_scale(void) {
    MotiveSketch sketch = four_quarters();
    sketch.has_scale = false;
    int alteration = 5;
    assert(motive_sketch_step(&sketch, 61, &alteration) == 61);
    assert(alteration == 0);
    assert(motive_sketch_pitch_at_step(&sketch, 64, 0) == 64);
    assert(motive_sketch_degree_of_step(&sketch, 64) == 0);
}

static void test_chord_tones(void) {
    MotiveSketch sketch = four_quarters();
    int pcs[4];

    MotiveSketchChord tonic = {q(0, 1), 1, 0, 0, false};
    assert(motive_sketch_chord_tones(&sketch, &tonic, pcs) == 3);
    assert(pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7);

    MotiveSketchChord supertonic = {q(0, 1), 2, 0, 0, false};
    assert(motive_sketch_chord_tones(&sketch, &supertonic, pcs) == 3);
    assert(pcs[0] == 2 && pcs[1] == 5 && pcs[2] == 9);

    MotiveSketchChord dominant_seventh = {q(0, 1), 5, 0, 0, true};
    assert(motive_sketch_chord_tones(&sketch, &dominant_seventh, pcs) == 4);
    assert(pcs[0] == 7 && pcs[1] == 11 && pcs[2] == 2 && pcs[3] == 5);

    /* An altered chord is major on the altered root. */
    MotiveSketchChord flat_six = {q(0, 1), 6, -1, 0, false};
    assert(motive_sketch_chord_tones(&sketch, &flat_six, pcs) == 3);
    assert(pcs[0] == 8 && pcs[1] == 0 && pcs[2] == 3);

    /* In minor the dominant has a raised third. */
    sketch.tonic_pc = 9;
    sketch.minor = true;
    MotiveSketchChord minor_dominant = {q(0, 1), 5, 0, 0, false};
    assert(motive_sketch_chord_tones(&sketch, &minor_dominant, pcs) == 3);
    assert(pcs[0] == 4 && pcs[1] == 8 && pcs[2] == 11);
    MotiveSketchChord minor_tonic = {q(0, 1), 1, 0, 0, false};
    assert(motive_sketch_chord_tones(&sketch, &minor_tonic, pcs) == 3);
    assert(pcs[0] == 9 && pcs[1] == 0 && pcs[2] == 4);

    sketch.has_scale = false;
    assert(motive_sketch_chord_tones(&sketch, &minor_tonic, pcs) == 0);
}

static void test_expressible_durations(void) {
    assert(motive_sketch_expressible(q(1, 1)));
    assert(motive_sketch_expressible(q(1, 4)));
    assert(motive_sketch_expressible(q(3, 8)));  /* dotted quarter */
    assert(motive_sketch_expressible(q(7, 16))); /* double-dotted quarter */
    assert(motive_sketch_expressible(q(1, 32)));
    assert(motive_sketch_expressible(q(3, 64)));

    assert(!motive_sketch_expressible(q(1, 3)));
    assert(!motive_sketch_expressible(q(1, 64)));
    assert(!motive_sketch_expressible(q(5, 8)));
    assert(!motive_sketch_expressible(q(0, 1)));
    assert(!motive_sketch_expressible(msk_bad()));

    assert(motive_sketch_on_grid(q(0, 1)));
    assert(motive_sketch_on_grid(q(3, 32)));
    assert(!motive_sketch_on_grid(q(1, 3)));
    assert(!motive_sketch_on_grid(q(1, 64)));
}

static void test_strength(void) {
    MotiveSketch sketch = four_quarters();
    assert(motive_sketch_strength(&sketch, q(0, 1)) == 2);
    assert(motive_sketch_strength(&sketch, q(1, 4)) == 1);
    assert(motive_sketch_strength(&sketch, q(3, 8)) == 0);
    assert(motive_sketch_strength(&sketch, q(1, 1)) == 2);

    /* With a quarter-note pickup the first downbeat is at 1/4. */
    sketch.pickup = q(1, 4);
    assert(motive_sketch_strength(&sketch, q(0, 1)) == 1);
    assert(motive_sketch_strength(&sketch, q(1, 4)) == 2);
    assert(motive_sketch_strength(&sketch, q(5, 4)) == 2);
}

static void test_rational_helpers(void) {
    assert(msk_eq(msk_add(q(1, 4), q(1, 4)), q(1, 2)));
    assert(msk_eq(msk_sub(q(3, 4), q(1, 4)), q(1, 2)));
    assert(!msk_ok(msk_sub(q(1, 4), q(3, 4))));
    assert(msk_eq(msk_div(q(1, 2), q(1, 4)), q(2, 1)));
    assert(!msk_ok(msk_div(q(1, 2), q(0, 1))));
    assert(msk_eq(msk_mod(q(5, 4), q(1, 1)), q(1, 4)));
    assert(msk_floor_div(q(5, 4), q(1, 1)) == 1);
    assert(!msk_ok(msk_add(msk_bad(), q(1, 4))));
}

int main(void) {
    test_valid_sketch();
    test_refused_sketches();
    test_chords_and_features();
    test_equality();
    test_steps_in_major();
    test_steps_in_minor();
    test_steps_without_a_scale();
    test_chord_tones();
    test_expressible_durations();
    test_strength();
    test_rational_helpers();
    printf("motive_sketch: ok\n");
    return 0;
}
