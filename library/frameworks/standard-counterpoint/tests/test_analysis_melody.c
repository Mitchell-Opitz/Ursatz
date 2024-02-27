#include <math.h>

#include "analysis_melody.h"
#include "core/framework_reference.h"
#include "melody_test_util.h"

/* Melodic intervals and contour of one line: directions and sizes the adapter computes, rests that break the chain, the
   generic size from a spelling or a scale degree, and the contour summary of a span. */

static StandardCounterpointMelodicIntervals intervals_of(const TestMelody *m) {
    StandardCounterpointMelodicIntervals intervals;
    assert(standard_counterpoint_melody_intervals(&m->line, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    return intervals;
}

static void test_a_stepwise_scale_has_no_leaps_and_rises(void) {
    const int scale[] = {60, 62, 64, 65, 67, 69, 71, 72}; /* C major, C4 to C5 */
    TestMelody m;
    test_melody_make(&m, scale, 8);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.count == 7);
    for (size_t i = 0; i < intervals.count; i++) {
        assert(intervals.items[i].claim.direction == (int)CONTOUR_DIRECTION_UP);
        assert(intervals.items[i].claim.step == 1 && intervals.items[i].claim.leap == 0);
        assert(intervals.items[i].claim.semitones > 0 && intervals.items[i].claim.semitones <= 2);
        assert(contour_direction_at(intervals.items[i].contour, 0) == CONTOUR_DIRECTION_UP);
        assert(contour_pitch_id_count(intervals.items[i].contour) == 2);
        assert(interval_has_directed_semitones(intervals.items[i].interval) &&
               interval_directed_semitones(intervals.items[i].interval) == intervals.items[i].claim.semitones);
    }
    StandardCounterpointContourSummaries summaries;
    assert(standard_counterpoint_melody_contour_summaries(&m.line, &intervals, NULL, 0, &summaries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(summaries.count == 1);
    const StandardCounterpointContourSummaryClaim *c = &summaries.items[0].claim;
    assert(c->note_count == 8 && c->up == 7 && c->down == 0 && c->same == 0 && c->step_count == 7 && c->leap_count == 0);
    assert(c->net_direction == (int)CONTOUR_DIRECTION_UP && c->net_semitones == 12 && c->range_semitones == 12);
    assert(c->lowest_index == 0 && c->highest_index == 7);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_the_interpretation_follows_the_claim_pattern(void) {
    const int pitches[] = {60, 67};
    TestMelody m;
    test_melody_make(&m, pitches, 2);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.count == 1);
    const StandardCounterpointMelodicInterval *item = &intervals.items[0];
    assert(structure_event_id_count(item->shell) == 2);
    assert(strcmp(entity_id_value(interpretation_target_id(item->interpretation)),
                  entity_id_value(entity_id(structure_entity(item->shell)))) == 0);
    assert(interpretation_framework_reference(item->interpretation) != NULL);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(item->interpretation)) == 750000UL); /* approximate (no spelling or degree): the default penalty applies */
    StandardCounterpointMelodicIntervalClaim claim;
    assert(standard_counterpoint_melodic_interval_claim_read(item->interpretation, &claim));
    assert(claim.from == 0 && claim.to == 1 && claim.semitones == 7 && claim.leap == 1 && claim.step == 0);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_a_rest_breaks_the_chain(void) {
    const int pitches[] = {60, 62, 0, 64, 65, 0, 0, 67};
    TestMelody m;
    test_melody_make(&m, pitches, 8);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.count == 2); /* 60-62 and 64-65 only */
    assert(intervals.items[0].claim.from == 0 && intervals.items[1].claim.from == 3);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_down_and_same_directions_and_the_summary(void) {
    const int pitches[] = {67, 67, 64, 65, 62, 60};
    TestMelody m;
    test_melody_make(&m, pitches, 6);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.items[0].claim.direction == (int)CONTOUR_DIRECTION_SAME && intervals.items[0].claim.semitones == 0);
    assert(intervals.items[1].claim.direction == (int)CONTOUR_DIRECTION_DOWN);
    StandardCounterpointContourSummaries summaries;
    assert(standard_counterpoint_melody_contour_summaries(&m.line, &intervals, NULL, 0, &summaries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    const StandardCounterpointContourSummaryClaim *c = &summaries.items[0].claim;
    assert(c->up == 1 && c->down == 3 && c->same == 1);
    assert(c->net_direction == (int)CONTOUR_DIRECTION_DOWN && c->net_semitones == -7 && c->range_semitones == 7);
    assert(c->highest_index == 0 && c->lowest_index == 5);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_generic_size_from_spellings(void) {
    /* 61 spelled D-flat 4 against C4 is a minor second (a step) */
    const int pitches[] = {60, 61, 63};
    TestMelody m;
    test_melody_init(&m, pitches, 3);
    PitchSpelling *c4 = NULL;
    PitchSpelling *d_flat = NULL;
    PitchSpelling *e_flat = NULL; /* 63 spelled E-flat 4 */
    assert(pitch_spelling_create(PITCH_LETTER_C, 0, 4, &c4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_D, -1, 4, &d_flat) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_E, -1, 4, &e_flat) == PITCH_SPELLING_OK);
    m.notes[0].spelling = c4;
    m.notes[1].spelling = d_flat;
    m.notes[2].spelling = e_flat;
    test_melody_build(&m);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.items[0].claim.generic_size == 2 && intervals.items[0].claim.step == 1);
    assert(intervals.items[0].claim.generic_source == (int)STANDARD_COUNTERPOINT_GENERIC_SPELLING);
    assert(intervals.items[0].claim.approximate == 0);
    assert(intervals.items[1].claim.generic_size == 2); /* D-flat to E-flat */
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(d_flat);
    pitch_spelling_destroy(e_flat);
}

static void test_generic_size_from_scale_degrees(void) {
    /* degrees of C major: 1 to 3 is a third; 5 up to 4 across the octave is a seventh... 7 to 1 up is a second;
       1 down to 3 an octave below is a thirteenth */
    const int pitches[] = {60, 64, 71, 72, 52};
    const int degrees[] = {1, 3, 7, 1, 3};
    TestMelody m;
    test_melody_init(&m, pitches, 5);
    for (size_t i = 0; i < 5; i++) {
        m.notes[i].degree = degrees[i];
    }
    test_melody_build(&m);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.items[0].claim.generic_size == 3 && intervals.items[0].claim.leap == 1);
    assert(intervals.items[1].claim.generic_size == 5); /* E up to B: a fifth */
    assert(intervals.items[2].claim.generic_size == 2 && intervals.items[2].claim.step == 1);
    assert(intervals.items[3].claim.generic_size == 13); /* C5 down to E3: a sixth and an octave, a thirteenth */
    for (size_t i = 0; i < intervals.count; i++) {
        assert(intervals.items[i].claim.generic_source == (int)STANDARD_COUNTERPOINT_GENERIC_DEGREE);
        assert(intervals.items[i].claim.approximate == 0);
    }
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_no_spelling_and_no_degree_is_approximate(void) {
    const int pitches[] = {60, 62, 65, 72};
    TestMelody m;
    test_melody_make(&m, pitches, 4);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(intervals.items[0].claim.generic_size == 0 && intervals.items[0].claim.approximate == 1);
    assert(intervals.items[0].claim.step == 1 && intervals.items[1].claim.leap == 1 && intervals.items[2].claim.leap == 1);
    assert(fabs(intervals.items[0].uncertainty - 0.25) < 1e-12); /* the default penalty at full confidence */
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    StandardCounterpointMelodyConfig none = {0.0};
    assert(standard_counterpoint_melody_intervals(&m.line, &none, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(fabs(intervals.items[0].uncertainty) < 1e-12);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_uncertainty_follows_the_least_confidence(void) {
    const int pitches[] = {60, 62};
    TestMelody m;
    test_melody_init(&m, pitches, 2);
    m.notes[0].confidence = 0.6;
    m.notes[0].degree = 1;
    m.notes[1].degree = 2;
    test_melody_build(&m);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    assert(fabs(intervals.items[0].uncertainty - 0.4) < 1e-12);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(intervals.items[0].interpretation)) ==
           600000UL);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_spans_are_caller_supplied(void) {
    const int pitches[] = {60, 64, 62, 67, 65, 72, 0, 59};
    TestMelody m;
    test_melody_make(&m, pitches, 8);
    StandardCounterpointMelodicIntervals intervals = intervals_of(&m);
    const StandardCounterpointMelodySpan spans[] = {{"a", 0, 3}, {"b", 4, 5}, {"rests", 6, 6}, {"c", 6, 7}};
    StandardCounterpointContourSummaries summaries;
    assert(standard_counterpoint_melody_contour_summaries(&m.line, &intervals, spans, 4, &summaries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(summaries.count == 3); /* the span of only a rest has no summary */
    assert(summaries.items[0].claim.up == 2 && summaries.items[0].claim.down == 1 && summaries.items[0].claim.leap_count == 2);
    assert(summaries.items[0].claim.range_semitones == 7 && summaries.items[0].claim.highest_index == 3);
    assert(summaries.items[1].claim.up == 1 && summaries.items[1].claim.note_count == 2);
    assert(summaries.items[2].claim.note_count == 1 && summaries.items[2].claim.up + summaries.items[2].claim.down == 0);
    StandardCounterpointContourSummaryClaim read;
    assert(standard_counterpoint_contour_summary_claim_read(summaries.items[0].interpretation, &read) && read.up == 2);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    const StandardCounterpointMelodySpan bad[] = {{"x", 3, 2}};
    assert(standard_counterpoint_melody_contour_summaries(&m.line, &intervals, bad, 1, &summaries) ==
           STANDARD_COUNTERPOINT_MELODY_ERR_BAD_SPAN);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    test_melody_destroy(&m);
}

static void test_bad_input_is_refused(void) {
    StandardCounterpointMelodyLine line;
    standard_counterpoint_melody_line_init(&line);
    StandardCounterpointMelodyNote note;
    memset(&note, 0, sizeof(note));
    note.present = true;
    note.note_id = "x";
    note.confidence = 1.0;
    assert(standard_counterpoint_melody_line_add_note(&line, &note) == STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE); /* no pitch */
    assert(standard_counterpoint_melody_line_add_note(NULL, &note) == STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT);
    StandardCounterpointMelodicIntervals intervals;
    assert(standard_counterpoint_melody_intervals(NULL, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT);
    assert(standard_counterpoint_melody_intervals(&line, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(intervals.count == 0 && intervals.items == NULL);
    standard_counterpoint_melody_line_destroy(&line);
}

int main(void) {
    test_a_stepwise_scale_has_no_leaps_and_rises();
    test_the_interpretation_follows_the_claim_pattern();
    test_a_rest_breaks_the_chain();
    test_down_and_same_directions_and_the_summary();
    test_generic_size_from_spellings();
    test_generic_size_from_scale_degrees();
    test_no_spelling_and_no_degree_is_approximate();
    test_uncertainty_follows_the_least_confidence();
    test_spans_are_caller_supplied();
    test_bad_input_is_refused();
    printf("test_standard_counterpoint_analysis_melody: ok\n");
    return 0;
}
