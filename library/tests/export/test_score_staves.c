#include "score_test_util.h"

#include "export/score_staves.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"

/* score_staves: the grand staff program table, the pairing of two piano streams and the treble / bass assignment of a
   two-staff part. The assignment follows the notes sounding together (no fixed split at middle C, R-362); cases the old
   middle C rule and its 55 to 64 window decided are marked "old rule" with what changed. */

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

static void test_lone_notes_by_pitch_and_hand(void) {
    /* Old rule: split at middle C. Now each note sounds alone, so the nearer hand decides and a tie goes by pitch; the
       staffs are the same as before. */
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
    /* Old rule: not split (within an octave), each note by its own rule. Now the two sound together and split at their
       gap: 57 is bass, 62 treble, as before. */
    piece_add(&piece, 0, 1, 1, 2, 57, 0);
    piece_add(&piece, 0, 1, 1, 2, 62, 0);
    int staff[2];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2 && staff[1] == 1);
    piece_free(&piece);
}

static void test_lone_note_follows_the_hand(void) {
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
    /* The history is older than a whole note: the plain pitch rule applies (59 is bass, 61 is treble). */
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

/* --- the sounding notes rule --- */

static void assign_hinted(const Piece *piece, const int *part_staff_count, size_t part_count, const int *hint,
                          int *out) {
    assert(score_staves_assign_hinted(piece->bag.events, piece->values, piece->bag.count, piece->bag.part,
                                      part_staff_count, part_count, hint, out) == SCORE_STAVES_OK);
}

static void test_alberti_bass_under_a_melody(void) {
    /* Mozart K. 545 m1 in one stream: melody C5 (half), E5, G5 (quarters) over the Alberti eighths C4 G4 E4 G4 twice.
       Old rule: C4, E4 and G4 (60 to 67) sat at or above middle C and went to the treble staff. Now every Alberti
       note sounds with a melody note an octave or more above it, so it is bass. */
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 2, 72, 0);
    piece_add(&piece, 1, 2, 1, 4, 76, 0);
    piece_add(&piece, 3, 4, 1, 4, 79, 0);
    const long alberti[8] = {60, 67, 64, 67, 60, 67, 64, 67};
    for (long k = 0; k < 8; k++) {
        piece_add(&piece, k, 8, 1, 8, alberti[k], 0);
    }
    int staff[11];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    for (int i = 0; i < 3; i++) {
        assert(staff[i] == 1);
    }
    for (int i = 3; i < 11; i++) {
        assert(staff[i] == 2);
    }
    piece_free(&piece);
}

static void test_scale_over_a_held_bass(void) {
    /* A3 to A4 in eighths over a held C3 (48). Old rule: the notes from 57 to 64 lay in the 55 to 64 window and
       followed the first one onto the bass staff. Now every scale note sounds with the held C3 and is treble, and the
       held note is bass. */
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 1, 48, 0);
    const long scale[8] = {57, 59, 60, 62, 64, 65, 67, 69};
    for (long k = 0; k < 8; k++) {
        piece_add(&piece, k, 8, 1, 8, scale[k], 0);
    }
    int staff[9];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2);
    for (int i = 1; i < 9; i++) {
        assert(staff[i] == 1);
    }
    piece_free(&piece);
}

static void test_bass_line_above_middle_c(void) {
    /* A left hand on 62 64 65 67 under a right hand at 79: bass because the right hand sounds above it. */
    Piece piece;
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 1, 79, 0);
    const long left[4] = {62, 64, 65, 67};
    for (long k = 0; k < 4; k++) {
        piece_add(&piece, k, 4, 1, 4, left[k], 0);
    }
    int staff[5];
    const int counts[1] = {2};
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 1);
    for (int i = 1; i < 5; i++) {
        assert(staff[i] == 2);
    }
    piece_free(&piece);
}

static void test_lone_note_one_sided_history(void) {
    const int counts[1] = {2};
    int staff[2];
    Piece piece;
    /* Only treble history, the note is more than an octave away: by pitch (bass), not pulled up to the treble. */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 1, 4, 1, 4, 48, 0);
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 1 && staff[1] == 2);
    piece_free(&piece);

    /* Only treble history within an octave: the line continues on the treble staff (as the old window rule did). */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 72, 0);
    piece_add(&piece, 1, 4, 1, 4, 62, 0);
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 1 && staff[1] == 1);
    piece_free(&piece);

    /* Old rule: 62 after a bass at 48 (window) went to the bass. Now it is 14 away, past an octave: by pitch, treble. */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 48, 0);
    piece_add(&piece, 1, 4, 1, 4, 62, 0);
    assign(&piece, counts, 1, staff);
    assert(staff[0] == 2 && staff[1] == 1);
    piece_free(&piece);
}

