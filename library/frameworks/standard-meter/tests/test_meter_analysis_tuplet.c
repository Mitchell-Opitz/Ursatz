#include <math.h>
#include <string.h>

#include "analysis_test_util.h"
#include "interpretation/interpretation.h"
#include "meter_analysis_tuplet.h"

/* A piece plus the notes handed to the finder. */
typedef struct {
    Piece piece;
    AnalysisNote notes[64];
    char ids[64][16];
    bool in_line[64];
    size_t count;
} Fixture;

static void fixture_init(Fixture *f, unsigned top, int unit_exponent) {
    memset(f, 0, sizeof(*f));
    piece_init(&f->piece, top, unit_exponent);
}

static void fixture_note(Fixture *f, long sn, unsigned long sd, long en, unsigned long ed) {
    AnalysisNote *note = &f->notes[f->count];
    snprintf(f->ids[f->count], sizeof(f->ids[f->count]), "t%zu", f->count);
    memset(note, 0, sizeof(*note));
    note->note_id = f->ids[f->count];
    note->span = piece_span(&f->piece, sn, sd, en, ed);
    note->midi = 60;
    note->track = ANALYSIS_INDEX_NONE;
    note->voice = ANALYSIS_INDEX_NONE;
    note->voice_confidence = 1.0;
    f->in_line[f->count] = true;
    f->count++;
}

static AnalysisNoteSet fixture_set(const Fixture *f) {
    AnalysisNoteSet set = {f->notes, f->count, f->piece.map};
    return set;
}

static StandardMeterTuplets find(const Fixture *f, const bool *in_line) {
    AnalysisNoteSet set = fixture_set(f);
    StandardMeterTuplets found = {NULL, 0};
    assert(standard_meter_find_tuplets(&set, in_line, &found) == STANDARD_METER_ANALYSIS_OK);
    return found;
}

static void fixture_destroy(Fixture *f) {
    piece_destroy(&f->piece);
}

/* Three eighth triplets on beat one of 4/4, then a quarter on beat two. */
static void test_eighth_triplets_in_four_four(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    fixture_note(&f, 0, 1, 1, 12);
    fixture_note(&f, 1, 12, 1, 6);
    fixture_note(&f, 1, 6, 1, 4);
    fixture_note(&f, 1, 4, 1, 2);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 1);
    const StandardMeterTuplet *t = &found.items[0];
    assert(t->actual == 3 && t->normal == 2 && strcmp(t->name, "triplet") == 0);
    assert(t->attacks == 3 && t->note_count == 3);
    assert(t->notes[0] == 0 && t->notes[1] == 1 && t->notes[2] == 2);
    assert(t->measure == 1 && fabs(t->beat - 1.0) < 1e-12);
    assert(t->meter == METER_TYPE_SIMPLE && t->borrowed); /* a triplet borrows the compound division */
    assert(t->has_level && t->level == TUPLET_LEVEL_BEAT && t->level_typical);
    standard_meter_tuplets_destroy(&found);
    assert(found.items == NULL && found.count == 0);
    fixture_destroy(&f);
}

/* Six sixteenth sextuplets fill beat two: one run of six attacks. */
static void test_sixteenth_sextuplets(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    for (long k = 0; k < 6; k++) {
        fixture_note(&f, 6 + k, 24, 7 + k, 24); /* from 1/4 in steps of 1/24 */
    }
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 1);
    const StandardMeterTuplet *t = &found.items[0];
    assert(t->actual == 3 && t->normal == 2 && t->attacks == 6 && t->note_count == 6);
    assert(t->measure == 1 && fabs(t->beat - 2.0) < 1e-12);
    /* the level is that of the first group of three (an eighth of a whole note, half a beat) */
    assert(t->has_level && t->level == TUPLET_LEVEL_SUBDIVISION);
    standard_meter_tuplets_destroy(&found);
    fixture_destroy(&f);
}

/* In 6/8 a dotted-quarter beat halved in two equal notes is a duplet. */
static void test_duplet_in_six_eight(void) {
    Fixture f;
    fixture_init(&f, 6, 3);
    fixture_note(&f, 0, 1, 3, 16);
    fixture_note(&f, 3, 16, 3, 8);
    fixture_note(&f, 3, 8, 3, 4);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 1);
    const StandardMeterTuplet *t = &found.items[0];
    assert(t->actual == 2 && t->normal == 3 && strcmp(t->name, "duplet") == 0);
    assert(t->attacks == 2 && t->note_count == 2 && t->notes[0] == 0 && t->notes[1] == 1);
    assert(t->measure == 1 && fabs(t->beat - 1.0) < 1e-12);
    assert(t->meter == METER_TYPE_COMPOUND && t->borrowed);
    standard_meter_tuplets_destroy(&found);
    fixture_destroy(&f);
}

