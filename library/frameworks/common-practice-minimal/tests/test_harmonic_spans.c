#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmonic_spans.h"

/*
 * Harmonic span tests: the bass of each segment (R-370) and the grouping of segments into spans (R-369), on the
 * bar shapes of Michelle's Clementi and Mozart readings (analyzer-review) and a chorale bar. Key of C major, 4/4.
 *
 * A bar is a list of notes in sixteenths of a whole note (a bar is 16). build() cuts it into cells at every note
 * start and end, the way adaptive segmentation does: each cell holds every note sounding in it, a note that began
 * earlier held. The metric level of a cell comes from where it starts (0 BAR, 8 HALF_BAR, 4 and 12 BEAT, else
 * INNER), and an attack's beat strength from the same position (strong, medium, weak), as standard-meter gives them.
 */

typedef struct {
    int pitch; /* semitone number, C4 = 60 */
    int start; /* sixteenths from the first bar's start */
    int end;
} Note;

#define SIXTEENTH 0.0625
#define BAR 16
#define MAX_CELLS 48
#define MAX_MEMBERS 16

typedef struct {
    HarmonicSpanCell cells[MAX_CELLS];
    CommonPracticeSegment segments[MAX_CELLS];
    CommonPracticeMember members[MAX_CELLS][MAX_MEMBERS];
    int bass[MAX_CELLS];
    size_t count;
} Piece;

static int position_in_bar(int time) {
    return time % BAR;
}

static HarmonicSpanLevel level_at(int position) {
    if (position == 0) {
        return HARMONIC_SPAN_LEVEL_BAR;
    }
    if (position == 8) {
        return HARMONIC_SPAN_LEVEL_HALF_BAR;
    }
    return position % 4 == 0 ? HARMONIC_SPAN_LEVEL_BEAT : HARMONIC_SPAN_LEVEL_INNER;
}

static BeatStrength strength_at(int position) {
    if (position == 0) {
        return BEAT_STRENGTH_STRONG;
    }
    return position == 8 ? BEAT_STRENGTH_MEDIUM : BEAT_STRENGTH_WEAK;
}

static void build(Piece *piece, const Note *notes, size_t note_count, int bars) {
    memset(piece, 0, sizeof(*piece));
    for (int bar = 0; bar < bars; bar++) {
        int bar_start = bar * BAR;
        int cuts[4 * MAX_CELLS];
        size_t cut_count = 0;
        cuts[cut_count++] = bar_start;
        cuts[cut_count++] = bar_start + BAR;
        for (size_t i = 0; i < note_count; i++) {
            if (notes[i].start > bar_start && notes[i].start < bar_start + BAR) {
                cuts[cut_count++] = notes[i].start;
            }
            if (notes[i].end > bar_start && notes[i].end < bar_start + BAR) {
                cuts[cut_count++] = notes[i].end;
            }
        }
        for (size_t i = 0; i < cut_count; i++) { /* sort */
            for (size_t j = i + 1; j < cut_count; j++) {
                if (cuts[j] < cuts[i]) {
                    int swap = cuts[i];
                    cuts[i] = cuts[j];
                    cuts[j] = swap;
                }
            }
        }
        for (size_t c = 0; c + 1 < cut_count; c++) {
            int t0 = cuts[c];
            int t1 = cuts[c + 1];
            if (t1 == t0) {
                continue;
            }
            size_t m = 0;
            for (size_t i = 0; i < note_count; i++) {
                if (notes[i].start >= t1 || notes[i].end <= t0) {
                    continue;
                }
                CommonPracticeMember *member = &piece->members[piece->count][m++];
                assert(m <= MAX_MEMBERS);
                memset(member, 0, sizeof(*member));
                member->pitch = notes[i].pitch;
                member->held = notes[i].start < t0;
                member->duration = (double)(t1 - t0) * SIXTEENTH;
                member->strength = strength_at(position_in_bar(notes[i].start));
                member->onset = (double)notes[i].start * SIXTEENTH;
            }
            if (m == 0) {
                continue;
            }
            CommonPracticeSegment *segment = &piece->segments[piece->count];
            segment->members = piece->members[piece->count];
            segment->member_count = m;
            segment->id = "cell";
            HarmonicSpanCell *cell = &piece->cells[piece->count];
            cell->segment = segment;
            cell->start = (double)t0 * SIXTEENTH;
            cell->end = (double)t1 * SIXTEENTH;
            cell->measure = bar + 1;
            cell->level = level_at(position_in_bar(t0));
            piece->count++;
            assert(piece->count <= MAX_CELLS);
        }
    }
    assert(harmonic_span_basses(piece->cells, piece->count, piece->bass));
}

