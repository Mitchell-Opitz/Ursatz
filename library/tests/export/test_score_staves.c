#include "score_test_util.h"

#include "export/score_staves.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"

/* score_staves: the grand staff program table and the treble / bass assignment of a two-staff part. */

typedef struct {
    TestBag bag;
    TuningSystem *tuning;
    PitchNumericValue **values;
} Piece;

static void piece_init(Piece *piece) {
    memset(piece, 0, sizeof(*piece));
    assert(tuning_system_create(tu_id("midi"), "MIDI note number", &piece->tuning) == TUNING_SYSTEM_OK);
}

/* A note at pos / dur (whole-note fractions) with a MIDI pitch, in the given part. */
static void piece_add(Piece *piece, long pos_n, unsigned long pos_d, long dur_n, unsigned long dur_d, long midi,
                      size_t part) {
    char id[32];
    snprintf(id, sizeof(id), "n%zu", piece->bag.count);
    bag_add(&piece->bag, tu_note(id, pos_n, pos_d, dur_n, dur_d, PITCH_LETTER_C, 0, 4, NULL), part, 1);
    piece->values = (PitchNumericValue **)realloc(piece->values, piece->bag.count * sizeof(PitchNumericValue *));
    assert(piece->values != NULL);
    assert(pitch_numeric_value_create(piece->tuning, midi, 1, &piece->values[piece->bag.count - 1]) ==
           PITCH_NUMERIC_VALUE_OK);
}

static void piece_free(Piece *piece) {
    for (size_t i = 0; i < piece->bag.count; i++) {
        pitch_numeric_value_destroy(piece->values[i]);
    }
    free(piece->values);
    tuning_system_destroy(piece->tuning);
    bag_free(&piece->bag);
}

static void assign(const Piece *piece, const int *part_staff_count, size_t part_count, int *out) {
    assert(score_staves_assign(piece->bag.events, piece->values, piece->bag.count, piece->bag.part, part_staff_count,
                               part_count, out) == SCORE_STAVES_OK);
}

static void test_program_table(void) {
    for (int program = 0; program <= 8; program++) {
        assert(score_program_uses_grand_staff(program));
    }
    for (int program = 9; program <= 15; program++) {
        assert(!score_program_uses_grand_staff(program));
    }
    for (int program = 16; program <= 21; program++) {
        assert(score_program_uses_grand_staff(program));
    }
    assert(!score_program_uses_grand_staff(22));
    assert(!score_program_uses_grand_staff(45));
    assert(score_program_uses_grand_staff(46));
    assert(!score_program_uses_grand_staff(47));
    assert(!score_program_uses_grand_staff(40));
    assert(!score_program_uses_grand_staff(-1));
    assert(!score_program_uses_grand_staff(127));
}

static void test_one_staff_part(void) {
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 40, 0);
    piece_add(&piece, 1, 4, 1, 4, 80, 0);
    int staff[2] = {0, 0};
    const int counts[1] = {1};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 1 && staff[1] == 1);
    piece_free(&piece);
}

static void test_split_by_middle_c(void) {
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 1, 4, 1, 4, 48, 0);
    piece_add(&piece, 1, 2, 1, 4, 60, 0);
    piece_add(&piece, 3, 4, 1, 4, 54, 0);
    int staff[4];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 1);
    assert(staff[1] == 2);
    assert(staff[2] == 1);
    assert(staff[3] == 2);
    piece_free(&piece);
}

