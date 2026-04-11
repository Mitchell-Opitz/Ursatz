#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/plan_state.h>

#include "check/shaping.h"
#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"

/*
 * The shaping preferences (R-164), each on hand-written melodies and progressions that hold and that fail. The melody
 * is one bar of C major over a C chord and a C bass unless a case builds its own; MELODY_TICKS to a whole note.
 */

#define QUARTER 32L

static MelodyCandidate *candidate(long length) {
    MelodyCandidate *melody = calloc(1, sizeof(*melody));
    assert(melody);
    PlanTime whole;
    PlanTime quarter;
    assert(plan_time_make(1, 1, &whole) == PLAN_OK && plan_time_make(1, 4, &quarter) == PLAN_OK);
    MelodyContext *context = &melody->context;
    context->key.tonic_pitch_class = 0;
    context->key.mode = PLAN_MODE_MAJOR;
    context->metre.bar_length = whole;
    context->metre.cell_length = quarter;
    context->metre.cells_per_bar = 4;
    context->length = length;
    context->cell = QUARTER;
    context->count = 1;
    context->chords[0].chord.degree = 1;
    context->chord_mask[0] = (1u << 0) | (1u << 4) | (1u << 7);
    context->chord_start[1] = length;
    context->bass[0].end = length;
    context->bass[0].pitch = 48;
    context->bass_count = 1;
    melody->params = melody_fill_params_default();
    melody->magic = MELODY_PAYLOAD_MAGIC;
    return melody;
}

static void add(MelodyCandidate *melody, long onset, long duration, int pitch) {
    MelodyNote *note = &melody->notes[melody->count++];
    note->onset = onset;
    note->duration = duration;
    note->pitch = pitch;
    note->motive_index = -1;
    note->strong = onset % QUARTER == 0;
}

/* Quarter notes from the start, one pitch each. */
static MelodyCandidate *quarters(const int *pitches, size_t count) {
    MelodyCandidate *melody = candidate((long)count * QUARTER);
    for (size_t i = 0; i < count; i++) {
        add(melody, (long)i * QUARTER, QUARTER, pitches[i]);
    }
    return melody;
}

static bool broken(const MelodyCandidate *melody, unsigned rule) {
    return (melody_shaping_broken(melody) & rule) != 0;
}

/* An idea that ends the phrase in a cadence, after two bars of the phrase with four notes in them. */
static MelodyCandidate *ending(void) {
    MelodyCandidate *melody = candidate(4 * QUARTER);
    melody->context.idea = true;
    melody->context.has_cadence = true;
    melody->context.phrase_start = -8 * QUARTER;
    melody->context.phrase_length = 12 * QUARTER;
    melody->context.phrase_before_notes = 4;
    return melody;
}

static void test_ending_reduction(void) {
    /* Four attacks in a bar against four in two: busier than the phrase before it. */
    MelodyCandidate *busy = ending();
    for (int i = 0; i < 4; i++) {
        add(busy, i * QUARTER, QUARTER, 60);
    }
    assert(broken(busy, SHAPING_ENDING_REDUCTION));
    free(busy);

    /* Two attacks, the last the longest. */
    MelodyCandidate *reduced = ending();
    add(reduced, 0, QUARTER, 64);
    add(reduced, QUARTER, 3 * QUARTER, 60);
    assert(!broken(reduced, SHAPING_ENDING_REDUCTION));
    free(reduced);

    /* Two attacks, but the last is the shorter. */
    MelodyCandidate *short_end = ending();
    add(short_end, 0, 3 * QUARTER, 64);
    add(short_end, 3 * QUARTER, QUARTER, 60);
    assert(broken(short_end, SHAPING_ENDING_REDUCTION));
    free(short_end);

    /* An idea that does not end the phrase is not asked. */
    MelodyCandidate *middle = ending();
    middle->context.has_cadence = false;
    for (int i = 0; i < 4; i++) {
        add(middle, i * QUARTER, QUARTER, 60);
    }
    assert(!broken(middle, SHAPING_ENDING_REDUCTION));
    free(middle);
}

static void test_waves(void) {
    const int waves[8] = {60, 64, 62, 66, 63, 67, 62, 60};
    const int one_peak[8] = {60, 62, 64, 66, 65, 63, 62, 61};
    const int climb[8] = {60, 62, 64, 65, 67, 69, 71, 72};
    const int few[6] = {60, 62, 64, 66, 65, 63};
    MelodyCandidate *melody = quarters(waves, 8);
    assert(!broken(melody, SHAPING_WAVES));
    free(melody);
    melody = quarters(one_peak, 8);
    assert(broken(melody, SHAPING_WAVES));
    free(melody);
    melody = quarters(climb, 8);
    assert(broken(melody, SHAPING_WAVES));
    free(melody);
    /* Under eight notes the rule is not asked. */
    melody = quarters(few, 6);
    assert(!broken(melody, SHAPING_WAVES));
    free(melody);
}

