#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_tone_roles.h"

/*
 * Analysis-regression tests for the motive tone roles: each non-chord tone type on a small sketch in C major,
 * Hollow Knight 1 (A5 A5 B5 C6 B5) over vi and over I, a sketch without chords, the unresolved end of a
 * motive, overrides, and the refusals.
 */

static ExactRational q(unsigned long long n, unsigned long long d) {
    return er_make(n, d);
}

/* Quarter notes from the given pitches in C major, 4/4. */
static MotiveSketch line(const int *pitches, size_t count) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    ExactRational at = q(0, 1);
    for (size_t i = 0; i < count; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = at;
        sketch.notes[i].duration = q(1, 4);
        at = msk_add(at, q(1, 4));
    }
    sketch.note_count = count;
    sketch.has_scale = true;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = at;
    return sketch;
}

static void add_chord(MotiveSketch *sketch, unsigned long long num, unsigned long long den, int degree) {
    MotiveSketchChord chord = {q(num, den), degree, 0, 0, false};
    sketch->chords[sketch->chord_count++] = chord;
}

static void expect_ok(const MotiveSketch *sketch, MotiveToneLabel *labels) {
    assert(motive_tone_roles(sketch, NULL, 0, labels) == MOTIVE_TONE_OK);
}

static void expect_chord(const MotiveToneLabel *label) {
    assert(label->role == MOTIVE_TONE_CHORD);
    assert(label->resolves_to == -1);
    assert(!label->unresolved);
    assert(!label->overridden);
}

static void expect_non_chord(const MotiveToneLabel *label, NonChordTone type, long resolves_to) {
    assert(label->role == MOTIVE_TONE_NON_CHORD);
    assert(label->type == type);
    assert(label->resolves_to == resolves_to);
    assert(label->unresolved == (resolves_to < 0));
}

static void expect_unclassified(const MotiveToneLabel *label, long resolves_to) {
    assert(label->role == MOTIVE_TONE_UNCLASSIFIED);
    assert(label->resolves_to == resolves_to);
    assert(label->unresolved == (resolves_to < 0));
}

