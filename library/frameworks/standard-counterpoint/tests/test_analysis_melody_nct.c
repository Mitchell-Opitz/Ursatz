#include <math.h>

#include "analysis_melody_nct.h"
#include "melody_test_util.h"

/* The L2.5 result of each melody note, attached by reference: never re-derived, with the unclassified fallback and the
   per-span counts by type. */

static void test_each_note_carries_its_result_and_reference(void) {
    const int pitches[] = {67, 69, 71, 72}; /* G A B C */
    TestMelody m;
    test_melody_init(&m, pitches, 4);
    m.notes[0].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    m.notes[1].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[1].nct_type = NON_CHORD_TONE_PASSING;
    m.notes[1].nct_ref = "seg-4/m1";
    /* notes[2]: no result at all: the zero value is UNCLASSIFIED */
    m.notes[3].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    m.notes[3].nct_ref = "seg-6/m3";
    m.notes[3].confidence = 0.5;
    test_melody_build(&m);
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels(&m.line, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(labels.count == 4);
    StandardCounterpointNctClaim claim;
    assert(standard_counterpoint_nct_claim_read(labels.items[1].interpretation, &claim));
    assert(claim.status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE && claim.type == (int)NON_CHORD_TONE_PASSING);
    assert(claim.has_ref == 1 && strcmp(claim.ref, "seg-4/m1") == 0);
    assert(structure_event_id_count(labels.items[1].shell) == 1);
    assert(strcmp(entity_id_value(interpretation_target_id(labels.items[1].interpretation)),
                  entity_id_value(entity_id(structure_entity(labels.items[1].shell)))) == 0);
    assert(standard_counterpoint_nct_claim_read(labels.items[0].interpretation, &claim));
    assert(claim.status == (int)STANDARD_COUNTERPOINT_NCT_CHORD_TONE && claim.type == -1 && claim.has_ref == 0);
    assert(standard_counterpoint_nct_claim_read(labels.items[2].interpretation, &claim));
    assert(claim.status == (int)STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED && claim.type == -1 && claim.has_ref == 0);
    assert(fabs(labels.items[3].uncertainty - 0.5) < 1e-12);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(labels.items[3].interpretation)) == 500000UL);
    standard_counterpoint_nct_labels_destroy(&labels);
    test_melody_destroy(&m);
}

static void test_rests_have_no_label(void) {
    const int pitches[] = {67, 0, 69};
    TestMelody m;
    test_melody_make(&m, pitches, 3);
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels(&m.line, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(labels.count == 2);
    StandardCounterpointNctClaim claim;
    assert(standard_counterpoint_nct_claim_read(labels.items[1].interpretation, &claim) && claim.index == 2);
    standard_counterpoint_nct_labels_destroy(&labels);
    test_melody_destroy(&m);
}

static void test_counts_by_type_per_span(void) {
    const int pitches[] = {60, 62, 64, 65, 67, 69, 0, 71};
    TestMelody m;
    test_melody_init(&m, pitches, 8);
    m.notes[0].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    m.notes[1].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[1].nct_type = NON_CHORD_TONE_PASSING;
    m.notes[2].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    m.notes[3].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[3].nct_type = NON_CHORD_TONE_NEIGHBOR;
    m.notes[4].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[4].nct_type = NON_CHORD_TONE_PASSING;
    /* notes[5], [7]: unclassified */
    test_melody_build(&m);
    const StandardCounterpointMelodySpan spans[] = {{"a", 0, 3}, {"b", 4, 7}};
    StandardCounterpointNctCountList counts;
    assert(standard_counterpoint_melody_nct_counts(&m.line, spans, 2, &counts) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(counts.count == 2);
    const StandardCounterpointNctCountsClaim *a = &counts.items[0].claim;
    assert(a->note_count == 4 && a->chord_tones == 2 && a->non_chord_tones == 2 && a->unclassified == 0);
    assert(a->by_type[NON_CHORD_TONE_PASSING] == 1 && a->by_type[NON_CHORD_TONE_NEIGHBOR] == 1);
    const StandardCounterpointNctCountsClaim *b = &counts.items[1].claim;
    assert(b->note_count == 3 && b->chord_tones == 0 && b->non_chord_tones == 1 && b->unclassified == 2);
    assert(b->by_type[NON_CHORD_TONE_PASSING] == 1);
    StandardCounterpointNctCountsClaim read;
    assert(standard_counterpoint_nct_counts_claim_read(counts.items[0].interpretation, &read) && read.chord_tones == 2);
    standard_counterpoint_nct_count_list_destroy(&counts);
    assert(standard_counterpoint_melody_nct_counts(&m.line, NULL, 0, &counts) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(counts.count == 1 && counts.items[0].claim.note_count == 7);
    standard_counterpoint_nct_count_list_destroy(&counts);
    test_melody_destroy(&m);
}

static void test_a_bad_result_is_refused(void) {
    StandardCounterpointMelodyLine line;
    standard_counterpoint_melody_line_init(&line);
    const int pitches[] = {60};
    TestMelody m;
    test_melody_init(&m, pitches, 1);
    m.notes[0].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[0].nct_type = (NonChordTone)99;
    assert(standard_counterpoint_melody_line_add_note(&line, &m.notes[0]) == STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE);
    m.notes[0].nct = (StandardCounterpointNctStatus)7;
    assert(standard_counterpoint_melody_line_add_note(&line, &m.notes[0]) == STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE);
    m.notes[0].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    char longest[200];
    memset(longest, 'x', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    m.notes[0].nct_ref = longest;
    assert(standard_counterpoint_melody_line_add_note(&line, &m.notes[0]) == STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE);
    standard_counterpoint_melody_line_destroy(&line);
    test_melody_destroy(&m);
}

int main(void) {
    test_each_note_carries_its_result_and_reference();
    test_rests_have_no_label();
    test_counts_by_type_per_span();
    test_a_bad_result_is_refused();
    printf("test_standard_counterpoint_analysis_melody_nct: ok\n");
    return 0;
}
