#include <math.h>
#include <string.h>

#include "analysis_test_util.h"
#include "meter_analysis_syncopation.h"
#include "interpretation/interpretation.h"

static StandardMeterSyncopations find(Piece *piece, const StandardMeterConfig *config) {
    StandardMeterSyncopations found = {NULL, 0};
    assert(standard_meter_find_syncopations(&piece->input, config, &found) == STANDARD_METER_ANALYSIS_OK);
    return found;
}

static void test_a_tie_across_a_barline(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 0, 1, 3, 4);  /* beats 1 to 3, then ... */
    piece_note(&piece, 0, 3, 4, 5, 4);  /* beat 4 tied over the barline into beat 1 of measure 2 */
    piece_note(&piece, 0, 5, 4, 3, 2);
    StandardMeterSyncopations found = find(&piece, NULL);
    assert(found.count == 1);
    const StandardMeterSyncopationClaim *claim = &found.items[0].claim;
    assert(strcmp(claim->note_id, "n1") == 0 && claim->measure == 1 && fabs(claim->beat - 4.0) < 1e-12);
    assert(claim->attack_level == (int)STANDARD_METER_LEVEL_BEAT && claim->through_level == (int)STANDARD_METER_LEVEL_DOWNBEAT);
    assert(claim->strength == 2 && claim->through_measure == 2 && fabs(claim->through_beat - 1.0) < 1e-12);
    assert(found.items[0].interpretation != NULL && found.items[0].uncertainty == 0.0);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);
}

static void test_an_and_held_over_the_beat(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 0, 1, 1, 8);
    piece_note(&piece, 0, 1, 8, 3, 8); /* the "and" of one, held over beat 2 */
    piece_note(&piece, 0, 3, 8, 1, 2);
    StandardMeterSyncopations found = find(&piece, NULL);
    assert(found.count == 1);
    const StandardMeterSyncopationClaim *claim = &found.items[0].claim;
    assert(claim->attack_level == (int)STANDARD_METER_LEVEL_OFFBEAT && claim->through_level == (int)STANDARD_METER_LEVEL_BEAT);
    assert(claim->strength == 1 && fabs(claim->beat - 1.5) < 1e-12 && fabs(claim->through_beat - 2.0) < 1e-12);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);
}

static void test_no_syncopation_when_the_stronger_position_is_attacked(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    /* the same long "and", but the line attacks beat 2 as well: the first note does not sound through it */
    piece_note(&piece, 0, 1, 8, 3, 8);
    piece_note(&piece, 0, 1, 4, 1, 2);
    StandardMeterSyncopations found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);

    /* notes on the beats, and a note held from a downbeat, are not syncopated */
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 0, 1, 1, 2);
    piece_note(&piece, 0, 1, 2, 1, 1);
    piece_note(&piece, 0, 1, 1, 2, 1);
    found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);
}

static void test_streams_per_line_and_together(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 1, 8, 3, 8); /* the upper line holds an "and" over beat 2 ... */
    piece_note(&piece, 1, 1, 4, 1, 2); /* ... where the lower line attacks */
    StandardMeterSyncopations per_line = find(&piece, NULL);
    assert(per_line.count == 1 && per_line.items[0].claim.line == 0);
    StandardMeterConfig together = standard_meter_config_default();
    together.syncopation_all_notes = true;
    StandardMeterSyncopations all = find(&piece, &together);
    assert(all.count == 0);
    standard_meter_syncopations_destroy(&per_line);
    standard_meter_syncopations_destroy(&all);
    piece_destroy(&piece);
}

static void test_a_half_note_on_beat_two_and_a_snapped_time(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 1, 4, 3, 4); /* beat 2 held through beat 3, the secondary beat */
    StandardMeterSyncopations found = find(&piece, NULL);
    assert(found.count == 1 && found.items[0].claim.strength == 1 &&
           found.items[0].claim.through_level == (int)STANDARD_METER_LEVEL_SECONDARY_BEAT);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);

    /* a performed "and" a little late and a little long is read on the grid, with a small uncertainty */
    piece_init(&piece, 4, 2);
    piece_note(&piece, 0, 33, 256, 97, 256);
    found = find(&piece, NULL);
    assert(found.count == 1 && fabs(found.items[0].uncertainty - 0.1) < 1e-12);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);
}

static void test_bad_calls(void) {
    StandardMeterSyncopations found = {NULL, 0};
    assert(standard_meter_find_syncopations(NULL, NULL, &found) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    Piece piece;
    piece_init(&piece, 4, 2);
    assert(standard_meter_find_syncopations(&piece.input, NULL, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardMeterConfig bad = standard_meter_config_default();
    bad.hemiola_min_evidence = 0;
    assert(standard_meter_find_syncopations(&piece.input, &bad, &found) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_add_note(&piece.input, "", piece_span(&piece, 0, 1, 1, 4), 60, 0) ==
           STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_add_note(&piece.input, "x", NULL, 60, 0) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    found = find(&piece, NULL);
    assert(found.count == 0 && found.items == NULL);
    standard_meter_syncopations_destroy(&found);
    piece_destroy(&piece);
}

int main(void) {
    test_a_tie_across_a_barline();
    test_an_and_held_over_the_beat();
    test_no_syncopation_when_the_stronger_position_is_attacked();
    test_streams_per_line_and_together();
    test_a_half_note_on_beat_two_and_a_snapped_time();
    test_bad_calls();
    printf("test_analysis_syncopation (standard-meter): ok\n");
    return 0;
}
