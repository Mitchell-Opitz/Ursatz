#include "analysis_repeats.h"
#include "form_test_util.h"

/* Written-out repeats and melodic returns over measures (R-400), built from notes in 4/4: a measure is a whole-note bass under four
   quarter-note melody notes; measure m (1-based, no pickup) starts at (m - 1) * 16 sixteenths. */

#define MAX_NOTES 512

typedef struct {
    MeterMap *map;
    TimeSpan *spans[MAX_NOTES];
    StandardFormRepeatNote notes[MAX_NOTES];
    size_t count;
} Piece;

static void piece_init(Piece *p) {
    memset(p, 0, sizeof(*p));
    assert(meter_map_create_defaulted(form_id("repeats-test-meter"), &p->map) == METER_MAP_OK);
}

static void piece_destroy(Piece *p) {
    for (size_t i = 0; i < p->count; i++) {
        time_span_destroy(p->spans[i]);
    }
    meter_map_destroy(p->map);
}

static void note(Piece *p, int measure, int start16, int length16, int pitch) {
    assert(p->count < MAX_NOTES);
    long start = (long)(measure - 1) * 16 + start16;
    p->spans[p->count] = form_span(start, start + length16);
    p->notes[p->count].span = p->spans[p->count];
    p->notes[p->count].semitone = pitch;
    p->count++;
}

/* A melody measure whose steps differ from every other measure's. */
static int melody_pitch(int id, int beat) {
    return 64 + (int)(((unsigned)id * 37u + (unsigned)beat * 11u + (unsigned)(id * beat) * 5u) % 23u);
}

/* `count` measures from measure `at`: melody ids from `melody_from` moved by `transpose`, bass ids from `bass_from`. */
static void block(Piece *p, int at, int melody_from, int bass_from, int count, int transpose) {
    for (int k = 0; k < count; k++) {
        note(p, at + k, 0, 16, 36 + 2 * ((bass_from + k) % 7));
        for (int beat = 0; beat < 4; beat++) {
            note(p, at + k, beat * 4, 4, melody_pitch(melody_from + k, beat) + transpose);
        }
    }
}

static StandardFormRepeats find(const Piece *p, const StandardFormRepeatConfig *config) {
    StandardFormRepeats out = {NULL, 0};
    assert(standard_form_find_repeats(p->notes, p->count, p->map, config, &out) == STANDARD_FORM_ANALYSIS_OK);
    return out;
}

static void assert_item(const StandardFormRepeats *r, size_t i, StandardFormRepeatKind kind, int ef, int el, int lf, int ll,
                        int transposition) {
    assert(i < r->count);
    const StandardFormRepeat *item = &r->items[i];
    assert(item->kind == kind && item->earlier_first == ef && item->earlier_last == el);
    assert(item->later_first == lf && item->later_last == ll && item->transposition == transposition);
}

static void test_an_exact_repeat_of_four_measures_is_found(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);
    block(&p, 5, 1, 1, 4, 0);
    block(&p, 9, 20, 20, 4, 0);
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 1);
    assert_item(&r, 0, STANDARD_FORM_REPEAT, 1, 4, 5, 8, 0);
    standard_form_repeats_destroy(&r);
    assert(r.items == NULL && r.count == 0);
    piece_destroy(&p);
}