static void test_simple_figures(void) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];

    /* No chords: the tonic triad. C D E: D passes. */
    static const int passing[3] = {60, 62, 64};
    MotiveSketch sketch = line(passing, 3);
    expect_ok(&sketch, labels);
    expect_chord(&labels[0]);
    expect_non_chord(&labels[1], NON_CHORD_TONE_PASSING, 2);
    expect_chord(&labels[2]);

    static const int neighbor[3] = {60, 62, 60};
    sketch = line(neighbor, 3);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_NEIGHBOR, 2);

    /* E A G: A is left by step, approached by leap. */
    static const int appoggiatura[3] = {64, 69, 67};
    sketch = line(appoggiatura, 3);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_APPOGGIATURA, 2);

    /* E F C: F is approached by step, left by leap. */
    static const int escape[3] = {64, 65, 60};
    sketch = line(escape, 3);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_ESCAPE_TONE, 2);

    /* C C B over I I V... the C is held into the V and steps down to B. */
    static const int suspension[3] = {60, 60, 59};
    sketch = line(suspension, 3);
    add_chord(&sketch, 0, 1, 1);
    add_chord(&sketch, 1, 4, 5);
    expect_ok(&sketch, labels);
    expect_chord(&labels[0]);
    expect_non_chord(&labels[1], NON_CHORD_TONE_SUSPENSION, 2);
    expect_chord(&labels[2]);

    /* B B C: the B is held into the I and steps up to C. */
    static const int retardation[3] = {59, 59, 60};
    sketch = line(retardation, 3);
    add_chord(&sketch, 0, 1, 5);
    add_chord(&sketch, 1, 4, 1);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_RETARDATION, 2);

    /* C B B: the first B arrives early, before the V that holds it. */
    static const int anticipation[3] = {60, 59, 59};
    sketch = line(anticipation, 3);
    add_chord(&sketch, 0, 1, 1);
    add_chord(&sketch, 1, 2, 5);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_ANTICIPATION, 2);
    expect_chord(&labels[2]);

    /* C D B C: both outer neighbors, a third apart: changing tones, resolving to the last C. */
    static const int changing[4] = {60, 62, 59, 60};
    sketch = line(changing, 4);
    expect_ok(&sketch, labels);
    expect_chord(&labels[0]);
    expect_non_chord(&labels[1], NON_CHORD_TONE_CHANGING_TONE, 3);
    expect_non_chord(&labels[2], NON_CHORD_TONE_CHANGING_TONE, 3);
    expect_chord(&labels[3]);

    /* C C# B C: both a semitone from the C: double neighbors. */
    static const int double_neighbor[4] = {60, 61, 59, 60};
    sketch = line(double_neighbor, 4);
    expect_ok(&sketch, labels);
    expect_non_chord(&labels[1], NON_CHORD_TONE_DOUBLE_NEIGHBOR, 3);
    expect_non_chord(&labels[2], NON_CHORD_TONE_DOUBLE_NEIGHBOR, 3);

    /* D D D D G over I IV I IV I: a D held across four chord changes, never a chord tone, then a G. */
    static const int pedal[5] = {62, 62, 62, 62, 67};
    sketch = line(pedal, 5);
    add_chord(&sketch, 0, 1, 1);
    add_chord(&sketch, 1, 4, 4);
    add_chord(&sketch, 1, 2, 1);
    add_chord(&sketch, 3, 4, 4);
    add_chord(&sketch, 1, 1, 1);
    expect_ok(&sketch, labels);
    for (size_t i = 0; i < 4; i++) {
        expect_non_chord(&labels[i], NON_CHORD_TONE_PEDAL, 4);
    }
    expect_chord(&labels[4]);
}

static void test_unresolved_end(void) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    /* C D: the D has no note after it. */
    static const int pitches[2] = {60, 62};
    MotiveSketch sketch = line(pitches, 2);
    expect_ok(&sketch, labels);
    expect_chord(&labels[0]);
    expect_unclassified(&labels[1], -1);
}

static void test_hollow_knight(void) {
    static const int pitches[5] = {81, 81, 83, 84, 83};
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];

    /* Over vi (A C E): the B passes from A to C; the last B has nothing after it. */
    MotiveSketch sketch = line(pitches, 5);
    add_chord(&sketch, 0, 1, 6);
    expect_ok(&sketch, labels);
    expect_chord(&labels[0]);
    expect_chord(&labels[1]);
    expect_non_chord(&labels[2], NON_CHORD_TONE_PASSING, 3);
    expect_chord(&labels[3]);
    expect_unclassified(&labels[4], -1);

    /* Over I (C E G): the A A are not chord tones and fit no figure, the B still passes to the C. */
    sketch = line(pitches, 5);
    add_chord(&sketch, 0, 1, 1);
    expect_ok(&sketch, labels);
    expect_unclassified(&labels[0], -1);
    expect_unclassified(&labels[1], -1);
    expect_non_chord(&labels[2], NON_CHORD_TONE_PASSING, 3);
    expect_chord(&labels[3]);
    expect_unclassified(&labels[4], -1);
}

