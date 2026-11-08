#include "score_test_util.h"

#include "export/score_clefs.h"
#include "export/score_quantize.h"

/* score_clefs: the clef of each staff, measure by measure, from the ledger lines the notes need. Measures are 4/4 (the
   defaulted map) so a whole note fills one measure. Costs used below: a whole note is 4 quarters, a change of clef is
   SCORE_CLEF_CHANGE_COST (4) quarters of one ledger line. */

typedef struct {
    TestBag bag;
    size_t next_id;
} Piece;

static void piece_free(Piece *piece) {
    bag_free(&piece->bag);
}

/* A note at pos / dur (whole notes) on a staff of part 0. */
static void add(Piece *piece, long pos_n, unsigned long pos_d, long dur_n, unsigned long dur_d, PitchLetter letter,
                int octave, int staff) {
    char id[32];
    snprintf(id, sizeof(id), "n%zu", piece->next_id++);
    bag_add(&piece->bag, tu_note(id, pos_n, pos_d, dur_n, dur_d, letter, 0, octave, NULL), 0, staff);
}

/* A whole note filling measure m (1-based). */
static void whole(Piece *piece, long m, PitchLetter letter, int octave, int staff) {
    add(piece, m - 1, 1, 1, 1, letter, octave, staff);
}

static void infer(const Piece *piece, const int *part_staff_count, size_t part_count, ScoreClefChanges *out) {
    assert(score_clefs_infer(piece->bag.events, NULL, piece->bag.count, piece->bag.part, piece->bag.staff,
                             part_staff_count, part_count, NULL, NULL, out) == SCORE_CLEFS_OK);
}

static void expect(const ScoreClefChanges *changes, size_t index, size_t part, int staff, size_t measure,
                   ScoreClef clef) {
    assert(index < changes->count);
    assert(changes->part[index] == part);
    assert(changes->staff[index] == staff);
    assert(changes->measure[index] == measure);
    assert(changes->clef[index] == clef);
}

static void test_two_staves_start_treble_and_bass(void) {
    Piece piece = {0};
    for (long m = 1; m <= 3; m++) {
        whole(&piece, m, PITCH_LETTER_G, 4, 1);
        whole(&piece, m, PITCH_LETTER_C, 3, 2);
    }
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 2);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_left_hand_rises_and_returns(void) {
    /* Staff 2: a bar of C3, two bars of D4 to A4 (12 ledger line quarters a bar under bass against 0 under treble, a
       change costs 4 each way), then C3 again. The right hand stays in treble. */
    Piece piece = {0};
    for (long m = 1; m <= 4; m++) {
        whole(&piece, m, PITCH_LETTER_E, 5, 1);
    }
    whole(&piece, 1, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 1, 1, 4, PITCH_LETTER_D, 4, 2);
    add(&piece, 5, 4, 1, 4, PITCH_LETTER_F, 4, 2);
    add(&piece, 3, 2, 1, 4, PITCH_LETTER_A, 4, 2);
    add(&piece, 7, 4, 1, 4, PITCH_LETTER_G, 4, 2);
    whole(&piece, 3, PITCH_LETTER_A, 4, 2);
    whole(&piece, 4, PITCH_LETTER_C, 3, 2);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 4);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    expect(&changes, 2, 0, 2, 2, SCORE_CLEF_TREBLE);
    expect(&changes, 3, 0, 2, 4, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_single_high_note_does_not_switch(void) {
    /* Three quarters of C3 and one A4 in a bass bar: bass costs 3 (the A4), treble costs 12 (the C3s). */
    Piece piece = {0};
    add(&piece, 0, 1, 1, 4, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 4, 1, 4, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 2, 1, 4, PITCH_LETTER_C, 3, 2);
    add(&piece, 3, 4, 1, 4, PITCH_LETTER_A, 4, 2);
    whole(&piece, 2, PITCH_LETTER_C, 3, 2);
    whole(&piece, 1, PITCH_LETTER_C, 5, 1);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 2);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_a_short_visit_is_not_worth_a_change(void) {
    /* Half a bar of A4 in a bar of C3: bass costs 6 ledger line quarters, treble costs 8 for the C3, so bass stays. */
    Piece piece = {0};
    add(&piece, 0, 1, 1, 2, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 2, 1, 2, PITCH_LETTER_A, 4, 2);
    whole(&piece, 1, PITCH_LETTER_C, 5, 1);
    whole(&piece, 2, PITCH_LETTER_C, 3, 2);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 2);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_empty_measures_keep_the_clef(void) {
    /* Staff 2: C3, then two empty bars, then A4 for two bars. The change is written at measure 4, the first measure
       with notes, and never at the empty measures 2 and 3. */
    Piece piece = {0};
    whole(&piece, 1, PITCH_LETTER_C, 3, 2);
    whole(&piece, 4, PITCH_LETTER_A, 4, 2);
    whole(&piece, 5, PITCH_LETTER_A, 4, 2);
    whole(&piece, 1, PITCH_LETTER_C, 5, 1);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 3);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    expect(&changes, 2, 0, 2, 4, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);

    /* A one-staff part with a gap: bass notes around two empty bars stay in bass, one entry. */
    Piece gap = {0};
    whole(&gap, 1, PITCH_LETTER_C, 2, 1);
    whole(&gap, 4, PITCH_LETTER_C, 2, 1);
    const int one[1] = {1};
    infer(&gap, one, 1, &changes);
    assert(changes.count == 1);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&gap);
}