static void group(const Piece *piece, HarmonicSpans *spans) {
    assert(harmonic_spans_group(piece->cells, piece->count, piece->bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, spans) ==
           HARMONIC_SPAN_OK);
}

/* The first cell of every span starts at these sixteenths (in order); the count must match. */
static void assert_span_starts(const Piece *piece, const HarmonicSpans *spans, const int *starts, size_t count) {
    assert(spans->count == count);
    for (size_t i = 0; i < count; i++) {
        assert(piece->cells[spans->first[i]].start == (double)starts[i] * SIXTEENTH);
        if (i + 1 < count) {
            assert(spans->last[i] + 1 == spans->first[i + 1]);
        }
    }
    assert(spans->first[0] == 0 && spans->last[count - 1] == piece->count - 1);
}

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

/* Pitches by name. */
enum {
    G2 = 43, B2 = 47, C3 = 48, D3 = 50, E3 = 52, F3 = 53, G3 = 55, A3 = 57, B3 = 59, C4 = 60, D4 = 62, E4 = 64,
    F4 = 65, G4 = 67, A4 = 69, B4 = 71, C5 = 72, D5 = 74, E5 = 76, Fs5 = 78, G5 = 79
};

/* Clementi m1: C5 E5 C5 G4 G4 over a C3 quarter. One chord, the bass resting while the right hand moves. */
static void test_clementi_m1_is_one_span_over_c3(void) {
    Note notes[] = {{C3, 0, 4}, {C5, 0, 4}, {E5, 4, 6}, {C5, 6, 8}, {G4, 8, 12}, {G4, 12, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    for (size_t i = 0; i < piece.count; i++) {
        assert(piece.bass[i] == C3);
    }
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C3);
    harmonic_spans_destroy(&spans);
}

/* Clementi m8: the scale G4 to G5 in eighths over a G2 quarter. One span, the bass G2 persisting. */
static void test_clementi_m8_scale_is_one_span_over_g2(void) {
    Note notes[] = {{G2, 0, 4},  {G4, 0, 2},  {A4, 2, 4},  {B4, 4, 6},   {C5, 6, 8},
                    {D5, 8, 10}, {E5, 10, 12}, {Fs5, 12, 14}, {G5, 14, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    for (size_t i = 0; i < piece.count; i++) {
        assert(piece.bass[i] == G2);
    }
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == G2);
    harmonic_spans_destroy(&spans);
}

/* Mozart m1: an Alberti bass C4 G4 E4 G4 under C5 E5 G5. The lowest note is the figure's own, yet it is one chord. */
static void test_mozart_m1_alberti_is_one_span(void) {
    Note notes[] = {{C4, 0, 2},  {G4, 2, 4},   {E4, 4, 6},   {G4, 6, 8},   {C4, 8, 10}, {G4, 10, 12},
                    {E4, 12, 14}, {G4, 14, 16}, {C5, 0, 8},   {E5, 8, 12},  {G5, 12, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C4);
    harmonic_spans_destroy(&spans);
}

/* Mozart m2: V on beats 1 and 2 (D4 G4 F4 G4 under B4, a C5 D5 turn), I on beats 3 and 4 (C4 G4 E4 G4 under C5).
   Two spans, split at the half bar. */
static const Note MOZART_M2[] = {{D4, 0, 2},  {G4, 2, 4},  {F4, 4, 6},   {G4, 6, 8},   {C4, 8, 10}, {G4, 10, 12},
                                 {E4, 12, 14}, {G4, 14, 16}, {B4, 0, 6},   {C5, 6, 7},   {D5, 7, 8},   {C5, 8, 16}};

static void test_mozart_m2_splits_at_the_half_bar(void) {
    Piece piece;
    build(&piece, MOZART_M2, COUNT(MOZART_M2), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0, 8};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == D4 && spans.bass[1] == C4);
    harmonic_spans_destroy(&spans);
}

/* A passing bass eighth inside a beat (C3 D3 under a held C major triad) does not split the bar. */
static void test_a_passing_bass_eighth_does_not_split_a_beat(void) {
    Note notes[] = {{C3, 0, 2},   {D3, 2, 4},   {E3, 4, 8}, {G3, 8, 16}, {E4, 0, 16}, {G4, 0, 16}, {C5, 0, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    assert(piece.bass[1] == D3); /* the passing note is the bass of its own cell */
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C3);
    harmonic_spans_destroy(&spans);
}

/* Clementi m4: G2, then G3 F3 E3 D3 in eighths, under B4. One span, the bass G2. */
static void test_clementi_m4_descending_bass_line_is_one_span(void) {
    Note notes[] = {{G2, 0, 4}, {G3, 4, 6}, {F3, 6, 8}, {E3, 8, 10}, {D3, 10, 12}, {B4, 0, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    assert(piece.bass[0] == G2);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == G2);
    harmonic_spans_destroy(&spans);
}

/* The bass rule: 12 or more semitones above the last bass of the measure persists; less takes over; a bar's first
   cell takes its lowest sounding pitch; with nothing sounding the bass stays; -1 until something has sounded. */
static void test_the_bass_rule(void) {
    Note notes[] = {{C3, 0, 4},   {E4, 4, 8},   {G4, 8, 12}, {D3, 12, 16}, /* bar 1 */
                    {B4, 16, 24}, {C4, 24, 32},                            /* bar 2 */
                    {C5, 32, 40}};                                         /* bar 3 */
    Piece piece;
    build(&piece, notes, COUNT(notes), 3);
    assert(piece.count == 7);
    assert(piece.bass[0] == C3);
    assert(piece.bass[1] == C3);  /* E4 is 16 above C3 */
    assert(piece.bass[2] == C3);  /* G4 is 19 above */
    assert(piece.bass[3] == D3);  /* D3 is 2 above */
    assert(piece.bass[4] == B4);  /* first cell of the bar: its lowest sounding pitch, however high */
    assert(piece.bass[5] == C4);  /* 11 below: it takes over (only a rise of 12 or more is kept out) */
    assert(piece.bass[6] == C5);  /* first cell of bar 3 */

    /* a rise of exactly 12 persists; one of 11 does not */
    Note octave[] = {{C3, 0, 8}, {C4, 8, 16}};
    build(&piece, octave, COUNT(octave), 1);
    assert(piece.bass[1] == C3);
    Note seventh[] = {{C3, 0, 8}, {B3, 8, 16}};
    build(&piece, seventh, COUNT(seventh), 1);
    assert(piece.bass[1] == B3);

    /* nothing sounding keeps the bass, across a bar line too; -1 only before anything has sounded */
    Note one_note[] = {{G2, 0, 4}};
    build(&piece, one_note, COUNT(one_note), 1);
    CommonPracticeSegment empty = {NULL, 0, "empty", 0, false, 0};
    HarmonicSpanCell cells[4] = {{&empty, 0.0, 0.0625, 1, HARMONIC_SPAN_LEVEL_BAR},
                                 {&piece.segments[0], 0.0625, 0.125, 1, HARMONIC_SPAN_LEVEL_INNER},
                                 {&empty, 0.125, 0.25, 1, HARMONIC_SPAN_LEVEL_INNER},
                                 {&empty, 1.0, 1.25, 2, HARMONIC_SPAN_LEVEL_BAR}};
    int bass[4];
    assert(harmonic_span_basses(cells, 4, bass));
    assert(bass[0] == -1 && bass[1] == G2 && bass[2] == G2 && bass[3] == G2);

    /* a note attacked after the cell start does not count */
    CommonPracticeMember late = {NULL, G2, false, 0.0625, BEAT_STRENGTH_WEAK, NULL, 0.125};
    CommonPracticeMember early = {NULL, E4, false, 0.25, BEAT_STRENGTH_STRONG, NULL, 0.0};
    CommonPracticeMember pair[2] = {late, early};
    CommonPracticeSegment mixed = {pair, 2, "mixed", 0, false, 0};
    HarmonicSpanCell one = {&mixed, 0.0, 0.25, 1, HARMONIC_SPAN_LEVEL_BAR};
    assert(harmonic_span_basses(&one, 1, bass) && bass[0] == E4);
}

/* Two bars group independently: Clementi m1 (one span) then Mozart m2 (two), the second shifted one bar on. */
static void test_two_bars_group_independently(void) {
    Note notes[24];
    size_t count = 0;
    static const Note CLEMENTI_M1[] = {{C3, 0, 4}, {C5, 0, 4}, {E5, 4, 6}, {C5, 6, 8}, {G4, 8, 12}, {G4, 12, 16}};
    for (size_t i = 0; i < COUNT(CLEMENTI_M1); i++) {
        notes[count++] = CLEMENTI_M1[i];
    }
    for (size_t i = 0; i < COUNT(MOZART_M2); i++) {
        notes[count] = MOZART_M2[i];
        notes[count].start += BAR;
        notes[count].end += BAR;
        count++;
    }
    Piece piece;
    build(&piece, notes, count, 2);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0, 16, 24};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C3 && spans.bass[1] == D4 && spans.bass[2] == C4);
    harmonic_spans_destroy(&spans);
}

static void test_bad_input_is_refused(void) {
    Piece piece;
    build(&piece, MOZART_M2, COUNT(MOZART_M2), 1);
    HarmonicSpans spans;
    memset(&spans, 0, sizeof(spans));
    const CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();

    assert(harmonic_spans_group(NULL, piece.count, piece.bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_spans_group(piece.cells, piece.count, NULL, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_spans_group(piece.cells, piece.count, piece.bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, NULL) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_spans_group(piece.cells, piece.count, piece.bass, 12, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(harmonic_spans_group(piece.cells, piece.count, piece.bass, 0, (CommonPracticeKeyMode)99, &defaults,
                                &spans) == HARMONIC_SPAN_ERR_BAD_INPUT);

    HarmonicSpanCell bad = piece.cells[0];
    bad.end = bad.start;
    assert(harmonic_spans_group(&bad, 1, piece.bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    bad = piece.cells[0];
    bad.segment = NULL;
    assert(harmonic_spans_group(&bad, 1, piece.bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(!harmonic_span_basses(&bad, 1, piece.bass));
    bad = piece.cells[0];
    bad.level = (HarmonicSpanLevel)7;
    assert(harmonic_spans_group(&bad, 1, piece.bass, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(!harmonic_span_basses(NULL, 1, piece.bass) && !harmonic_span_basses(piece.cells, 1, NULL));
    assert(spans.first == NULL && spans.count == 0); /* untouched on every error */

    /* no cells: no spans */
    assert(harmonic_span_basses(NULL, 0, NULL));
    assert(harmonic_spans_group(NULL, 0, NULL, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &spans) == HARMONIC_SPAN_OK);
    assert(spans.count == 0 && spans.first == NULL);
    harmonic_spans_destroy(&spans);
    harmonic_spans_destroy(NULL);
}

int main(void) {
    test_clementi_m1_is_one_span_over_c3();
    test_clementi_m8_scale_is_one_span_over_g2();
    test_mozart_m1_alberti_is_one_span();
    test_mozart_m2_splits_at_the_half_bar();
    test_a_passing_bass_eighth_does_not_split_a_beat();
    test_clementi_m4_descending_bass_line_is_one_span();
    test_the_bass_rule();
    test_two_bars_group_independently();
    test_bad_input_is_refused();
    printf("test_harmonic_spans: ok\n");
    return 0;
}
