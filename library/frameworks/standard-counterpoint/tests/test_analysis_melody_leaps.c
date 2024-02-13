#include <math.h>

#include "analysis_melody_climax.h"
#include "analysis_melody_leaps.h"
#include "melody_test_util.h"

/* Leap recovery, consecutive leaps and the climax of a span: the existing cantus firmus and melodic rules' definitions,
   read back as Interpretations. */

typedef struct {
    TestMelody m;
    StandardCounterpointMelodicIntervals intervals;
} Fixture;

static void fixture_make(Fixture *f, const int *pitches, size_t count) {
    test_melody_make(&f->m, pitches, count);
    assert(standard_counterpoint_melody_intervals(&f->m.line, NULL, &f->intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
}

static void fixture_destroy(Fixture *f) {
    standard_counterpoint_melodic_intervals_destroy(&f->intervals);
    test_melody_destroy(&f->m);
}

static int status_of_first(const Fixture *f) {
    StandardCounterpointLeapRecoveries recoveries;
    assert(standard_counterpoint_melody_leap_recoveries(&f->m.line, &f->intervals, &recoveries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(recoveries.count >= 1);
    int status = recoveries.items[0].claim.status;
    standard_counterpoint_leap_recoveries_destroy(&recoveries);
    return status;
}

static void test_a_leap_then_a_step_down_is_recovered(void) {
    const int pitches[] = {60, 67, 65}; /* C up a fifth to G, down a step to F */
    Fixture f;
    fixture_make(&f, pitches, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_RECOVERED);
    StandardCounterpointLeapRecoveries recoveries;
    assert(standard_counterpoint_melody_leap_recoveries(&f.m.line, &f.intervals, &recoveries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(recoveries.count == 1 && recoveries.items[0].claim.next_index == 2);
    assert(structure_event_id_count(recoveries.items[0].shell) == 3);
    assert(strcmp(entity_id_value(interpretation_target_id(recoveries.items[0].interpretation)),
                  entity_id_value(entity_id(structure_entity(recoveries.items[0].shell)))) == 0);
    StandardCounterpointLeapRecoveryClaim read;
    assert(standard_counterpoint_leap_recovery_claim_read(recoveries.items[0].interpretation, &read) &&
           read.status == (int)STANDARD_COUNTERPOINT_LEAP_RECOVERED);
    standard_counterpoint_leap_recoveries_destroy(&recoveries);
    fixture_destroy(&f);
}

static void test_a_leap_then_a_step_up_is_unrecovered(void) {
    const int pitches[] = {60, 67, 69};
    Fixture f;
    fixture_make(&f, pitches, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED);
    fixture_destroy(&f);
}

static void test_a_leap_then_a_repeated_note_or_a_leap_is_unrecovered(void) {
    const int repeated[] = {60, 67, 67};
    Fixture f;
    fixture_make(&f, repeated, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED);
    fixture_destroy(&f);
    const int leaped[] = {60, 67, 62};
    fixture_make(&f, leaped, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED); /* a step is required, not a leap back */
    fixture_destroy(&f);
}

static void test_a_third_needs_no_recovery_and_a_final_leap_is_not_judged(void) {
    const int third[] = {60, 64, 66};
    Fixture f;
    fixture_make(&f, third, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED);
    fixture_destroy(&f);
    const int last[] = {60, 62, 69}; /* the last interval is a fifth with nothing after it */
    fixture_make(&f, last, 3);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_NOT_JUDGED);
    fixture_destroy(&f);
    const int rest[] = {60, 67, 0, 65}; /* a rest after the leap breaks the chain */
    fixture_make(&f, rest, 4);
    assert(status_of_first(&f) == (int)STANDARD_COUNTERPOINT_LEAP_NOT_JUDGED);
    fixture_destroy(&f);
}

static void test_a_stepwise_line_has_no_leap_claims(void) {
    const int scale[] = {60, 62, 64, 65, 67};
    Fixture f;
    fixture_make(&f, scale, 5);
    StandardCounterpointLeapRecoveries recoveries;
    assert(standard_counterpoint_melody_leap_recoveries(&f.m.line, &f.intervals, &recoveries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(recoveries.count == 0 && recoveries.items == NULL);
    StandardCounterpointConsecutiveLeapList leaps;
    assert(standard_counterpoint_melody_consecutive_leaps(&f.m.line, &f.intervals, &leaps) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(leaps.count == 0);
    fixture_destroy(&f);
}

static void test_two_leaps_in_the_same_direction_are_flagged(void) {
    const int pitches[] = {60, 65, 70}; /* a fourth up, a fourth up */
    Fixture f;
    fixture_make(&f, pitches, 3);
    StandardCounterpointConsecutiveLeapList leaps;
    assert(standard_counterpoint_melody_consecutive_leaps(&f.m.line, &f.intervals, &leaps) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(leaps.count == 1 && leaps.items[0].claim.same_direction == 1 && leaps.items[0].claim.three_in_a_row == 0);
    assert(leaps.items[0].claim.first_from == 0 && leaps.items[0].claim.middle == 1 && leaps.items[0].claim.last_to == 2);
    StandardCounterpointConsecutiveLeapsClaim read;
    assert(standard_counterpoint_consecutive_leaps_claim_read(leaps.items[0].interpretation, &read) &&
           read.same_direction == 1);
    standard_counterpoint_consecutive_leap_list_destroy(&leaps);
    fixture_destroy(&f);
}

static void test_leaps_in_opposite_directions_and_a_triad_are_not_flagged(void) {
    const int opposite[] = {60, 67, 62}; /* up a fifth, down a fifth */
    Fixture f;
    fixture_make(&f, opposite, 3);
    StandardCounterpointConsecutiveLeapList leaps;
    assert(standard_counterpoint_melody_consecutive_leaps(&f.m.line, &f.intervals, &leaps) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(leaps.count == 0);
    fixture_destroy(&f);
    const int triad[] = {60, 64, 67}; /* two thirds up outlining a perfect fifth */
    fixture_make(&f, triad, 3);
    assert(standard_counterpoint_melody_consecutive_leaps(&f.m.line, &f.intervals, &leaps) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(leaps.count == 0);
    fixture_destroy(&f);
}

static void test_three_leaps_in_a_row_are_flagged(void) {
    const int pitches[] = {60, 67, 62, 69}; /* up, down, up: all leaps, alternating */
    Fixture f;
    fixture_make(&f, pitches, 4);
    StandardCounterpointConsecutiveLeapList leaps;
    assert(standard_counterpoint_melody_consecutive_leaps(&f.m.line, &f.intervals, &leaps) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(leaps.count == 1 && leaps.items[0].claim.three_in_a_row == 1 && leaps.items[0].claim.same_direction == 0);
    standard_counterpoint_consecutive_leap_list_destroy(&leaps);
    fixture_destroy(&f);
}

static void test_a_unique_climax_and_its_position(void) {
    const int pitches[] = {60, 62, 69, 67, 65, 64}; /* the top, A4, is reached by a leap, the fourth note of six... */
    Fixture f;
    fixture_make(&f, pitches, 6);
    f.m.notes[2].has_strength = true;
    f.m.notes[2].strength = BEAT_STRENGTH_STRONG;
    test_melody_build(&f.m);
    standard_counterpoint_melodic_intervals_destroy(&f.intervals);
    assert(standard_counterpoint_melody_intervals(&f.m.line, NULL, &f.intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointClimaxes climaxes;
    assert(standard_counterpoint_melody_climaxes(&f.m.line, &f.intervals, NULL, 0, &climaxes) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(climaxes.count == 1);
    const StandardCounterpointClimaxClaim *c = &climaxes.items[0].claim;
    assert(c->pitch == 69 && c->first_index == 2 && c->unique == 1 && c->occurrence_count == 1);
    assert(fabs(c->relative_position - 2.0 / 5.0) < 1e-12);
    assert(c->has_strength == 1 && c->strength == (int)BEAT_STRENGTH_STRONG);
    assert(c->by_leap == 1 && c->final_note == 0);
    assert(structure_event_id_count(climaxes.items[0].shell) == 1);
    StandardCounterpointClimaxClaim read;
    assert(standard_counterpoint_climax_claim_read(climaxes.items[0].interpretation, &read) && read.pitch == 69);
    standard_counterpoint_climaxes_destroy(&climaxes);
    fixture_destroy(&f);
}

static void test_a_repeated_top_note_is_listed_not_collapsed(void) {
    const int pitches[] = {60, 67, 64, 67, 62, 67};
    Fixture f;
    fixture_make(&f, pitches, 6);
    StandardCounterpointClimaxes climaxes;
    assert(standard_counterpoint_melody_climaxes(&f.m.line, &f.intervals, NULL, 0, &climaxes) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    const StandardCounterpointClimaxClaim *c = &climaxes.items[0].claim;
    assert(c->unique == 0 && c->occurrence_count == 3 && c->first_index == 1);
    assert(c->occurrences[0] == 1 && c->occurrences[1] == 3 && c->occurrences[2] == 5);
    assert(c->final_note == 0); /* the FIRST occurrence is what the rules call the climax */
    assert(structure_event_id_count(climaxes.items[0].shell) == 3);
    standard_counterpoint_climaxes_destroy(&climaxes);
    fixture_destroy(&f);
}

static void test_the_climax_is_per_span_and_skips_rests(void) {
    const int pitches[] = {60, 72, 0, 64, 67, 65};
    Fixture f;
    fixture_make(&f, pitches, 6);
    const StandardCounterpointMelodySpan spans[] = {{"first", 0, 2}, {"second", 3, 5}, {"rest", 2, 2}};
    StandardCounterpointClimaxes climaxes;
    assert(standard_counterpoint_melody_climaxes(&f.m.line, &f.intervals, spans, 3, &climaxes) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(climaxes.count == 2);
    assert(climaxes.items[0].claim.pitch == 72 && climaxes.items[0].claim.first_index == 1 &&
           climaxes.items[0].claim.final_note == 1); /* the last NOTE of the span; the rest does not count */
    assert(fabs(climaxes.items[0].claim.relative_position - 1.0) < 1e-12);
    assert(climaxes.items[1].claim.pitch == 67 && climaxes.items[1].claim.first_index == 4 &&
           climaxes.items[1].claim.by_leap == 1); /* approached from 64 by a third inside the span: a leap */
    standard_counterpoint_climaxes_destroy(&climaxes);
    fixture_destroy(&f);
}

int main(void) {
    test_a_leap_then_a_step_down_is_recovered();
    test_a_leap_then_a_step_up_is_unrecovered();
    test_a_leap_then_a_repeated_note_or_a_leap_is_unrecovered();
    test_a_third_needs_no_recovery_and_a_final_leap_is_not_judged();
    test_a_stepwise_line_has_no_leap_claims();
    test_two_leaps_in_the_same_direction_are_flagged();
    test_leaps_in_opposite_directions_and_a_triad_are_not_flagged();
    test_three_leaps_in_a_row_are_flagged();
    test_a_unique_climax_and_its_position();
    test_a_repeated_top_note_is_listed_not_collapsed();
    test_the_climax_is_per_span_and_skips_rests();
    printf("test_standard_counterpoint_analysis_melody_leaps: ok\n");
    return 0;
}