static void test_one_staff_part_starts_in_either_clef(void) {
    Piece low = {0};
    whole(&low, 1, PITCH_LETTER_C, 2, 1);
    whole(&low, 2, PITCH_LETTER_E, 2, 1);
    const int one[1] = {1};
    ScoreClefChanges changes;
    infer(&low, one, 1, &changes);
    assert(changes.count == 1);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&low);

    Piece high = {0};
    whole(&high, 1, PITCH_LETTER_C, 6, 1);
    infer(&high, one, 1, &changes);
    assert(changes.count == 1);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);
    piece_free(&high);

    /* A first bar of middle C costs one ledger line in either clef: a tie is treble. */
    Piece middle = {0};
    whole(&middle, 1, PITCH_LETTER_C, 4, 1);
    infer(&middle, one, 1, &changes);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);
    piece_free(&middle);
}

static void test_ties_keep_the_current_clef(void) {
    /* Staff 2 starts in bass; middle C costs one ledger line in both clefs, so it stays in bass. Staff 1 stays in
       treble the same way. */
    Piece piece = {0};
    whole(&piece, 1, PITCH_LETTER_C, 4, 1);
    whole(&piece, 1, PITCH_LETTER_C, 4, 2);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 2);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_ledger_lines_by_position(void) {
    /* The staff 1 clef of a lone whole note, to see which clef each pitch is cheaper in. D4 sits just under the treble
       staff (no ledger line), C4 and B3 take one, A3 two; in bass B3 sits just above the staff, C4 and D4 take one. */
    struct {
        PitchLetter letter;
        int octave;
        int staff;
        ScoreClef expected;
    } cases[] = {
        {PITCH_LETTER_D, 4, 1, SCORE_CLEF_TREBLE}, {PITCH_LETTER_A, 3, 1, SCORE_CLEF_BASS},
        {PITCH_LETTER_B, 3, 2, SCORE_CLEF_BASS},   {PITCH_LETTER_G, 5, 2, SCORE_CLEF_TREBLE},
        {PITCH_LETTER_A, 5, 2, SCORE_CLEF_TREBLE}, {PITCH_LETTER_E, 2, 1, SCORE_CLEF_BASS},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Piece piece = {0};
        whole(&piece, 1, cases[i].letter, cases[i].octave, cases[i].staff);
        const int counts[1] = {2};
        ScoreClefChanges changes;
        infer(&piece, counts, 1, &changes);
        size_t entry = (size_t)cases[i].staff - 1;
        assert(changes.clef[entry] == cases[i].expected);
        score_clef_changes_destroy(&changes);
        piece_free(&piece);
    }
}