static void test_wide_chord_splits_at_its_gap(void) {
    Piece piece;
    piece_init(&piece);
    /* 43 52 59 64 67: the gaps are 9, 7, 5 and 3, so the split is below 52. */
    piece_add(&piece, 0, 1, 1, 2, 43, 0);
    piece_add(&piece, 0, 1, 1, 2, 52, 0);
    piece_add(&piece, 0, 1, 1, 2, 59, 0);
    piece_add(&piece, 0, 1, 1, 2, 64, 0);
    piece_add(&piece, 0, 1, 1, 2, 67, 0);
    int staff[5];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2 && staff[1] == 1 && staff[2] == 1 && staff[3] == 1 && staff[4] == 1);
    piece_free(&piece);

    piece_init(&piece);
    /* 48 55 | 70 72: the gap 55 to 70 is largest. */
    piece_add(&piece, 0, 1, 1, 2, 48, 0);
    piece_add(&piece, 0, 1, 1, 2, 55, 0);
    piece_add(&piece, 0, 1, 1, 2, 70, 0);
    piece_add(&piece, 0, 1, 1, 2, 72, 0);
    int staff2[4];
    assign(&piece, counts, 1, staff2);
    assert(staff2[0] == 2 && staff2[1] == 2 && staff2[2] == 1 && staff2[3] == 1);
    piece_free(&piece);
}

static void test_narrow_chord_is_not_split(void) {
    Piece piece;
    piece_init(&piece);
    /* Within an octave across middle C: each note by its own rule, so 57 (window, no history) is bass. */
    piece_add(&piece, 0, 1, 1, 2, 57, 0);
    piece_add(&piece, 0, 1, 1, 2, 62, 0);
    int staff[2];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2 && staff[1] == 1);
    piece_free(&piece);
}

static void test_window_follows_the_hand(void) {
    const int counts[1] = {2};
    Piece piece;
    piece_init(&piece);
    /* A bass note at 48 and a treble note at 72, then 58: 10 from the bass, 14 from the treble, so bass. */
    piece_add(&piece, 0, 1, 1, 4, 48, 0);
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 1, 4, 1, 4, 58, 0);
    int staff[3];
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2 && staff[1] == 1 && staff[2] == 2);
    piece_free(&piece);

    piece_init(&piece);
    /* Bass at 40, treble at 72: 58 is nearer the treble hand although it lies below middle C. */
    piece_add(&piece, 0, 1, 1, 4, 40, 0);
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 1, 4, 1, 4, 58, 0);
    assign(&piece, counts, 1, staff);
    assert(staff[2] == 1);
    piece_free(&piece);

    piece_init(&piece);
    /* The history is older than a whole note: the plain middle C rule applies (59 is bass, 61 is treble). */
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 2, 1, 1, 4, 59, 0);
    piece_add(&piece, 4, 1, 1, 4, 61, 0);
    assign(&piece, counts, 1, staff);
    assert(staff[1] == 2 && staff[2] == 1);
    piece_free(&piece);
}

static void test_parts_are_independent(void) {
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 40, 0);
    piece_add(&piece, 0, 1, 1, 4, 40, 1);
    piece_add(&piece, 0, 1, 1, 4, 80, 2);
    int staff[3];
    const int counts[3] = {2, 1, 2};
    assign(&piece, counts, 3, staff);
    assert(staff[0] == 2 && staff[1] == 1 && staff[2] == 1);
    piece_free(&piece);
}

static void test_bad_input(void) {
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 60, 3);
    int staff[1];
    const int counts[1] = {2};
    assert(score_staves_assign(piece.bag.events, piece.values, 1, piece.bag.part, counts, 1, staff) ==
           SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    const int bad_counts[1] = {3};
    piece.bag.part[0] = 0;
    assert(score_staves_assign(piece.bag.events, piece.values, 1, piece.bag.part, bad_counts, 1, staff) ==
           SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    assert(score_staves_assign(piece.bag.events, piece.values, 1, piece.bag.part, counts, 1, NULL) ==
           SCORE_STAVES_ERR_NULL_ARG);
    pitch_numeric_value_destroy(piece.values[0]);
    piece.values[0] = NULL;
    assert(score_staves_assign(piece.bag.events, piece.values, 1, piece.bag.part, counts, 1, staff) ==
           SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    piece_free(&piece);
}

int main(void) {
    test_program_table();
    test_one_staff_part();
    test_split_by_middle_c();
    test_wide_chord_splits_at_its_gap();
    test_narrow_chord_is_not_split();
    test_window_follows_the_hand();
    test_parts_are_independent();
    test_bad_input();
    return 0;
}