static void test_a_repeat_of_three_measures_needs_a_shorter_minimum(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 3, 0);
    block(&p, 4, 60, 60, 1, 0);
    block(&p, 5, 1, 1, 3, 0);
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 0);
    standard_form_repeats_destroy(&r);
    StandardFormRepeatConfig three = {3};
    r = find(&p, &three);
    assert(r.count == 1);
    assert_item(&r, 0, STANDARD_FORM_REPEAT, 1, 3, 5, 7, 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_melody_an_octave_lower_over_the_same_bass_is_a_return(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);
    block(&p, 5, 40, 40, 3, 0);
    block(&p, 8, 50, 50, 1, 0);
    block(&p, 9, 1, 1, 4, -12); /* the melody of 1-4 an octave lower over the bass of 1-4 */
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 1);
    assert_item(&r, 0, STANDARD_FORM_RETURN, 1, 4, 9, 12, -12);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_transposed_return_carries_its_semitone_count(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 5, 0);
    block(&p, 6, 40, 40, 4, 0);
    block(&p, 10, 1, 30, 5, 5); /* up a fourth over another bass */
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 1);
    assert_item(&r, 0, STANDARD_FORM_RETURN, 1, 5, 10, 14, 5);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_return_implied_by_a_repeat_is_dropped(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);
    block(&p, 5, 1, 1, 4, 0);  /* a repeat of 1-4 */
    block(&p, 9, 1, 30, 4, 7); /* a return of 1-4 after both, over another bass */
    StandardFormRepeats r = find(&p, NULL);
    /* 5-8 to 9-12 is the same return from the repeat: only the return from the first statement is listed */
    assert(r.count == 2);
    assert_item(&r, 0, STANDARD_FORM_REPEAT, 1, 4, 5, 8, 0);
    assert_item(&r, 1, STANDARD_FORM_RETURN, 1, 4, 9, 12, 7);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_return_inside_a_repeats_later_span_is_dropped(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);   /* 1-4 */
    block(&p, 5, 1, 30, 4, 2);  /* a return of 1-4, over another bass */
    block(&p, 9, 40, 40, 4, 0); /* unrelated */
    block(&p, 13, 5, 5, 8, 0);  /* a source of eight */
    block(&p, 21, 5, 5, 8, 0);  /* and its repeat */
    block(&p, 29, 5, 31, 4, 3); /* the first half of the source again, a return from it and from its repeat */
    StandardFormRepeats r = find(&p, NULL);
    int returns_to_29 = 0;
    int returns_from_21 = 0;
    for (size_t i = 0; i < r.count; i++) {
        returns_to_29 += r.items[i].kind == STANDARD_FORM_RETURN && r.items[i].later_first == 29;
        returns_from_21 += r.items[i].kind == STANDARD_FORM_RETURN && r.items[i].earlier_first == 21;
    }
    assert(returns_to_29 == 1 && returns_from_21 == 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_overlapping_runs_are_refused(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 7, 7, 1, 0);
    for (int m = 2; m <= 5; m++) {
        block(&p, m, 7, 7, 1, 0); /* five equal measures: every run overlaps its source */
    }
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
    piece_init(&p);
    for (int m = 1; m <= 8; m++) {
        block(&p, m, 7, 7, 1, 0); /* eight equal measures: only 1-4 against 5-8 does not overlap */
    }
    r = find(&p, NULL);
    assert(r.count == 1);
    assert_item(&r, 0, STANDARD_FORM_REPEAT, 1, 4, 5, 8, 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_measure_with_no_notes_matches_nothing(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 2, 0);
    block(&p, 4, 1, 1, 1, 0); /* measure 3 rests */
    block(&p, 5, 1, 1, 2, 0);
    block(&p, 8, 1, 1, 1, 0); /* measure 7 rests */
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_a_returning_run_must_keep_one_semitone_count(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);
    block(&p, 5, 40, 40, 4, 0);
    block(&p, 9, 1, 30, 2, 5);
    block(&p, 11, 3, 32, 2, 7); /* the run of +5 stops after two measures */
    StandardFormRepeats r = find(&p, NULL);
    assert(r.count == 0);
    standard_form_repeats_destroy(&r);
    piece_destroy(&p);
}

static void test_bad_calls_are_refused(void) {
    Piece p;
    piece_init(&p);
    block(&p, 1, 1, 1, 4, 0);
    StandardFormRepeats out = {NULL, 0};
    StandardFormRepeatConfig zero = {0};
    assert(standard_form_find_repeats(NULL, p.count, p.map, NULL, &out) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_find_repeats(p.notes, p.count, NULL, NULL, &out) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_find_repeats(p.notes, p.count, p.map, NULL, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_find_repeats(p.notes, p.count, p.map, &zero, &out) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    const TimeSpan *kept = p.notes[3].span;
    p.notes[3].span = NULL;
    assert(standard_form_find_repeats(p.notes, p.count, p.map, NULL, &out) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    p.notes[3].span = kept;
    assert(out.items == NULL && out.count == 0);
    assert(standard_form_find_repeats(NULL, 0, p.map, NULL, &out) == STANDARD_FORM_ANALYSIS_OK && out.count == 0);
    standard_form_repeats_destroy(&out);
    standard_form_repeats_destroy(NULL);
    piece_destroy(&p);
}

int main(void) {
    test_an_exact_repeat_of_four_measures_is_found();
    test_a_repeat_of_three_measures_needs_a_shorter_minimum();
    test_a_melody_an_octave_lower_over_the_same_bass_is_a_return();
    test_a_transposed_return_carries_its_semitone_count();
    test_a_return_implied_by_a_repeat_is_dropped();
    test_a_return_inside_a_repeats_later_span_is_dropped();
    test_overlapping_runs_are_refused();
    test_a_measure_with_no_notes_matches_nothing();
    test_a_returning_run_must_keep_one_semitone_count();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_repeats: ok\n");
    return 0;
}