static void test_a_note_across_a_barline_counts_in_each_measure(void) {
    /* Staff 2: a whole note of A4 from the middle of measure 1 to the middle of measure 2 is half a bar in each (6
       ledger line quarters under bass in each bar); with C3 filling the rest of both bars, treble would cost 8 + 8
       for the C3s against 6 + 6 under bass, so bass stays. */
    Piece piece = {0};
    add(&piece, 0, 1, 1, 2, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 2, 1, 1, PITCH_LETTER_A, 4, 2);
    add(&piece, 3, 2, 1, 2, PITCH_LETTER_C, 3, 2);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    assert(changes.count == 2);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_parts_and_order(void) {
    /* Two parts: a one-staff part with high notes and a two-staff part whose staff 2 rises at measure 2. The measure 1
       entries come first (part, staff), then the changes in measure order. */
    Piece piece = {0};
    whole(&piece, 1, PITCH_LETTER_C, 3, 2);
    whole(&piece, 2, PITCH_LETTER_A, 4, 2);
    whole(&piece, 3, PITCH_LETTER_A, 4, 2);
    whole(&piece, 1, PITCH_LETTER_C, 5, 1);
    for (size_t i = 0; i < piece.bag.count; i++) {
        piece.bag.part[i] = 1;
    }
    add(&piece, 0, 1, 1, 1, PITCH_LETTER_C, 6, 1);
    piece.bag.part[piece.bag.count - 1] = 0;
    const int counts[2] = {1, 2};
    ScoreClefChanges changes;
    infer(&piece, counts, 2, &changes);
    assert(changes.count == 4);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 1, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 2, 1, 2, 1, SCORE_CLEF_BASS);
    expect(&changes, 3, 1, 2, 2, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_notated_times_are_used(void) {
    /* One C2 on staff 1 of a grand staff part, played in measure 1. Notated in measure 3 the first measure keeps the
       treble clef and the bass clef is written at measure 3. */
    Piece piece = {0};
    whole(&piece, 1, PITCH_LETTER_C, 2, 1);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    infer(&piece, counts, 1, &changes);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);

    unsigned long long start_num[1] = {2}, start_den[1] = {1}, end_num[1] = {3}, end_den[1] = {1};
    ScoreQuantized notated = {1, start_num, start_den, end_num, end_den};
    assert(score_clefs_infer(piece.bag.events, NULL, piece.bag.count, piece.bag.part, piece.bag.staff, counts, 1, NULL,
                             &notated, &changes) == SCORE_CLEFS_OK);
    assert(changes.count == 3);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 0, 2, 1, SCORE_CLEF_BASS);
    expect(&changes, 2, 0, 1, 3, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
}

static void test_no_notes_and_no_parts(void) {
    ScoreClefChanges changes;
    assert(score_clefs_infer(NULL, NULL, 0, NULL, NULL, NULL, 0, NULL, NULL, &changes) == SCORE_CLEFS_OK);
    assert(changes.count == 1);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);

    const int counts[2] = {1, 2};
    assert(score_clefs_infer(NULL, NULL, 0, NULL, NULL, counts, 2, NULL, NULL, &changes) == SCORE_CLEFS_OK);
    assert(changes.count == 3);
    expect(&changes, 0, 0, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 1, 1, 1, 1, SCORE_CLEF_TREBLE);
    expect(&changes, 2, 1, 2, 1, SCORE_CLEF_BASS);
    score_clef_changes_destroy(&changes);
    score_clef_changes_destroy(&changes);
    score_clef_changes_destroy(NULL);
}

