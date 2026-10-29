#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_sketch.h"
#include "analysis/motive_tone_roles.h"
#include "pitch/pitch_spelling.h"

/*
 * MotiveSketch as the one motive value: validate, equal and fingerprint over the form, key source, tonic spelling and
 * tone overrides; three fingerprints pinned from before those fields existed; the tone overrides carried through
 * motive_tone_roles_of; bar, offset in bar and on-beat with and without a pickup.
 */

static Rational q(unsigned long long n, unsigned long long d) {
    return rational_ratio(n, d);
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
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = q(1, 1);
    return sketch;
}

/* The three sketches whose fingerprints were taken before the added fields existed. */
static MotiveSketch pinned_a(void) {
    return four_quarters();
}

static MotiveSketch pinned_b(void) {
    MotiveSketch sketch = four_quarters();
    sketch.minor = true;
    sketch.tonic_pc = 9;
    sketch.accompaniment = true;
    sketch.chords[0].onset = q(0, 1);
    sketch.chords[0].degree = 1;
    sketch.chord_count = 1;
    sketch.features[0].label[0] = 'a';
    sketch.features[0].first = 0;
    sketch.features[0].last = 1;
    sketch.feature_count = 1;
    return sketch;
}

static MotiveSketch pinned_c(void) {
    MotiveSketch sketch = four_quarters();
    sketch.has_scale = false;
    sketch.bar = q(3, 4);
    sketch.pickup = q(1, 4);
    sketch.notes[1].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    return sketch;
}

static void test_pinned_fingerprints(void) {
    MotiveSketch a = pinned_a();
    MotiveSketch b = pinned_b();
    MotiveSketch c = pinned_c();
    assert(motive_sketch_validate(&a) && motive_sketch_validate(&b) && motive_sketch_validate(&c));
    assert(motive_sketch_fingerprint(&a) == 10331613212329969655ULL);
    assert(motive_sketch_fingerprint(&b) == 2919547091505499556ULL);
    assert(motive_sketch_fingerprint(&c) == 16734664634238882527ULL);
}

static void test_validate_new_fields(void) {
    MotiveSketch sketch = four_quarters();
    sketch.form = MOTIVE_SKETCH_FORM_CONTOUR;
    sketch.key_source = MOTIVE_SKETCH_KEY_INFERRED;
    assert(motive_sketch_validate(&sketch));
    sketch.form = (MotiveSketchForm)3;
    assert(!motive_sketch_validate(&sketch));
    sketch.form = (MotiveSketchForm)-1;
    assert(!motive_sketch_validate(&sketch));
    sketch.form = MOTIVE_SKETCH_FORM_RHYTHM;
    sketch.key_source = (MotiveSketchKeySource)3;
    assert(!motive_sketch_validate(&sketch));

    /* The spelling must agree with tonic_pc and needs a scale. */
    sketch = four_quarters();
    sketch.tonic_pc = 1;
    sketch.has_tonic_spelling = true;
    sketch.tonic_letter = PITCH_LETTER_D;
    sketch.tonic_accidental = -1;
    assert(motive_sketch_validate(&sketch));
    sketch.tonic_letter = PITCH_LETTER_C;
    sketch.tonic_accidental = 1;
    assert(motive_sketch_validate(&sketch)); /* C sharp: the same pitch class, another spelling */
    sketch.tonic_accidental = 0;
    assert(!motive_sketch_validate(&sketch)); /* C is pitch class 0 */
    sketch.tonic_letter = 7;
    assert(!motive_sketch_validate(&sketch));
    sketch.tonic_letter = PITCH_LETTER_D;
    sketch.tonic_accidental = 3;
    assert(!motive_sketch_validate(&sketch));
    sketch.tonic_accidental = -1;
    sketch.has_scale = false;
    assert(!motive_sketch_validate(&sketch));

    /* Overrides in range. */
    sketch = four_quarters();
    sketch.tone_overrides[0] = (MotiveToneOverride){1, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_NEIGHBOR};
    sketch.tone_override_count = 1;
    assert(motive_sketch_validate(&sketch));
    sketch.tone_overrides[0].note = 4;
    assert(!motive_sketch_validate(&sketch));
    sketch.tone_overrides[0].note = 1;
    sketch.tone_overrides[0].type = NON_CHORD_TONE_COUNT;
    assert(!motive_sketch_validate(&sketch));
    sketch.tone_overrides[0].role = MOTIVE_TONE_CHORD; /* the type is read only for a non-chord tone */
    assert(motive_sketch_validate(&sketch));
    sketch.tone_overrides[0].role = (MotiveToneRole)3;
    assert(!motive_sketch_validate(&sketch));
    sketch.tone_overrides[0].role = MOTIVE_TONE_CHORD;
    sketch.tone_override_count = MOTIVE_SKETCH_MAX_NOTES + 1;
    assert(!motive_sketch_validate(&sketch));
}