static void test_central_range(void) {
    const int central[8] = {60, 62, 64, 60, 62, 64, 60, 66};
    const int outliers[8] = {60, 60, 60, 60, 60, 73, 73, 73};
    const int one_outlier[8] = {60, 62, 64, 60, 62, 64, 60, 73};
    MelodyCandidate *melody = quarters(central, 8);
    assert(!broken(melody, SHAPING_CENTRAL_RANGE));
    free(melody);
    /* 3 of the 8 notes lie outside the median plus or minus 6. */
    melody = quarters(outliers, 8);
    assert(broken(melody, SHAPING_CENTRAL_RANGE));
    free(melody);
    /* 1 of 8 is within the quarter allowed. */
    melody = quarters(one_outlier, 8);
    assert(!broken(melody, SHAPING_CENTRAL_RANGE));
    free(melody);
}

static void test_monotony(void) {
    const int same[12] = {60, 62, 64, 62, 60, 62, 64, 62, 60, 62, 64, 62};
    /* The same bar moved up a step has the same intervals and rhythm. */
    const int moved[12] = {60, 62, 64, 62, 62, 64, 66, 64, 64, 66, 68, 66};
    const int third_differs[12] = {60, 62, 64, 62, 60, 62, 64, 62, 60, 64, 67, 64};
    const int two_bars[8] = {60, 62, 64, 62, 60, 62, 64, 62};
    MelodyCandidate *melody = quarters(same, 12);
    assert(broken(melody, SHAPING_MONOTONY));
    free(melody);
    melody = quarters(moved, 12);
    assert(broken(melody, SHAPING_MONOTONY));
    free(melody);
    melody = quarters(third_differs, 12);
    assert(!broken(melody, SHAPING_MONOTONY));
    free(melody);
    /* Two equal bars are not three. */
    melody = quarters(two_bars, 8);
    assert(!broken(melody, SHAPING_MONOTONY));
    free(melody);
    /* Three bars of the same intervals with another rhythm in the middle. */
    melody = candidate(12 * QUARTER);
    for (int bar = 0; bar < 3; bar++) {
        long at = bar * 4 * QUARTER;
        if (bar == 1) {
            add(melody, at, 2 * QUARTER, 60);
            add(melody, at + 2 * QUARTER, QUARTER, 62);
            add(melody, at + 3 * QUARTER, QUARTER, 64);
        } else {
            add(melody, at, QUARTER, 60);
            add(melody, at + QUARTER, QUARTER, 62);
            add(melody, at + 2 * QUARTER, 2 * QUARTER, 64);
        }
    }
    assert(!broken(melody, SHAPING_MONOTONY));
    free(melody);
}

static void test_root_progressions(void) {
    HarmonyChord i = {1, 0, false, false, false, false};
    HarmonyChord iv = {4, 0, false, false, false, false};
    HarmonyChord v = {5, 0, false, false, false, false};
    const HarmonyChord weak[4] = {i, v, i, v};
    const HarmonyChord strong[4] = {i, iv, v, i};
    const HarmonyChord one_change[3] = {i, i, v};
    const HarmonyChord repeated[4] = {i, i, iv, iv};
    assert(!harmony_root_progression_holds(weak, 4));
    assert(harmony_root_progression_holds(strong, 4));
    /* Fewer than two root changes always hold. */
    assert(harmony_root_progression_holds(one_change, 3));
    assert(harmony_root_progression_holds(repeated, 4));
}

/* The scores the checker gave before the shaping rules, for the one bar C E G E over the C chord and bass. */
#define PLAIN_NUMERATOR 13
#define PLAIN_DENOMINATOR 17

static void test_plain_shaping(void) {
    const int arpeggio[4] = {60, 64, 67, 64};
    MelodyCandidate *melody = quarters(arpeggio, 4);
    FillCheck shaped;
    FillCheck plain;
    assert(melody_check(melody, &shaped) && !shaped.hard_violation);
    melody->params.plain_shaping = true;
    assert(melody_check(melody, &plain) && !plain.hard_violation);
    assert(plain.score_numerator == PLAIN_NUMERATOR && plain.score_denominator == PLAIN_DENOMINATOR);
    /* Four rules more, all held. */
    assert(shaped.score_denominator == plain.score_denominator + 4);
    assert(shaped.score_numerator == plain.score_numerator + 4);
    free(melody);
}

int main(void) {
    test_ending_reduction();
    test_waves();
    test_central_range();
    test_monotony();
    test_root_progressions();
    test_plain_shaping();
    printf("shaping ok\n");
    return 0;
}