/* A dotted figure (a long note and a short one) in 6/8 is not a duplet; nor is it any tuplet. */
static void test_dotted_figure_is_not_a_duplet(void) {
    Fixture f;
    fixture_init(&f, 6, 3);
    fixture_note(&f, 0, 1, 9, 32);
    fixture_note(&f, 9, 32, 3, 8);
    fixture_note(&f, 3, 8, 3, 4);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 0 && found.items == NULL);
    fixture_destroy(&f);
}

/* Onsets further than a quarter of a step from the 96 grid (a performance's rubato) are read as nothing. */
static void test_rubato_reads_as_nothing(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    fixture_note(&f, 0, 1, 1, 10);
    fixture_note(&f, 1, 10, 3, 20);
    fixture_note(&f, 3, 20, 9, 50);
    fixture_note(&f, 9, 50, 1, 4);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 0);
    fixture_destroy(&f);
}

/* One attack is no group. */
static void test_a_single_attack_is_dropped(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    fixture_note(&f, 1, 12, 1, 4); /* off the 1/32 grid, but the next attack, on it, is not a triplet's */
    fixture_note(&f, 1, 2, 3, 4);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 0);
    fixture_destroy(&f);
}

/* `in_line` skips notes: a run with its notes out is gone, and one left with a single attack is dropped. */
static void test_in_line_skips_notes(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    fixture_note(&f, 0, 1, 1, 12);
    fixture_note(&f, 1, 12, 1, 6);
    fixture_note(&f, 1, 6, 1, 4);
    fixture_note(&f, 1, 4, 1, 2);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 1);
    standard_meter_tuplets_destroy(&found);

    bool none[4] = {false, false, false, true};
    found = find(&f, none);
    assert(found.count == 0);

    bool one[4] = {true, false, false, true};
    found = find(&f, one);
    assert(found.count == 0);

    bool two[4] = {true, true, false, true};
    found = find(&f, two);
    assert(found.count == 1 && found.items[0].attacks == 2 && found.items[0].note_count == 2);
    standard_meter_tuplets_destroy(&found);
    fixture_destroy(&f);
}

/* Smaller `actual` first: a duplet in one bar sorts before a triplet in an earlier one. */
static void test_order(void) {
    Fixture f;
    fixture_init(&f, 6, 3);
    fixture_note(&f, 0, 1, 1, 12);
    fixture_note(&f, 1, 12, 1, 6);
    fixture_note(&f, 1, 6, 1, 4);
    fixture_note(&f, 3, 4, 15, 16);
    fixture_note(&f, 15, 16, 9, 8);
    fixture_note(&f, 9, 8, 3, 2);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 2);
    assert(found.items[0].actual == 2 && found.items[0].measure == 2);
    assert(found.items[1].actual == 3 && found.items[1].measure == 1);
    standard_meter_tuplets_destroy(&found);
    fixture_destroy(&f);
}

static void test_claim(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    fixture_note(&f, 0, 1, 1, 12);
    fixture_note(&f, 1, 12, 1, 6);
    fixture_note(&f, 1, 6, 1, 4);
    fixture_note(&f, 1, 4, 1, 2);
    AnalysisNoteSet set = fixture_set(&f);
    StandardMeterTuplets found = find(&f, NULL);
    assert(found.count == 1);
    Structure *shell = NULL;
    Interpretation *interpretation = NULL;
    assert(standard_meter_tuplet_emit(&set, &found.items[0], "m1-b1", &shell, &interpretation));
    StandardMeterTupletClaim claim;
    assert(standard_meter_tuplet_claim_read(interpretation, &claim));
    assert(claim.actual == 3 && claim.normal == 2 && claim.attacks == 3 && claim.note_count == 3);
    assert(claim.measure == 1 && claim.meter == METER_TYPE_SIMPLE && claim.has_level);
    assert(!standard_meter_tuplet_claim_read(NULL, &claim));
    assert(!standard_meter_tuplet_emit(&set, &found.items[0], NULL, &shell, &interpretation));
    interpretation_destroy(interpretation);
    structure_destroy(shell);
    standard_meter_tuplets_destroy(&found);
    fixture_destroy(&f);
}

static void test_bad_arguments(void) {
    Fixture f;
    fixture_init(&f, 4, 2);
    AnalysisNoteSet set = fixture_set(&f);
    StandardMeterTuplets found = {NULL, 0};
    assert(standard_meter_find_tuplets(NULL, NULL, &found) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_find_tuplets(&set, NULL, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_find_tuplets(&set, NULL, &found) == STANDARD_METER_ANALYSIS_OK && found.count == 0);
    standard_meter_tuplets_destroy(NULL);
    fixture_destroy(&f);
}

int main(void) {
    test_eighth_triplets_in_four_four();
    test_sixteenth_sextuplets();
    test_duplet_in_six_eight();
    test_dotted_figure_is_not_a_duplet();
    test_rubato_reads_as_nothing();
    test_a_single_attack_is_dropped();
    test_in_line_skips_notes();
    test_order();
    test_claim();
    test_bad_arguments();
    printf("test_meter_analysis_tuplet: ok\n");
    return 0;
}