static void test_equal_and_fingerprint(void) {
    MotiveSketch base = four_quarters();
    unsigned long long base_print = motive_sketch_fingerprint(&base);

    MotiveSketch form = base;
    form.form = MOTIVE_SKETCH_FORM_RHYTHM;
    assert(!motive_sketch_equal(&base, &form));
    assert(motive_sketch_fingerprint(&form) != base_print);

    MotiveSketch source = base;
    source.key_source = MOTIVE_SKETCH_KEY_GIVEN;
    assert(!motive_sketch_equal(&base, &source));
    assert(motive_sketch_fingerprint(&source) != base_print);
    /* Without a scale the key source is not read. */
    source.has_scale = false;
    MotiveSketch no_scale = base;
    no_scale.has_scale = false;
    assert(motive_sketch_equal(&source, &no_scale));
    assert(motive_sketch_fingerprint(&source) == motive_sketch_fingerprint(&no_scale));

    MotiveSketch spelled = base;
    spelled.has_tonic_spelling = true;
    spelled.tonic_letter = PITCH_LETTER_C;
    assert(!motive_sketch_equal(&base, &spelled));
    assert(motive_sketch_fingerprint(&spelled) != base_print);
    MotiveSketch respelled = spelled;
    respelled.tonic_letter = PITCH_LETTER_B;
    respelled.tonic_accidental = 1;
    assert(!motive_sketch_equal(&spelled, &respelled));
    assert(motive_sketch_fingerprint(&spelled) != motive_sketch_fingerprint(&respelled));

    MotiveSketch overridden = base;
    overridden.tone_overrides[0] = (MotiveToneOverride){1, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_NEIGHBOR};
    overridden.tone_override_count = 1;
    assert(!motive_sketch_equal(&base, &overridden));
    assert(motive_sketch_fingerprint(&overridden) != base_print);
    MotiveSketch other = overridden;
    other.tone_overrides[0].type = NON_CHORD_TONE_PASSING;
    assert(!motive_sketch_equal(&overridden, &other));
    assert(motive_sketch_fingerprint(&overridden) != motive_sketch_fingerprint(&other));
    other = overridden;
    assert(motive_sketch_equal(&overridden, &other));
    assert(motive_sketch_fingerprint(&overridden) == motive_sketch_fingerprint(&other));
    /* Entries past the count do not count. */
    other.tone_overrides[5].note = 3;
    assert(motive_sketch_equal(&overridden, &other));
    assert(motive_sketch_fingerprint(&overridden) == motive_sketch_fingerprint(&other));
}

static bool labels_equal(const MotiveToneLabel *a, const MotiveToneLabel *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (a[i].role != b[i].role || a[i].type != b[i].type || a[i].resolves_to != b[i].resolves_to ||
            a[i].unresolved != b[i].unresolved || a[i].overridden != b[i].overridden) {
            return false;
        }
    }
    return true;
}

static void test_tone_roles_of(void) {
    MotiveSketch sketch = four_quarters();
    MotiveToneLabel plain[MOTIVE_SKETCH_MAX_NOTES];
    MotiveToneLabel own[MOTIVE_SKETCH_MAX_NOTES];
    MotiveToneLabel given[MOTIVE_SKETCH_MAX_NOTES];

    /* No overrides: the same as the plain call. */
    assert(motive_tone_roles(&sketch, NULL, 0, plain) == MOTIVE_TONE_OK);
    assert(motive_tone_roles_of(&sketch, own) == MOTIVE_TONE_OK);
    assert(labels_equal(plain, own, 4));
    assert(plain[1].role == MOTIVE_TONE_NON_CHORD && plain[1].type == NON_CHORD_TONE_PASSING);

    /* The sketch's overrides give what the same overrides given to motive_tone_roles give. */
    sketch.tone_overrides[0] = (MotiveToneOverride){1, MOTIVE_TONE_NON_CHORD, NON_CHORD_TONE_NEIGHBOR};
    sketch.tone_overrides[1] = (MotiveToneOverride){3, MOTIVE_TONE_UNCLASSIFIED, NON_CHORD_TONE_PASSING};
    sketch.tone_override_count = 2;
    assert(motive_tone_roles_of(&sketch, own) == MOTIVE_TONE_OK);
    assert(motive_tone_roles(&sketch, sketch.tone_overrides, 2, given) == MOTIVE_TONE_OK);
    assert(labels_equal(given, own, 4));
    assert(own[1].overridden && own[1].type == NON_CHORD_TONE_NEIGHBOR);
    assert(own[3].overridden && own[3].role == MOTIVE_TONE_UNCLASSIFIED);
    assert(!own[0].overridden && !own[2].overridden);

    /* Refusals. */
    assert(motive_tone_roles_of(NULL, own) == MOTIVE_TONE_INVALID);
    sketch.tone_overrides[1].note = 9;
    assert(motive_tone_roles_of(&sketch, own) == MOTIVE_TONE_BAD_OVERRIDE);
    sketch.tone_override_count = MOTIVE_SKETCH_MAX_NOTES + 1;
    assert(motive_tone_roles_of(&sketch, own) == MOTIVE_TONE_BAD_OVERRIDE);
    sketch.tone_override_count = 0;
    sketch.has_scale = false;
    assert(motive_tone_roles_of(&sketch, own) == MOTIVE_TONE_NO_SCALE);
}

