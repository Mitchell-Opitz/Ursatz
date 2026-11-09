#include <assert.h>
#include <stdbool.h>
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

/* voices[i] is the voice id of notes[i], or voices is NULL for none. */
static void build_voiced(Piece *piece, const Note *notes, const char *const *voices, size_t note_count, int bars) {
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
                member->voice_id = voices != NULL ? voices[i] : NULL;
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

static void build(Piece *piece, const Note *notes, size_t note_count, int bars) {
    build_voiced(piece, notes, NULL, note_count, bars);
}

static void group(const Piece *piece, HarmonicSpans *spans) {
    assert(harmonic_spans_group(piece->cells, piece->count, piece->bass, NULL, spans) == HARMONIC_SPAN_OK);
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
    A2 = 45, G2 = 43, B2 = 47, C3 = 48, D3 = 50, E3 = 52, F3 = 53, G3 = 55, A3 = 57, B3 = 59, C4 = 60, D4 = 62,
    E4 = 64, F4 = 65, G4 = 67, Gs4 = 68, A4 = 69, B4 = 71, C5 = 72, D5 = 74, E5 = 76, F5 = 77, Fs5 = 78, G5 = 79,
    A5 = 81, B5 = 83, C6 = 84, Fs3 = 54
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
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();

    assert(harmonic_spans_group(NULL, piece.count, piece.bass, NULL, &spans) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_spans_group(piece.cells, piece.count, NULL, NULL, &spans) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_spans_group(piece.cells, piece.count, piece.bass, NULL, NULL) ==
           HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    CommonPracticeWeightedConfig broken = defaults;
    broken.held_factor = -1.0;
    assert(harmonic_spans_group(piece.cells, piece.count, piece.bass, &broken, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);

    HarmonicSpanCell bad = piece.cells[0];
    bad.end = bad.start;
    assert(harmonic_spans_group(&bad, 1, piece.bass, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    bad = piece.cells[0];
    bad.segment = NULL;
    assert(harmonic_spans_group(&bad, 1, piece.bass, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(!harmonic_span_basses(&bad, 1, piece.bass));
    bad = piece.cells[0];
    bad.level = (HarmonicSpanLevel)7;
    assert(harmonic_spans_group(&bad, 1, piece.bass, NULL, &spans) ==
           HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(!harmonic_span_basses(NULL, 1, piece.bass) && !harmonic_span_basses(piece.cells, 1, NULL));
    assert(spans.first == NULL && spans.count == 0); /* untouched on every error */

    /* no cells: no spans */
    assert(harmonic_span_basses(NULL, 0, NULL));
    assert(harmonic_spans_group(NULL, 0, NULL, NULL, &spans) == HARMONIC_SPAN_OK);
    assert(spans.count == 0 && spans.first == NULL);
    harmonic_spans_destroy(&spans);
    harmonic_spans_destroy(NULL);
}

/* A four-voice bar of quarters: voices[v][beat] is the pitch of voice v on that beat. */
static size_t add_quarters(Note *notes, size_t count, const int *pitches) {
    for (int beat = 0; beat < 4; beat++) {
        notes[count].pitch = pitches[beat];
        notes[count].start = beat * 4;
        notes[count].end = beat * 4 + 4;
        count++;
    }
    return count;
}

static size_t chorale(Note *notes, const int *soprano, const int *alto, const int *tenor, const int *bass) {
    size_t count = add_quarters(notes, 0, soprano);
    count = add_quarters(notes, count, alto);
    count = add_quarters(notes, count, tenor);
    return add_quarters(notes, count, bass);
}

/* Groups the bar and checks the span starts (sixteenths). */
static void assert_bar_spans(const Note *notes, size_t count, const int *starts, size_t span_count, HarmonicSpans *spans,
                             Piece *piece) {
    build(piece, notes, count, 1);
    group(piece, spans);
    assert_span_starts(piece, spans, starts, span_count);
}

/* Mozart m3: IV then I over an Alberti bass. Two spans, the bass C4 on both. */
static void test_mozart_m3_iv_then_i(void) {
    Note notes[] = {{C4, 0, 2},  {A4, 2, 4},  {F4, 4, 6},   {A4, 6, 8},  {C4, 8, 10}, {G4, 10, 12},
                    {E4, 12, 14}, {G4, 14, 16}, {A5, 0, 8},   {G5, 8, 12}, {C6, 12, 16}};
    int starts[] = {0, 8};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    assert(spans.bass[0] == C4 && spans.bass[1] == C4);
    harmonic_spans_destroy(&spans);
}

/* Mozart m4: V7 then I. Two spans. */
static void test_mozart_m4_v7_then_i(void) {
    Note notes[] = {{B3, 0, 2},  {G4, 2, 4},  {D4, 4, 6},   {G4, 6, 8},  {C4, 8, 10}, {G4, 10, 12}, {E4, 12, 14},
                    {G4, 14, 16}, {G5, 0, 4},  {F5, 4, 6},   {E5, 6, 7},  {F5, 7, 8},  {E5, 8, 12}};
    int starts[] = {0, 8};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* Mozart m5: IV with a scale over an F. One span. */
static void test_mozart_m5_scale_over_f_is_one_span(void) {
    Note notes[] = {{F4, 0, 4},  {A4, 0, 2},  {B4, 2, 3},   {C5, 3, 4},   {D5, 4, 5},   {E5, 5, 6},   {F5, 6, 7},
                    {G5, 7, 8},  {A5, 8, 9},  {G5, 9, 10},  {F5, 10, 11}, {E5, 11, 12}, {D5, 12, 13}, {C5, 13, 14},
                    {B4, 14, 15}, {A4, 15, 16}, {F3, 12, 16}, {C4, 12, 16}};
    int starts[] = {0};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* Mozart m11 as in the corpus MIDI: V I V I by beat, the left hand an arpeggio in sixteenths over G2 on every beat. */
static void test_mozart_m11_changes_chord_every_beat(void) {
    static const int left[16] = {G2, B2, D3, G3, G2, C3, E3, G3, G2, B2, D3, G3, G2, C3, E3, G3};
    Note notes[24];
    size_t count = 0;
    for (int i = 0; i < 16; i++) {
        notes[count].pitch = left[i];
        notes[count].start = i;
        notes[count].end = i + 1;
        count++;
    }
    static const Note right[] = {{B4, 0, 2}, {G5, 2, 4}, {E5, 4, 5}, {C5, 6, 7}, {D5, 8, 10}, {G5, 10, 12}, {E5, 12, 13},
                                 {C5, 14, 15}};
    for (size_t i = 0; i < COUNT(right); i++) {
        notes[count++] = right[i];
    }
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    for (size_t i = 0; i < spans.count; i++) {
        assert(spans.bass[i] == G2);
    }
    harmonic_spans_destroy(&spans);
}

/* Mozart m12: V, the notes in the right hand on one beat. One span. */
static void test_mozart_m12_v_is_one_span(void) {
    Note notes[] = {{G2, 0, 3}, {D5, 0, 3}, {G3, 4, 7}, {B4, 4, 7}, {D5, 4, 7}, {G5, 4, 7}, {G2, 8, 11}, {G4, 8, 11}};
    int starts[] = {0};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* Chorale I IV V I: a new root on every beat. Four spans. */
static void test_chorale_i_iv_v_i_is_four_spans(void) {
    static const int s[4] = {C5, C5, B4, C5}, a[4] = {E4, F4, D4, E4}, t[4] = {G3, A3, G3, G3}, b[4] = {C3, F3, G3, C3};
    Note notes[16];
    size_t count = chorale(notes, s, a, t, b);
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    assert(spans.bass[0] == C3 && spans.bass[1] == F3 && spans.bass[2] == G3 && spans.bass[3] == C3);
    harmonic_spans_destroy(&spans);
}

/* Chorale I vi ii V: I and vi stay apart (read together they would be one A minor seventh). Four spans. */
static void test_chorale_i_vi_ii_v_keeps_i_and_vi_apart(void) {
    static const int s[4] = {E5, E5, D5, D5}, a[4] = {C5, C5, A4, B4}, t[4] = {G4, A4, F4, G4}, b[4] = {C3, A2, D3, G2};
    Note notes[16];
    size_t count = chorale(notes, s, a, t, b);
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* Chorale I I6 IV V: the first two beats are one chord. Spans at 0, 8 and 12. */
static void test_chorale_i_i6_iv_v_joins_the_inversion(void) {
    static const int s[4] = {C5, C5, C5, B4}, a[4] = {G4, G4, A4, G4}, t[4] = {E4, C4, F4, D4}, b[4] = {C3, E3, F3, G3};
    Note notes[16];
    size_t count = chorale(notes, s, a, t, b);
    int starts[] = {0, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* V7 I V7 I over a G3 pedal: four spans, the pedal under every one. */
static void test_a_pedal_does_not_hide_the_changes(void) {
    static const int s[4] = {F5, E5, F5, E5}, a[4] = {D5, C5, D5, C5}, t[4] = {B4, G4, B4, G4}, b[4] = {G3, G3, G3, G3};
    Note notes[16];
    size_t count = chorale(notes, s, a, t, b);
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* Chorale I IV V I with bass eighths C3 D3 F3 E3 in the first half: the passing eighths do not add spans. */
static void test_chorale_with_bass_eighths_is_four_spans(void) {
    static const int s[4] = {C5, C5, B4, C5}, a[4] = {E4, F4, D4, E4}, t[4] = {G3, A3, G3, G3};
    Note notes[24];
    size_t count = 0;
    count = add_quarters(notes, count, s);
    count = add_quarters(notes, count, a);
    count = add_quarters(notes, count, t);
    static const Note bass[] = {{C3, 0, 2}, {D3, 2, 4}, {F3, 4, 6}, {E3, 6, 8}, {G3, 8, 12}, {C3, 12, 16}};
    for (size_t i = 0; i < COUNT(bass); i++) {
        notes[count++] = bass[i];
    }
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, count, starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* A chord outside any home key: C, then E major, then A minor. E major is complete only when read key-free. */
static void test_a_chord_outside_the_key_is_read(void) {
    Note notes[] = {{C3, 0, 8},  {E4, 0, 8},  {G4, 0, 8},   {C5, 0, 8},   {E3, 8, 12}, {Gs4, 8, 12}, {B4, 8, 12},
                    {E5, 8, 12}, {A2, 12, 16}, {A4, 12, 16}, {C5, 12, 16}, {E5, 12, 16}};
    int starts[] = {0, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* A beat with a chord change at its half: A minor then D minor on the third beat's two eighths. */
static void test_a_beat_splits_at_its_half(void) {
    Note notes[] = {{C3, 0, 4},  {E4, 0, 4},  {G4, 0, 4},   {C5, 0, 4},   {F3, 4, 8},   {F4, 4, 8},  {A4, 4, 8},
                    {C5, 4, 8},  {A2, 8, 10}, {C4, 8, 10},  {E4, 8, 10},  {A4, 8, 10},  {D3, 10, 12}, {D4, 10, 12},
                    {F4, 10, 12}, {A4, 10, 12}, {G2, 12, 16}, {D4, 12, 16}, {G4, 12, 16}, {B4, 12, 16}};
    int starts[] = {0, 4, 8, 10, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    harmonic_spans_destroy(&spans);
}

/* An accented bass tone that resolves by step (A2 to G2 under a G chord's upper voices) does not break the span: the
   third span is read over G2. */
static void test_an_accented_bass_tone_resolves_into_the_span_bass(void) {
    Note notes[] = {{C3, 0, 4},  {E4, 0, 4},  {G4, 0, 4},  {C5, 0, 4},  {F3, 4, 8},   {F4, 4, 8},  {A4, 4, 8},
                    {C5, 4, 8},  {A2, 8, 10}, {G2, 10, 12}, {D4, 8, 12}, {G4, 8, 12},  {B4, 8, 12}, {C3, 12, 16},
                    {E4, 12, 16}, {G4, 12, 16}, {C5, 12, 16}};
    int starts[] = {0, 4, 8, 12};
    Piece piece;
    HarmonicSpans spans;
    assert_bar_spans(notes, COUNT(notes), starts, COUNT(starts), &spans, &piece);
    assert(spans.bass[2] == G2);
    harmonic_spans_destroy(&spans);
}

/* --- the two-voice span rules (R-379, R-382) --- */

/* The members of one span window [from, to) in sixteenths, as the chain builds them: every note sounding in it once, held
   when it began before the window, its duration the overlap, the strength of its attack. A note may begin before the first
   bar (a negative start). */
static size_t window_members(const Note *notes, const char *const *voices, size_t note_count, int from, int to,
                             CommonPracticeMember *out) {
    size_t n = 0;
    for (size_t i = 0; i < note_count; i++) {
        if (notes[i].start >= to || notes[i].end <= from) {
            continue;
        }
        int begin = notes[i].start > from ? notes[i].start : from;
        int stop = notes[i].end < to ? notes[i].end : to;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].pitch = notes[i].pitch;
        out[n].held = notes[i].start < from;
        out[n].duration = (double)(stop - begin) * SIXTEENTH;
        out[n].strength = strength_at(((notes[i].start % BAR) + BAR) % BAR);
        out[n].voice_id = voices != NULL ? voices[i] : NULL;
        out[n].onset = (double)notes[i].start * SIXTEENTH;
        n++;
    }
    return n;
}

/* The config the chain marks accents with: its span seventh penalty (ANALYSIS_CHAIN_SPAN_SEVENTH_PENALTY, R-376), so that a
   passing tone does not turn a triad into a seventh chord. */
static CommonPracticeWeightedConfig span_config(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.seventh_penalty = 0.15;
    return config;
}

/* Marks the accents of the window over `bass` and returns the pitches of the flagged members in `flagged`, and their count. */
static size_t flagged_pitches(const Note *notes, const char *const *voices, size_t note_count, int from, int to, int bass,
                              int flagged[MAX_MEMBERS]) {
    CommonPracticeMember members[MAX_MEMBERS];
    size_t n = window_members(notes, voices, note_count, from, to, members);
    CommonPracticeWeightedConfig config = span_config();
    assert(harmonic_span_mark_resolved_accents(members, n, (double)from * SIXTEENTH, (double)to * SIXTEENTH, bass, &config) ==
           HARMONIC_SPAN_OK);
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        if (members[i].resolves_in_segment) {
            flagged[count++] = members[i].pitch;
        }
    }
    return count;
}

/* A bass that moves under a melody that moves: C3 under E5, then G2 under D5. The second half is a root G and its fifth, and
   a bare fifth over the root is a complete reading, so the bar is two spans, the second over G2. */
static void test_a_root_and_fifth_over_the_root_is_a_span(void) {
    Note notes[] = {{C3, 0, 8}, {E5, 0, 8}, {G2, 8, 16}, {D5, 8, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0, 8};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C3 && spans.bass[1] == G2);
    harmonic_spans_destroy(&spans);
}

/* An accented step over a held bass: G2 under E5 for a beat, then D5 (one voice). The E5 resolves to D5, a chord tone of the
   G chord, and E is none: the bar is one span and the E5 is flagged. */
static void test_an_accented_step_over_a_held_bass_is_flagged(void) {
    Note notes[] = {{G2, 0, 16}, {E5, 0, 4}, {D5, 4, 16}};
    const char *voices[] = {"bass", "soprano", "soprano"};
    Piece piece;
    build_voiced(&piece, notes, voices, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    harmonic_spans_destroy(&spans);
    int flagged[MAX_MEMBERS];
    assert(flagged_pitches(notes, voices, COUNT(notes), 0, 16, G2, flagged) == 1 && flagged[0] == E5);
}

/* The same step that lands on a note of the chord it sat in is no accent: over C3, E5 D5 C5 is a chord tone then two steps,
   and the E5 stays. */
static void test_a_chord_tone_that_steps_away_is_not_flagged(void) {
    Note notes[] = {{C3, 0, 16}, {E5, 0, 4}, {D5, 4, 8}, {C5, 8, 16}};
    const char *voices[] = {"bass", "soprano", "soprano", "soprano"};
    int flagged[MAX_MEMBERS];
    assert(flagged_pitches(notes, voices, COUNT(notes), 0, 16, C3, flagged) == 0);
}

/* An accent that began before the window (held into it) over a bass that is held: F5 sounding to the first beat, then E5.
   C3 and G4 are the chord; the F5 is flagged. */
static void test_a_held_accent_into_the_window_is_flagged(void) {
    Note notes[] = {{C3, 0, 16}, {G4, 0, 16}, {F5, -4, 4}, {E5, 4, 16}};
    const char *voices[] = {"bass", "tenor", "soprano", "soprano"};
    int flagged[MAX_MEMBERS];
    assert(flagged_pitches(notes, voices, COUNT(notes), 0, 16, C3, flagged) == 1 && flagged[0] == F5);
}

/* The bass must still sound when the accent resolves: a bass that stops first leaves no held bass to be accented over. */
static void test_a_bass_that_stops_before_the_resolution_flags_nothing(void) {
    Note notes[] = {{G2, 0, 2}, {E5, 0, 4}, {D5, 4, 16}, {B4, 0, 16}};
    const char *voices[] = {"bass", "soprano", "soprano", "alto"};
    int flagged[MAX_MEMBERS];
    assert(flagged_pitches(notes, voices, COUNT(notes), 0, 16, G2, flagged) == 0);
}

/* A bass that ends as the step lands holds nothing: block chords I then IV, each note struck on its beat, are not accents over
   the first bass. The E4 and G4 of the I are not flagged against the F4 and A4 of the IV, and the bar stays four spans. */
static void test_a_bass_that_ends_at_the_resolution_flags_nothing(void) {
    Note notes[] = {{C3, 0, 4},  {E4, 0, 4},  {G4, 0, 4},  {C5, 0, 4},  {F3, 4, 8},   {F4, 4, 8},   {A4, 4, 8},   {C5, 4, 8},
                    {G2, 8, 12}, {B3, 8, 12}, {D4, 8, 12}, {F4, 8, 12}, {C3, 12, 16}, {E4, 12, 16}, {G4, 12, 16}, {C5, 12, 16}};
    const char *voices[] = {"b", "t", "a", "s", "b", "t", "a", "s", "b", "t", "a", "s", "b", "t", "a", "s"};
    int flagged[MAX_MEMBERS];
    assert(flagged_pitches(notes, voices, COUNT(notes), 0, 8, C3, flagged) == 0);
    Piece piece;
    build_voiced(&piece, notes, voices, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0, 4, 8, 12};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == C3 && spans.bass[1] == F3);
    harmonic_spans_destroy(&spans);
}

/* Direct cases of harmonic_span_mark_resolved_accents: bad input, no voice ids, no bass; every flag is cleared first. */
static void test_marking_resolved_accents_refuses_bad_input_and_reads_nothing_without_voices(void) {
    Note notes[] = {{G2, 0, 16}, {E5, 0, 4}, {D5, 4, 16}};
    const char *voices[] = {"bass", "soprano", "soprano"};
    CommonPracticeMember members[MAX_MEMBERS];
    size_t n = window_members(notes, voices, COUNT(notes), 0, 16, members);
    assert(n == 3);

    assert(harmonic_span_mark_resolved_accents(NULL, n, 0.0, 1.0, G2, NULL) == HARMONIC_SPAN_ERR_NULL_ARGUMENT);
    assert(harmonic_span_mark_resolved_accents(members, n, 1.0, 1.0, G2, NULL) == HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(harmonic_span_mark_resolved_accents(members, n, 1.0, 0.0, G2, NULL) == HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(harmonic_span_mark_resolved_accents(members, n, 0.0, 1.0, -2, NULL) == HARMONIC_SPAN_ERR_BAD_INPUT);
    assert(harmonic_span_mark_resolved_accents(NULL, 0, 0.0, 1.0, G2, NULL) == HARMONIC_SPAN_OK);

    /* no bass: nothing is flagged, and a flag that was set is cleared */
    for (size_t i = 0; i < n; i++) {
        members[i].resolves_in_segment = true;
    }
    assert(harmonic_span_mark_resolved_accents(members, n, 0.0, 1.0, -1, NULL) == HARMONIC_SPAN_OK);
    for (size_t i = 0; i < n; i++) {
        assert(!members[i].resolves_in_segment);
    }

    /* no voice ids: no resolution can be found */
    n = window_members(notes, NULL, COUNT(notes), 0, 16, members);
    for (size_t i = 0; i < n; i++) {
        members[i].resolves_in_segment = true;
    }
    assert(harmonic_span_mark_resolved_accents(members, n, 0.0, 1.0, G2, NULL) == HARMONIC_SPAN_OK);
    for (size_t i = 0; i < n; i++) {
        assert(!members[i].resolves_in_segment);
    }
}

/* A held bass note is no new bass: D3 sounds through the first half of the bar and the right hand moves above it on the second
   beat. No span starts at the second beat; the bar is cut only where the bass moves. */
static void test_a_held_bass_note_is_not_cut(void) {
    Note notes[] = {{D3, 0, 8}, {F4, 0, 8}, {A4, 0, 4}, {G4, 4, 8}, {B4, 4, 8}, {G3, 8, 16}, {B3, 8, 16}, {D4, 8, 16}, {F4, 8, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    bool beat_cell = false;
    for (size_t i = 1; i < piece.count; i++) {
        if (piece.cells[i].start == 4.0 * SIXTEENTH) {
            beat_cell = true;
            assert(piece.bass[i] == D3 && piece.bass[i - 1] == D3); /* the cell exists and the bass has not moved */
        }
    }
    assert(beat_cell);
    HarmonicSpans spans;
    group(&piece, &spans);
    for (size_t i = 0; i < spans.count; i++) {
        assert(piece.cells[spans.first[i]].start != 4.0 * SIXTEENTH);
    }
    harmonic_spans_destroy(&spans);
}

/* A pedal that is held while the chord above changes: A3 through the bar, A minor over it, then D minor over it on the half bar
   (A is D minor's fifth). Each half reads as a chord of its own and would beat the bar read as one, but the bass has not
   moved and the A3 is held, so the bar is one span over A3. */
static void test_a_held_bass_under_a_changing_chord_is_one_span(void) {
    Note notes[] = {{A3, 0, 16}, {C5, 0, 8}, {E5, 0, 8}, {D5, 8, 16}, {F5, 8, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[0] == A3);
    harmonic_spans_destroy(&spans);
}

/* A lone upper note is no new bass (R-387): the left hand rests after the first beat, so the melody (D5, then A5, each under
   twelve semitones above the bass before it) becomes the bass by the bass rule. Nothing sounds above either note, so neither
   is a new bass and the bar is one span. */
static void test_a_lone_upper_note_is_no_new_bass(void) {
    Note notes[] = {{F4, 0, 4}, {A4, 0, 4}, {C5, 0, 4}, {D5, 4, 8}, {A5, 8, 12}, {F3, 12, 16}, {C5, 12, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    harmonic_spans_destroy(&spans);
}

/* A diminished seventh over its bass is rooted on the bass (R-387): Clementi m6 as in the corpus MIDI. F#3 under E5 and C5 on the
   last beat scores F# diminished and C diminished seventh alike in every key, and the first key in order read C, whose bass F#
   is no chord root, so the part was incomplete and the bar stayed one span. Rooted on the bass it is cut at beat 4. */
static void test_a_diminished_seventh_is_rooted_on_the_bass(void) {
    Note notes[] = {{C4, 0, 4}, {E5, 0, 4}, {G5, 4, 6}, {E5, 6, 8}, {C5, 8, 12}, {E5, 12, 14}, {C5, 14, 16}, {Fs3, 12, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0, 12};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    assert(spans.bass[1] == Fs3);
    harmonic_spans_destroy(&spans);
}

/* True when a span starts at the given sixteenth. */
static bool span_starts_at(const Piece *piece, const HarmonicSpans *spans, int sixteenth) {
    for (size_t i = 0; i < spans->count; i++) {
        if (piece->cells[spans->first[i]].start == (double)sixteenth * SIXTEENTH) {
            return true;
        }
    }
    return false;
}

/* Mozart m10 as in the corpus MIDI, then m11's first beat: the left hand F3 G3 A3 in one voice and an F#3 on the last beat in
   another, under a right hand that plays F5 on that beat (the F the F# alters), and a G2 next. The F#3 is a chromatic bass
   against its chord tone, no new chord: no span of the measure starts at the last beat (R-381). */
static void test_a_chromatic_bass_against_its_chord_tone_is_not_cut(void) {
    Note notes[] = {{F3, 0, 6},  {G3, 6, 8},  {A3, 8, 12}, {Fs3, 12, 16}, {A5, 0, 1},  {B5, 1, 2},  {C6, 2, 3},   {B5, 3, 4},
                    {A5, 4, 5},  {G5, 5, 6},  {F5, 6, 7},  {E5, 7, 8},    {F5, 8, 9},  {G5, 9, 10}, {A5, 10, 11}, {G5, 11, 12},
                    {F5, 12, 13}, {E5, 13, 14}, {D5, 14, 15}, {C5, 15, 16}, {G2, 16, 20}, {B4, 16, 18}, {G5, 18, 20}};
    const char *voices[] = {"left", "left", "left", "left2", "right", "right", "right", "right", "right", "right", "right", "right",
                            "right", "right", "right", "right", "right", "right", "right", "right", "left", "right", "right"};
    Piece piece;
    build_voiced(&piece, notes, voices, COUNT(notes), 2);
    bool beat_cell = false;
    for (size_t i = 1; i < piece.count; i++) {
        if (piece.cells[i].start == 12.0 * SIXTEENTH) {
            beat_cell = true;
            assert(piece.bass[i] == Fs3 && piece.bass[i - 1] == A3); /* the cell exists and the bass moved to the F# */
        }
    }
    assert(beat_cell);
    HarmonicSpans spans;
    group(&piece, &spans);
    assert(!span_starts_at(&piece, &spans, 12));
    harmonic_spans_destroy(&spans);
}

/* A passing bass is explained by its neighbours: C3 D3 E3 under a C major triad. D3 is a step from C3 and a step to E3, so it
   is not a new chord and the bar is one span. */
static void test_a_passing_bass_is_explained_by_its_neighbours(void) {
    Note notes[] = {{C3, 0, 4}, {D3, 4, 8}, {E3, 8, 16}, {E4, 0, 16}, {G4, 0, 16}, {C5, 0, 16}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 1);
    HarmonicSpans spans;
    group(&piece, &spans);
    int starts[] = {0};
    assert_span_starts(&piece, &spans, starts, COUNT(starts));
    harmonic_spans_destroy(&spans);
}

/* A chromatic bass without its chord tone is a new chord: C3 under a C major triad, then F#3 on the last beat under A4 C5 with
   no G sounding (the F# is no tone of the bar's chord and alters nothing that sounds), and a G2 in the next measure. The bar is
   cut at the last beat. */
static void test_a_chromatic_bass_without_its_chord_tone_is_cut(void) {
    Note notes[] = {{C3, 0, 12}, {E4, 0, 12}, {G4, 0, 12}, {C5, 0, 12}, {Fs3, 12, 16}, {A4, 12, 16}, {C5, 12, 16}, {G2, 16, 20}};
    Piece piece;
    build(&piece, notes, COUNT(notes), 2);
    HarmonicSpans spans;
    group(&piece, &spans);
    assert(span_starts_at(&piece, &spans, 12));
    harmonic_spans_destroy(&spans);
}

int main(void) {
    test_clementi_m1_is_one_span_over_c3();
    test_clementi_m8_scale_is_one_span_over_g2();
    test_mozart_m1_alberti_is_one_span();
    test_mozart_m2_splits_at_the_half_bar();
    test_a_passing_bass_eighth_does_not_split_a_beat();
    test_clementi_m4_descending_bass_line_is_one_span();
    test_the_bass_rule();
    test_mozart_m3_iv_then_i();
    test_mozart_m4_v7_then_i();
    test_mozart_m5_scale_over_f_is_one_span();
    test_mozart_m11_changes_chord_every_beat();
    test_mozart_m12_v_is_one_span();
    test_chorale_i_iv_v_i_is_four_spans();
    test_chorale_i_vi_ii_v_keeps_i_and_vi_apart();
    test_chorale_i_i6_iv_v_joins_the_inversion();
    test_a_pedal_does_not_hide_the_changes();
    test_chorale_with_bass_eighths_is_four_spans();
    test_a_chord_outside_the_key_is_read();
    test_a_beat_splits_at_its_half();
    test_an_accented_bass_tone_resolves_into_the_span_bass();
    test_two_bars_group_independently();
    test_a_root_and_fifth_over_the_root_is_a_span();
    test_an_accented_step_over_a_held_bass_is_flagged();
    test_a_chord_tone_that_steps_away_is_not_flagged();
    test_a_held_accent_into_the_window_is_flagged();
    test_a_bass_that_stops_before_the_resolution_flags_nothing();
    test_a_bass_that_ends_at_the_resolution_flags_nothing();
    test_marking_resolved_accents_refuses_bad_input_and_reads_nothing_without_voices();
    test_a_held_bass_note_is_not_cut();
    test_a_held_bass_under_a_changing_chord_is_one_span();
    test_a_lone_upper_note_is_no_new_bass();
    test_a_diminished_seventh_is_rooted_on_the_bass();
    test_a_chromatic_bass_against_its_chord_tone_is_not_cut();
    test_a_passing_bass_is_explained_by_its_neighbours();
    test_a_chromatic_bass_without_its_chord_tone_is_cut();
    test_bad_input_is_refused();
    printf("test_harmonic_spans: ok\n");
    return 0;
}