static void test_overrides(void) {
    static const int pitches[5] = {81, 81, 83, 84, 83};
    MotiveSketch sketch = line(pitches, 5);
    add_chord(&sketch, 0, 1, 1);
    MotiveToneLabel plain[MOTIVE_SKETCH_MAX_NOTES];
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    expect_ok(&sketch, plain);

    /* The passing B called a chord tone; the second A called a suspension; the last B a pedal. */
    MotiveToneOverride overrides[3] = {
        {2, MOTIVE_TONE_CHORD, NON_CHORD_TONE_PASSING},
        {1, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_SUSPENSION},
        {4, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_PEDAL},
    };
    assert(motive_tone_roles(&sketch, overrides, 3, labels) == MOTIVE_TONE_OK);
    assert(labels[0].role == plain[0].role && !labels[0].overridden);
    assert(labels[3].role == MOTIVE_TONE_CHORD && !labels[3].overridden);

    assert(labels[2].overridden);
    assert(labels[2].role == MOTIVE_TONE_CHORD);
    assert(labels[2].resolves_to == -1 && !labels[2].unresolved);

    /* Next note (the B) is not a chord tone by pitch class, so the suspension does not resolve. */
    assert(labels[1].overridden);
    assert(labels[1].role == MOTIVE_TONE_NON_CHORD && labels[1].type == NON_CHORD_TONE_SUSPENSION);
    assert(labels[1].resolves_to == -1 && labels[1].unresolved);

    /* The pedal on the last B has no note after its run. */
    assert(labels[4].overridden && labels[4].type == NON_CHORD_TONE_PEDAL);
    assert(labels[4].resolves_to == -1 && labels[4].unresolved);

    /* A pedal on the passing B resolves to the C after its run. */
    MotiveToneOverride pedal = {2, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_PEDAL};
    assert(motive_tone_roles(&sketch, &pedal, 1, labels) == MOTIVE_TONE_OK);
    expect_non_chord(&labels[2], NON_CHORD_TONE_PEDAL, 3);
    assert(labels[2].overridden);

    /* The later of two overrides on one note wins. */
    MotiveToneOverride twice[2] = {
        {2, MOTIVE_TONE_CHORD, NON_CHORD_TONE_PASSING},
        {2, MOTIVE_TONE_UNCLASSIFIED, NON_CHORD_TONE_PASSING},
    };
    assert(motive_tone_roles(&sketch, twice, 2, labels) == MOTIVE_TONE_OK);
    assert(labels[2].role == MOTIVE_TONE_UNCLASSIFIED && labels[2].resolves_to == 3);
}

static void test_refusals(void) {
    static const int pitches[3] = {60, 62, 64};
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveSketch sketch = line(pitches, 3);

    MotiveToneOverride off_end = {3, MOTIVE_TONE_CHORD, NON_CHORD_TONE_PASSING};
    assert(motive_tone_roles(&sketch, &off_end, 1, labels) == MOTIVE_TONE_BAD_OVERRIDE);
    MotiveToneOverride bad_type = {1, MOTIVE_TONE_NON_CHORD, (NonChordTone)99};
    assert(motive_tone_roles(&sketch, &bad_type, 1, labels) == MOTIVE_TONE_BAD_OVERRIDE);
    MotiveToneOverride bad_role = {1, (MotiveToneRole)7, NON_CHORD_TONE_PASSING};
    assert(motive_tone_roles(&sketch, &bad_role, 1, labels) == MOTIVE_TONE_BAD_OVERRIDE);
    assert(motive_tone_roles(&sketch, NULL, 1, labels) == MOTIVE_TONE_INVALID);

    sketch.has_scale = false;
    assert(motive_tone_roles(&sketch, NULL, 0, labels) == MOTIVE_TONE_NO_SCALE);

    sketch = line(pitches, 3);
    sketch.note_count = 0;
    assert(motive_tone_roles(&sketch, NULL, 0, labels) == MOTIVE_TONE_INVALID);

    /* An unknown pitch is unclassified, never unresolved, and breaks the rules around it. */
    sketch = line(pitches, 3);
    sketch.notes[1].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    assert(motive_tone_roles(&sketch, NULL, 0, labels) == MOTIVE_TONE_OK);
    expect_chord(&labels[0]);
    assert(labels[1].role == MOTIVE_TONE_UNCLASSIFIED && !labels[1].unresolved);
}

int main(void) {
    test_simple_figures();
    test_unresolved_end();
    test_hollow_knight();
    test_overrides();
    test_refusals();
    printf("motive tone roles: ok\n");
    return 0;
}
