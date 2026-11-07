#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

#include "grow/grow_internal.h"
#include "motive/motive_fidelity.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * motive_fidelity (B1 of P37, R-354): the 6/8 Test Motif against motives made from it: the score, the heard chain in
 * words, and whether the chain agrees with the ref that asked for the motive.
 */

static Motive load_motive(void) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

static PlanMotiveRef ref_of(MotiveRelationKind kind, long a, long b) {
    PlanMotiveRef ref;
    memset(&ref, 0, sizeof(ref));
    ref.has_step = true;
    ref.step.kind = kind;
    ref.step.value[0] = a;
    ref.step.value[1] = b;
    return ref;
}

static MotiveFidelity measure(const Motive *source, const Motive *realized, const PlanMotiveRef *ref, bool from_segment) {
    MotiveFidelity fidelity;
    memset(&fidelity, 0, sizeof(fidelity));
    assert(motive_fidelity(source, realized, ref, from_segment, &fidelity));
    return fidelity;
}

static bool is_one(MotiveScore score) {
    return score.numerator == score.denominator;
}

static void show(const char *what, const MotiveFidelity *fidelity) {
    fprintf(stderr, "%s: score %lu/%lu, found %d, agrees %d, heard \"%s\"\n", what, fidelity->score.numerator,
            fidelity->score.denominator, fidelity->found, fidelity->agrees, fidelity->heard);
}

static void expect_agrees(const char *what, const MotiveFidelity *fidelity, bool agrees) {
    if (fidelity->agrees != agrees) {
        show(what, fidelity);
        fprintf(stderr, "%s: expected agrees %d\n", what, agrees);
    }
    assert(fidelity->agrees == agrees);
}

static void expect_measured(const char *what, const Motive *source, const Motive *realized, const PlanMotiveRef *ref,
                            bool from_segment, bool agrees) {
    MotiveFidelity fidelity = measure(source, realized, ref, from_segment);
    expect_agrees(what, &fidelity, agrees);
}

int main(void) {
    Motive source = load_motive();
    assert(source.note_count == 10);
    PlanMotiveRef none;
    memset(&none, 0, sizeof(none));
    PlanMotiveRef exact = ref_of(MOTIVE_RELATION_EXACT, 0, 0);

    /* Itself: score 1, heard "exact", agrees with an exact ref and with a ref that has no step. */
    MotiveFidelity fidelity = measure(&source, &source, &exact, false);
    if (!is_one(fidelity.score) || !fidelity.found || strcmp(fidelity.heard, "exact") != 0) {
        show("itself", &fidelity);
    }
    assert(is_one(fidelity.score) && fidelity.found && strcmp(fidelity.heard, "exact") == 0);
    expect_agrees("itself, exact ref", &fidelity, true);
    expect_measured("itself, no step", &source, &source, &none, false, true);

    /* An octave up is not heard: "exact", and it agrees. */
    Motive octave = source;
    for (size_t n = 0; n < octave.note_count; n++) {
        octave.notes[n].pitch += 12;
    }
    fidelity = measure(&source, &octave, &exact, false);
    assert(fidelity.found && strcmp(fidelity.heard, "exact") == 0);
    expect_agrees("octave up", &fidelity, true);

    /* A tonal transposition by 1 is heard as one transposition: it agrees with a transpose-by-1 ref, not with an exact one. */
    PlanMotiveRef up = ref_of(MOTIVE_RELATION_TRANSPOSE, 1, 1);
    PlanMotiveRef up_two = ref_of(MOTIVE_RELATION_TRANSPOSE, 1, 2);
    Motive transposed;
    assert(grow_develop_motive(&source, &up, &transposed));
    fidelity = measure(&source, &transposed, &up, false);
    assert(fidelity.found && strcmp(fidelity.heard, "exact") != 0 && strstr(fidelity.heard, ", then ") == NULL);
    expect_agrees("transposed by 1, transpose ref", &fidelity, true);

    /* A segment against itself, from_segment, with a fragment ref: the fragment step is not heard again. */
    assert(source.feature_count >= 1);
    Motive segment;
    assert(grow_segment_motive(&source, &source.features[0], &segment));
    PlanMotiveRef fragment = ref_of(MOTIVE_RELATION_FRAGMENT, MOTIVE_FRAGMENT_LEADING, 5);
    fidelity = measure(&segment, &segment, &fragment, true);
    assert(is_one(fidelity.score) && strcmp(fidelity.heard, "exact") == 0);
    expect_agrees("segment, fragment ref", &fidelity, true);

    /* The same transposition of the segment agrees only with its own ref: not an exact one, not another step. */
    Motive bare = segment;
    bare.chord_count = 0;
    Motive segment_up;
    assert(grow_develop_motive(&bare, &up, &segment_up));
    expect_measured("segment transposed by 1, transpose ref", &bare, &segment_up, &up, false, true);
    expect_measured("segment transposed by 1, exact ref", &bare, &segment_up, &exact, false, false);
    expect_measured("segment transposed by 1, transpose by 2 ref", &bare, &segment_up, &up_two, false, false);

    /* A two-step liquidation by features agrees with a ref of two steps. */
    PlanMotiveRef liquidation = ref_of(MOTIVE_RELATION_LIQUIDATE, 0, 0);
    liquidation.steps = 2;
    Motive liquidated;
    assert(grow_develop_motive(&source, &liquidation, &liquidated));
    fidelity = measure(&source, &liquidated, &liquidation, false);
    assert(fidelity.found);
    expect_agrees("liquidated in two steps", &fidelity, true);

    /* An inversion agrees with an invert ref. */
    PlanMotiveRef invert = ref_of(MOTIVE_RELATION_INVERT, -1, 0);
    Motive inverted;
    assert(grow_develop_motive(&source, &invert, &inverted));
    fidelity = measure(&source, &inverted, &invert, false);
    assert(fidelity.found);
    expect_agrees("inverted", &fidelity, true);
    expect_measured("inverted, exact ref", &source, &inverted, &exact, false, false);

    /* A motive of unrelated pitches and rhythm scores under 1/2 and, when not found, is "not recognized". */
    Motive other = source;
    static const int PITCHES[] = {60, 61, 63, 66};
    static const long ONSETS[] = {0, 5, 7, 12};
    static const long DURATIONS[] = {5, 2, 5, 3};
    other.note_count = 4;
    other.feature_count = 0;
    for (size_t n = 0; n < other.note_count; n++) {
        other.notes[n].pitch = PITCHES[n];
        other.notes[n].onset = rational_ratio(ONSETS[n], 16);
        other.notes[n].duration = rational_ratio(DURATIONS[n], 16);
    }
    other.length = rational_ratio(15, 16);
    fidelity = measure(&segment, &other, &exact, false);
    assert(fidelity.score.numerator * 2 < fidelity.score.denominator);
    if (!fidelity.found) {
        assert(strcmp(fidelity.heard, "not recognized") == 0);
    }
    expect_agrees("unrelated", &fidelity, false);

    /* NULL arguments and a failed measure leave the result alone. */
    MotiveFidelity untouched;
    memset(&untouched, 0x5a, sizeof(untouched));
    assert(!motive_fidelity(NULL, &source, &exact, false, &untouched));
    assert(!motive_fidelity(&source, NULL, &exact, false, &untouched));
    assert(!motive_fidelity(&source, &source, NULL, false, &untouched));
    assert(!motive_fidelity(&source, &source, &exact, false, NULL));
    assert(untouched.agrees == (bool)0x5a || untouched.heard[0] == 0x5a);
    return 0;
}