static void test_bars_without_pickup(void) {
    MotiveSketch sketch = four_quarters();
    assert(motive_sketch_bar_of(&sketch, q(0, 1)) == 0);
    assert(motive_sketch_bar_of(&sketch, q(3, 4)) == 0);
    assert(motive_sketch_bar_of(&sketch, q(1, 1)) == 1);
    assert(motive_sketch_bar_of(&sketch, q(5, 2)) == 2);
    Rational offset = motive_sketch_offset_in_bar(&sketch, 1, q(3, 2));
    assert(msk_eq(offset, q(1, 2)));
    assert(msk_eq(motive_sketch_offset_in_bar(&sketch, 0, q(0, 1)), q(0, 1)));
    assert(!msk_ok(motive_sketch_offset_in_bar(&sketch, 2, q(1, 1)))); /* before that bar */
    assert(!msk_ok(motive_sketch_offset_in_bar(&sketch, -1, q(0, 1)))); /* no pickup bar */
    assert(motive_sketch_on_beat(&sketch, q(0, 1)));
    assert(motive_sketch_on_beat(&sketch, q(1, 4)));
    assert(motive_sketch_on_beat(&sketch, q(5, 4)));
    assert(!motive_sketch_on_beat(&sketch, q(1, 8)));
    assert(!motive_sketch_on_beat(&sketch, q(3, 8)));
    /* A bad time or sketch. */
    assert(motive_sketch_bar_of(&sketch, msk_bad()) == -1);
    assert(motive_sketch_bar_of(NULL, q(0, 1)) == -1);
    assert(!motive_sketch_on_beat(&sketch, msk_bad()));
}

static void test_bars_with_pickup(void) {
    /* 3/4 with a quarter-note pickup: the first downbeat is at 1/4. */
    MotiveSketch sketch = four_quarters();
    sketch.bar = q(3, 4);
    sketch.pickup = q(1, 4);
    sketch.length = q(1, 1);
    assert(motive_sketch_validate(&sketch));
    assert(motive_sketch_bar_of(&sketch, q(0, 1)) == -1);
    assert(motive_sketch_bar_of(&sketch, q(1, 8)) == -1);
    assert(motive_sketch_bar_of(&sketch, q(1, 4)) == 0);
    assert(motive_sketch_bar_of(&sketch, q(3, 4)) == 0);
    assert(motive_sketch_bar_of(&sketch, q(1, 1)) == 1);
    assert(msk_eq(motive_sketch_offset_in_bar(&sketch, -1, q(0, 1)), q(1, 2)));
    assert(msk_eq(motive_sketch_offset_in_bar(&sketch, 0, q(1, 4)), q(0, 1)));
    assert(msk_eq(motive_sketch_offset_in_bar(&sketch, 0, q(1, 2)), q(1, 4)));
    assert(msk_eq(motive_sketch_offset_in_bar(&sketch, 1, q(5, 4)), q(1, 4)));
    assert(!msk_ok(motive_sketch_offset_in_bar(&sketch, -2, q(0, 1))));
    assert(motive_sketch_on_beat(&sketch, q(0, 1)));
    assert(motive_sketch_on_beat(&sketch, q(1, 4)));
    assert(motive_sketch_on_beat(&sketch, q(1, 2)));
    assert(!motive_sketch_on_beat(&sketch, q(1, 8)));
    assert(!motive_sketch_on_beat(&sketch, q(3, 8)));
    /* The same grid as the strength of an onset: the downbeat is strong, other beats are not. */
    assert(motive_sketch_strength(&sketch, q(1, 4)) == 2);
    assert(motive_sketch_strength(&sketch, q(1, 2)) == 1);
}

int main(void) {
    test_pinned_fingerprints();
    test_validate_new_fields();
    test_equal_and_fingerprint();
    test_tone_roles_of();
    test_bars_without_pickup();
    test_bars_with_pickup();
    printf("motive sketch value tests passed\n");
    return 0;
}