static void test_hints_fix_the_staff(void) {
    const int counts[1] = {2};
    Piece piece;
    piece_init(&piece);
    /* The hands as two streams: a left hand above C4 for the bar stays on staff 2 whatever its pitch. */
    for (int q = 0; q < 4; q++) {
        piece_add(&piece, q, 4, 1, 4, 72, 0);
    }
    const long left[4] = {62, 64, 65, 67};
    for (long k = 0; k < 4; k++) {
        piece_add(&piece, k, 4, 1, 4, left[k], 0);
    }
    const int hint[8] = {1, 1, 1, 1, 2, 2, 2, 2};
    int staff[8];
    assign_hinted(&piece, counts, 1, hint, staff);
    for (int i = 0; i < 8; i++) {
        assert(staff[i] == hint[i]);
    }
    piece_free(&piece);

    /* A hinted bass note at 64 and a free note at 62 sounding with it: without the hint 62 would be bass and 64
       treble. The hint keeps 64 on the bass, and 62 follows the nearest hinted note. */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 2, 64, 0);
    piece_add(&piece, 0, 1, 1, 2, 62, 0);
    const int hint2[2] = {2, 0};
    int staff2[2];
    assign_hinted(&piece, counts, 1, hint2, staff2);
    assert(staff2[0] == 2 && staff2[1] == 2);
    assign(&piece, counts, 1, staff2);
    assert(staff2[0] == 1 && staff2[1] == 2);
    piece_free(&piece);

    /* A free note between a hinted treble 72 and a hinted bass 48 splits at its larger gap. */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 2, 48, 0);
    piece_add(&piece, 0, 1, 1, 2, 66, 0);
    piece_add(&piece, 0, 1, 1, 2, 72, 0);
    const int hint3[3] = {2, 0, 1};
    int staff3[3];
    assign_hinted(&piece, counts, 1, hint3, staff3);
    assert(staff3[0] == 2 && staff3[1] == 1 && staff3[2] == 1);
    piece_free(&piece);

    /* A hint of 3 is refused; in a part with one staff a hint changes nothing. */
    piece_init(&piece);
    piece_add(&piece, 0, 1, 1, 4, 40, 0);
    const int bad_hint[1] = {3};
    int one[1] = {0};
    assert(score_staves_assign_hinted(piece.bag.events, piece.values, 1, piece.bag.part, counts, 1, bad_hint, one) ==
           SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    const int single[1] = {1};
    const int two_hint[1] = {2};
    assign_hinted(&piece, single, 1, two_hint, one);
    assert(one[0] == 1);
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

/* --- pairing the hands of one piano --- */

/* Notes of a stream: a quarter note at each quarter from first to last (inclusive), all at one MIDI pitch. */
static void stream_add(Piece *piece, size_t stream, int first, int last, long midi) {
    for (int q = first; q <= last; q++) {
        piece_add(piece, q, 4, 1, 4, midi, stream);
    }
}

static ScoreStavesResult pair(const Piece *piece, const ScoreStream *streams, size_t stream_count, size_t *part_of,
                              int *staff_of, size_t *part_count) {
    return score_staves_pair_hands(piece->bag.events, piece->values, piece->bag.count, piece->bag.part, streams,
                                   stream_count, part_of, staff_of, part_count);
}

static void test_pair_two_piano_streams(void) {
    Piece piece;
    piece_init(&piece);
    /* The right hand on stream 0 at 72, the left hand on stream 1 at 62 to 67: it lies above C4 for the whole bar and
       is still the lower hand, so it stays staff 2. */
    stream_add(&piece, 0, 0, 3, 72);
    piece_add(&piece, 0, 4, 1, 4, 62, 1);
    piece_add(&piece, 1, 4, 1, 4, 64, 1);
    piece_add(&piece, 2, 4, 1, 4, 65, 1);
    piece_add(&piece, 3, 4, 1, 4, 67, 1);
    const ScoreStream streams[2] = {{0, 4}, {0, 4}};
    size_t part_of[2];
    int staff_of[2];
    size_t parts = 0;
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 1 && part_of[0] == 0 && part_of[1] == 0);
    assert(staff_of[0] == 1 && staff_of[1] == 2);
    piece_free(&piece);

    /* The lower stream first: the staffs follow the median, not the stream order. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 48);
    stream_add(&piece, 1, 0, 3, 76);
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 1 && part_of[0] == 0 && part_of[1] == 0);
    assert(staff_of[0] == 2 && staff_of[1] == 1);
    piece_free(&piece);
}

static void test_pair_needs_two_pianos(void) {
    Piece piece;
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    stream_add(&piece, 1, 0, 3, 48);
    stream_add(&piece, 2, 0, 3, 55);
    /* A piano and a flute (program 73) are not one part; the flute pairs with nothing, so the piano and the other
       piano stream still pair. */
    const ScoreStream streams[3] = {{0, 4}, {73, 4}, {0, 4}};
    size_t part_of[3];
    int staff_of[3];
    size_t parts = 0;
    assert(pair(&piece, streams, 3, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2 && part_of[0] == 0 && part_of[1] == 1 && part_of[2] == 0);
    assert(staff_of[0] == 1 && staff_of[1] == 0 && staff_of[2] == 2);
    piece_free(&piece);

    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    stream_add(&piece, 1, 0, 3, 48);
    const ScoreStream piano_flute[2] = {{0, 4}, {73, 4}};
    assert(pair(&piece, piano_flute, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2 && part_of[0] == 0 && part_of[1] == 1 && staff_of[0] == 0 && staff_of[1] == 0);
    piece_free(&piece);
}

static void test_pair_three_piano_streams(void) {
    Piece piece;
    piece_init(&piece);
    /* Stream 0 at 76 and stream 1 at 48 sound together for four notes each (eight overlapping notes). Stream 2 at
       60 shares two notes with stream 1 (four overlapping notes), so streams 0 and 1 are the pair. */
    stream_add(&piece, 0, 0, 3, 76);
    stream_add(&piece, 1, 0, 3, 48);
    stream_add(&piece, 2, 2, 5, 60);
    const ScoreStream streams[3] = {{0, 4}, {0, 4}, {0, 4}};
    size_t part_of[3];
    int staff_of[3];
    size_t parts = 0;
    assert(pair(&piece, streams, 3, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2 && part_of[0] == 0 && part_of[1] == 0 && part_of[2] == 1);
    assert(staff_of[0] == 1 && staff_of[1] == 2 && staff_of[2] == 0);
    piece_free(&piece);
}

static void test_pair_conditions(void) {
    size_t part_of[2];
    int staff_of[2];
    size_t parts = 0;
    const ScoreStream streams[2] = {{0, 4}, {0, 4}};
    Piece piece;

    /* One after the other: nothing sounds together. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    stream_add(&piece, 1, 4, 7, 48);
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2 && part_of[1] == 1 && staff_of[0] == 0 && staff_of[1] == 0);
    piece_free(&piece);

    /* Two of four notes sound together: half of the shorter stream, so they pair. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    stream_add(&piece, 1, 2, 5, 48);
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 1 && staff_of[0] == 1 && staff_of[1] == 2);
    piece_free(&piece);

    /* One of four notes sounds together: less than half, so they stay apart. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    stream_add(&piece, 1, 3, 6, 48);
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2);
    piece_free(&piece);

    /* The same median pitch: no hand is higher, so they stay apart. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 60);
    stream_add(&piece, 1, 0, 3, 60);
    assert(pair(&piece, streams, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 2);
    piece_free(&piece);

    /* A held note counts: a whole note under four quarters sounds together with all of them. */
    piece_init(&piece);
    stream_add(&piece, 0, 0, 3, 72);
    piece_add(&piece, 0, 1, 1, 1, 48, 1);
    const ScoreStream held[2] = {{0, 4}, {0, 1}};
    assert(pair(&piece, held, 2, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 1 && staff_of[0] == 1 && staff_of[1] == 2);
    piece_free(&piece);
}

static void test_pair_bad_input(void) {
    Piece piece;
    piece_init(&piece);
    stream_add(&piece, 0, 0, 1, 72);
    size_t part_of[1];
    int staff_of[1];
    size_t parts = 0;
    const ScoreStream streams[1] = {{0, 2}};
    assert(pair(&piece, streams, 1, part_of, staff_of, &parts) == SCORE_STAVES_OK);
    assert(parts == 1 && part_of[0] == 0 && staff_of[0] == 0);
    const ScoreStream wrong_count[1] = {{0, 3}};
    assert(pair(&piece, wrong_count, 1, part_of, staff_of, &parts) == SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    piece.bag.part[1] = 4;
    assert(pair(&piece, streams, 1, part_of, staff_of, &parts) == SCORE_STAVES_ERR_INCONSISTENT_INPUT);
    piece.bag.part[1] = 0;
    assert(score_staves_pair_hands(piece.bag.events, piece.values, piece.bag.count, piece.bag.part, streams, 1, part_of,
                                   staff_of, NULL) == SCORE_STAVES_ERR_NULL_ARG);
    assert(score_staves_pair_hands(piece.bag.events, piece.values, piece.bag.count, piece.bag.part, NULL, 1, part_of,
                                   staff_of, &parts) == SCORE_STAVES_ERR_NULL_ARG);
    piece_free(&piece);

    /* No streams, no notes: no parts. */
    parts = 7;
    assert(score_staves_pair_hands(NULL, NULL, 0, NULL, NULL, 0, NULL, NULL, &parts) == SCORE_STAVES_OK);
    assert(parts == 0);
}

int main(void) {
    test_program_table();
    test_one_staff_part();
    test_lone_notes_by_pitch_and_hand();
    test_wide_chord_splits_at_its_gap();
    test_narrow_chord_is_not_split();
    test_lone_note_follows_the_hand();
    test_parts_are_independent();
    test_alberti_bass_under_a_melody();
    test_scale_over_a_held_bass();
    test_bass_line_above_middle_c();
    test_lone_note_one_sided_history();
    test_hints_fix_the_staff();
    test_bad_input();
    test_pair_two_piano_streams();
    test_pair_needs_two_pianos();
    test_pair_three_piano_streams();
    test_pair_conditions();
    test_pair_bad_input();
    return 0;
}