static void test_a_pickup_measure_is_measure_one(void) {
    /* A 1/4 pickup then 4/4: the pickup is measure 1 and the first full bar measure 2. */
    TuMeter meter[2] = {{1, 4, 0, 1}, {4, 4, 1, 4}};
    MeterMap *map = tu_meter_map(meter, 2);
    Piece piece = {0};
    add(&piece, 0, 1, 1, 4, PITCH_LETTER_C, 3, 2);
    add(&piece, 1, 4, 1, 1, PITCH_LETTER_A, 4, 2);
    add(&piece, 5, 4, 1, 1, PITCH_LETTER_A, 4, 2);
    const int counts[1] = {2};
    ScoreClefChanges changes;
    assert(score_clefs_infer(piece.bag.events, NULL, piece.bag.count, piece.bag.part, piece.bag.staff, counts, 1, map,
                             NULL, &changes) == SCORE_CLEFS_OK);
    assert(changes.count == 3);
    expect(&changes, 2, 0, 2, 2, SCORE_CLEF_TREBLE);
    score_clef_changes_destroy(&changes);
    piece_free(&piece);
    meter_map_destroy(map);
}

static void test_errors(void) {
    Piece piece = {0};
    whole(&piece, 1, PITCH_LETTER_C, 4, 1);
    const int counts[1] = {1};
    ScoreClefChanges changes;
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, counts, 1, NULL, NULL, NULL) ==
           SCORE_CLEFS_ERR_NULL_ARG);
    assert(score_clefs_infer(NULL, NULL, 1, NULL, NULL, counts, 1, NULL, NULL, &changes) == SCORE_CLEFS_ERR_NULL_ARG);
    assert(changes.count == 0 && changes.part == NULL);
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, NULL, 1, NULL, NULL,
                             &changes) == SCORE_CLEFS_ERR_NULL_ARG);

    piece.bag.staff[0] = 2; /* the part has one staff */
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, counts, 1, NULL, NULL,
                             &changes) == SCORE_CLEFS_ERR_INCONSISTENT_INPUT);
    assert(changes.count == 0 && changes.clef == NULL);
    piece.bag.staff[0] = 1;
    piece.bag.part[0] = 1; /* there is one part */
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, counts, 1, NULL, NULL,
                             &changes) == SCORE_CLEFS_ERR_INCONSISTENT_INPUT);
    piece.bag.part[0] = 0;
    const int bad_counts[1] = {0};
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, bad_counts, 1, NULL, NULL,
                             &changes) == SCORE_CLEFS_ERR_INCONSISTENT_INPUT);
    PitchNumericValue *const missing[1] = {NULL};
    assert(score_clefs_infer(piece.bag.events, missing, 1, piece.bag.part, piece.bag.staff, counts, 1, NULL, NULL,
                             &changes) == SCORE_CLEFS_ERR_INCONSISTENT_INPUT);
    ScoreQuantized wrong = {2, NULL, NULL, NULL, NULL};
    assert(score_clefs_infer(piece.bag.events, NULL, 1, piece.bag.part, piece.bag.staff, counts, 1, NULL, &wrong,
                             &changes) == SCORE_CLEFS_ERR_INCONSISTENT_INPUT);
    piece_free(&piece);
}

int main(void) {
    test_two_staves_start_treble_and_bass();
    test_left_hand_rises_and_returns();
    test_single_high_note_does_not_switch();
    test_a_short_visit_is_not_worth_a_change();
    test_empty_measures_keep_the_clef();
    test_one_staff_part_starts_in_either_clef();
    test_ties_keep_the_current_clef();
    test_ledger_lines_by_position();
    test_a_note_across_a_barline_counts_in_each_measure();
    test_parts_and_order();
    test_notated_times_are_used();
    test_no_notes_and_no_parts();
    test_a_pickup_measure_is_measure_one();
    test_errors();
    printf("score_clefs: all tests passed\n");
    return 0;
}
