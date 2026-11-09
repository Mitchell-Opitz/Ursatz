#include "analysis_phrase.h"
#include "analysis_phrase_grid.h"
#include "analysis_theme.h"
#include "form_test_util.h"
#include "phrase_level_form.h"
#include "time/meter_arithmetic.h"
#include <math.h>
#include <stdlib.h>

/* The phrase grid (R-389), phrases on it and themes on it (R-391): units of 4, 8 or 16 measures chosen by the evidence at their
   ends, built from notes, cadences and the MeterMap as test_analysis_phrase.c builds its inputs, 4/4. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

static const int A[4] = {60, 62, 64, 60};
static const int A_UP[4] = {65, 67, 69, 65};
static const int B[4] = {72, 67, 71, 64};
static const int C[4] = {69, 71, 72, 67};

/* A measure of quarters whose steps differ from every other measure's pair: no returning material unless a test asks for it. */
static void varied(int m, int pitches[4]) {
    for (int beat = 0; beat < 4; beat++) {
        pitches[beat] = 52 + (int)(((unsigned)m * 37u + (unsigned)beat * 11u + (unsigned)(m * beat) * 5u) % 23u);
    }
}

static void varied_measures(FormFixture *f, int first, int last) {
    for (int m = first; m <= last; m++) {
        int pitches[4];
        varied(m, pitches);
        form_measure(f, m, pitches);
    }
}

static StandardFormPhraseGrid find(const FormFixture *f) {
    StandardFormPhraseGrid grid;
    assert(standard_form_find_phrase_grid(&f->input, &grid) == STANDARD_FORM_ANALYSIS_OK);
    return grid;
}

static void assert_unit(const StandardFormPhraseGrid *grid, size_t index, int first, int counted_first, int last, int length,
                        bool elided, int half) {
    const StandardFormGridUnit *u = &grid->units[index];
    assert(u->first_measure == first && u->counted_first == counted_first && u->last_measure == last);
    assert(u->length_measures == length && u->elided == elided && u->half_measure == half);
}

static void test_clementis_shape_gives_its_five_units(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    for (int m = 1; m <= 38; m++) {
        int pitches[4];
        varied(m == 24 ? 1 : m == 25 ? 2 : m, pitches); /* the melody of m1 and m2 returns in m24 and m25 */
        if (m == 38) {
            pitches[3] = 0; /* a rest at the end of m38 */
        }
        if (m == 15 || m == 31) {
            double start = form_measure_start(&f, m);
            form_note(&f, start, 1.0, pitches[0]); /* a half note at the end of the measure */
            form_note(&f, start + 1.0, 1.0, pitches[1]);
            form_note(&f, start + 2.0, 2.0, pitches[2]);
        } else {
            form_measure(&f, m, pitches);
        }
    }
    form_cadence(&f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1); /* arrives on its downbeat: m8 may elide */
    form_cadence(&f, "hc19", 19, 0, 4, HC, 1, false, 0.1);
    form_cadence(&f, "hc23", 23, 0, 4, HC, 1, false, 0.1);
    form_cadence(&f, "pac24", 24, 0, 4, PAC, 3, false, 0.1);
    form_cadence(&f, "hc27", 27, 0, 4, HC, 1, false, 0.1);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    /* total 3.2: 1-8 (1.0 - 0.3 + half 0.8), 8-15 elided (0.3 - 0.3 - 0.1), 16-23 (1.1 - 0.3 + half 0.8), 24-31 (0.3 - 0.3 + half
       0.8), 32-38 (0.3 - 0.3 - 0.6) */
    assert(grid.count == 5);
    assert_unit(&grid, 0, 1, 1, 8, 8, false, 4);
    assert_unit(&grid, 1, 8, 8, 15, 8, true, 11);
    assert_unit(&grid, 2, 16, 16, 23, 8, false, 19);
    assert_unit(&grid, 3, 24, 24, 31, 8, false, 27);
    assert_unit(&grid, 4, 32, 32, 38, 7, false, -1);
    standard_form_phrase_grid_destroy(&grid);
    assert(grid.units == NULL && grid.count == 0);
    form_destroy(&f);
}

static void test_two_perfect_cadences_make_one_unit_of_sixteen(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    varied_measures(&f, 1, 16);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_cadence(&f, "pac16", 16, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    assert(grid.count == 1); /* 1.7 against 1.4 for 1-8 and 9-16 */
    assert_unit(&grid, 0, 1, 1, 16, 16, false, 8);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 2);
    const StandardFormPhraseClaim *a = &phrases.items[0].claim;
    const StandardFormPhraseClaim *b = &phrases.items[1].claim;
    assert(a->first_measure == 1 && a->end_measure == 8 && a->length_measures == 8 && a->cadence_measure == 8);
    assert(a->signal == (int)STANDARD_FORM_SIGNAL_CADENCE && a->cadence_type == (int)PAC);
    assert(b->first_measure == 9 && b->end_measure == 16 && b->length_measures == 8 && b->cadence_measure == 16);
    assert(b->signal == (int)STANDARD_FORM_SIGNAL_CADENCE && b->cadence_type == (int)PAC);
    standard_form_phrases_destroy(&phrases);
    standard_form_phrase_grid_destroy(&grid);
    form_destroy(&f);
}

static void test_a_tie_goes_to_the_earlier_final_start(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    varied_measures(&f, 1, 12);
    form_cadence(&f, "hc12", 12, 0, 4, HC, 1, false, 0.1);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    /* 1-4 and 5-12 tie with 1-8 and 9-12 at 0.2; the first reached wins (Mozart's m1 to m12 measured the same) */
    assert(grid.count == 2);
    assert_unit(&grid, 0, 1, 1, 4, 4, false, -1);
    assert_unit(&grid, 1, 5, 5, 12, 8, false, 8);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 1); /* m4 ends a unit with no cadence: inside the phrase */
    const StandardFormPhraseClaim *c = &phrases.items[0].claim;
    assert(c->first_measure == 1 && c->end_measure == 12 && c->length_measures == 12 && c->cadence_type == (int)HC);
    assert(c->signal == (int)STANDARD_FORM_SIGNAL_CADENCE);
    standard_form_phrases_destroy(&phrases);
    standard_form_phrase_grid_destroy(&grid);
    form_destroy(&f);
}

static void test_a_unit_end_without_a_cadence_ends_a_phrase_only_past_sixteen_measures(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    varied_measures(&f, 1, 36);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    assert(grid.count >= 3);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count >= 2);
    assert(phrases.items[0].claim.end_measure == 8 && phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_CADENCE);
    for (size_t i = 1; i < phrases.count; i++) {
        const StandardFormPhraseClaim *c = &phrases.items[i].claim;
        assert(c->length_measures <= 16 && c->first_measure == phrases.items[i - 1].claim.last_measure + 1);
        assert(c->signal == (int)STANDARD_FORM_SIGNAL_DISCONTINUITY && c->cadence_type == -1 && c->cadence_index == -1);
    }
    assert(phrases.items[phrases.count - 1].claim.end_measure == 36);
    standard_form_phrases_destroy(&phrases);
    standard_form_phrase_grid_destroy(&grid);
    form_destroy(&f);
}

typedef struct {
    FormFixture f;
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
} Period;

/* The period of test_analysis_theme.c: A A B B on a half cadence, A' A' C C on a perfect authentic one. */
static void period_init(Period *p) {
    memset(p, 0, sizeof(*p));
    form_init(&p->f, 4, 0);
    const int *m[8] = {A, A, B, B, A_UP, A_UP, C, C};
    for (int i = 0; i < 8; i++) {
        form_measure(&p->f, 1 + i, m[i]);
    }
    form_cadence(&p->f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&p->f, "pac8", 8, 0, 4, PAC, 3, false, 0.2);
    form_finish(&p->f);
}

static void period_phrases(Period *p, const StandardFormPhraseGrid *grid) {
    assert(standard_form_segment_phrases_on_grid(&p->f.input, NULL, grid, &p->phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&p->f.input, &p->phrases, NULL, &p->segmentation) == STANDARD_FORM_ANALYSIS_OK);
}

static void period_destroy(Period *p) {
    standard_form_segmentation_destroy(&p->segmentation);
    standard_form_phrases_destroy(&p->phrases);
    form_destroy(&p->f);
}

static void test_a_period_inside_one_unit_is_paired(void) {
    Period p;
    period_init(&p);
    StandardFormPhraseGrid grid = find(&p.f);
    assert(grid.count == 1); /* 1.5 against 1.2 for 1-4 and 5-8 */
    assert_unit(&grid, 0, 1, 1, 8, 8, false, 4);
    period_phrases(&p, &grid);
    assert(p.phrases.count == 2);
    assert(p.phrases.items[0].claim.end_measure == 4 && p.phrases.items[0].claim.length_measures == 4);
    assert(p.phrases.items[1].claim.first_measure == 5 && p.phrases.items[1].claim.end_measure == 8);
    StandardFormThemes themes;
    assert(standard_form_identify_themes_on_grid(&p.f.input, &p.phrases, &p.segmentation, &grid, NULL, &themes) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(themes.count == 1);
    const StandardFormThemeClaim *c = &themes.items[0].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_PERIOD && c->phrase_count == 2 && c->parts_are_phrases == 1);
    assert(c->first_part_last_measure == 4 && c->second_part_first_measure == 5 && c->second_part_last_measure == 8);
    standard_form_themes_destroy(&themes);
    standard_form_phrase_grid_destroy(&grid);
    period_destroy(&p);
}

static void test_two_phrases_across_two_units_are_not_paired(void) {
    Period p;
    period_init(&p);
    StandardFormGridUnit units[2] = {{1, 1, 4, 4, false, -1}, {5, 5, 8, 4, false, -1}};
    StandardFormPhraseGrid grid = {units, 2}; /* two units of 4: the antecedent ends on a unit's end, not on a half */
    period_phrases(&p, &grid);
    assert(p.phrases.count == 2 && p.phrases.items[0].claim.end_measure == 4 && p.phrases.items[1].claim.end_measure == 8);
    StandardFormThemes themes;
    assert(standard_form_identify_themes_on_grid(&p.f.input, &p.phrases, &p.segmentation, &grid, NULL, &themes) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(themes.count == 0 && themes.items == NULL);
    /* without a grid the same phrases are a period (standard_form_identify_themes is unchanged) */
    assert(standard_form_identify_themes(&p.f.input, &p.phrases, &p.segmentation, NULL, &themes) == STANDARD_FORM_ANALYSIS_OK);
    assert(themes.count == 1 && themes.items[0].claim.form == (int)PHRASE_LEVEL_FORM_PERIOD);
    standard_form_themes_destroy(&themes);
    period_destroy(&p);
}

static void test_a_pickup_measure_counts_in_no_length(void) {
    FormFixture f;
    form_init(&f, 4, 1);
    form_note(&f, 0.0, 1.0, 59); /* the pickup: one quarter note */
    varied_measures(&f, 1, 8);
    form_cadence(&f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    assert(meter_map_first_counted_measure(f.map) == 1);
    StandardFormPhraseGrid grid = find(&f);
    assert(grid.count == 1);
    assert_unit(&grid, 0, 0, 1, 8, 8, false, 4);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 2);
    const StandardFormPhraseClaim *a = &phrases.items[0].claim;
    assert(a->first_measure == 0 && a->counted_first == 1 && a->end_measure == 4 && a->length_measures == 4);
    assert(phrases.items[1].claim.first_measure == 5 && phrases.items[1].claim.length_measures == 4);
    standard_form_phrases_destroy(&phrases);
    standard_form_phrase_grid_destroy(&grid);
    form_destroy(&f);
}

static void test_no_notes_give_no_units(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    assert(grid.count == 0 && grid.units == NULL);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 0 && phrases.items == NULL);
    form_destroy(&f);

    form_init(&f, 4, 0); /* one measure holds no unit of two */
    varied_measures(&f, 1, 1);
    form_finish(&f);
    grid = find(&f);
    assert(grid.count == 0 && grid.units == NULL);
    form_destroy(&f);
}

static void test_a_last_note_held_into_the_next_measure_ends_the_last_phrase(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    varied_measures(&f, 1, 15);
    int pitches[4];
    varied(16, pitches);
    double start = form_measure_start(&f, 16);
    for (int beat = 0; beat < 3; beat++) {
        form_note(&f, start + (double)beat, 1.0, pitches[beat]);
    }
    form_note(&f, start + 3.0, 2.0, pitches[3]); /* held into m17, where nothing starts */
    form_cadence(&f, "hc12", 12, 0, 4, HC, 1, false, 0.1);
    form_finish(&f);
    StandardFormPhraseGrid grid = find(&f);
    assert(grid.count == 2); /* the grid ends at the last note start, m16 */
    assert_unit(&grid, 0, 1, 1, 8, 8, false, 4);
    assert_unit(&grid, 1, 9, 9, 16, 8, false, 12);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 2);
    const StandardFormPhraseClaim *a = &phrases.items[0].claim;
    const StandardFormPhraseClaim *b = &phrases.items[1].claim;
    assert(a->first_measure == 1 && a->end_measure == 12 && a->signal == (int)STANDARD_FORM_SIGNAL_CADENCE);
    assert(a->cadence_type == (int)HC);
    assert(b->first_measure == 13 && b->end_measure == 17 && b->last_measure == 17 && b->length_measures == 5);
    assert(b->signal == (int)STANDARD_FORM_SIGNAL_DISCONTINUITY && b->note_end > b->note_first);
    assert(fabs(phrases.items[1].uncertainty - 0.25) < 1e-9);
    standard_form_phrases_destroy(&phrases);
    standard_form_phrase_grid_destroy(&grid);
    form_destroy(&f);
}

static StandardFormGridUnit unit(int first, int last, bool elided, int half) {
    StandardFormGridUnit u = {first, first, last, last - first + 1, elided, half};
    return u;
}

/* The shape of Mozart 545's second development: the units of 57-101 (56-63e, 63-70e, 70-77e, 78-85, 86-101) tile the earlier span
   and 102-146 is written out again in other units. */
static StandardFormPhraseGrid mozart_shape(void) {
    StandardFormGridUnit shape[] = {unit(49, 56, false, 52),   unit(56, 63, true, 59),    unit(63, 70, true, 66),
                                    unit(70, 77, true, 73),    unit(78, 85, false, 81),   unit(86, 101, false, 93),
                                    unit(102, 103, false, -1), unit(104, 107, false, -1), unit(108, 115, false, 111),
                                    unit(115, 122, true, 118), unit(123, 130, false, 126), unit(131, 146, false, 138)};
    StandardFormPhraseGrid grid;
    grid.count = sizeof(shape) / sizeof(shape[0]);
    grid.units = (StandardFormGridUnit *)malloc(sizeof(shape));
    assert(grid.units != NULL);
    memcpy(grid.units, shape, sizeof(shape));
    return grid;
}

static void test_a_grid_follows_a_written_out_repeat(void) {
    StandardFormPhraseGrid grid = mozart_shape();
    StandardFormRepeat repeat = {STANDARD_FORM_REPEAT, 57, 101, 102, 146, 0};
    StandardFormRepeats repeats = {&repeat, 1};
    assert(standard_form_phrase_grid_follow_repeats(&grid, &repeats) == STANDARD_FORM_ANALYSIS_OK);
    /* 102-103, 104-107, 108-115, 115-122e, 123-130, 131-146 become the earlier units moved by 45 */
    assert(grid.count == 11);
    assert_unit(&grid, 5, 86, 86, 101, 16, false, 93);
    const StandardFormGridUnit *u = &grid.units[6];
    assert(u->first_measure == 101 && u->counted_first == 101 && u->last_measure == 108 && u->length_measures == 8 && u->elided &&
           u->half_measure == 104);
    assert_unit(&grid, 7, 108, 108, 115, 8, true, 111);
    assert_unit(&grid, 8, 115, 115, 122, 8, true, 118);
    assert_unit(&grid, 9, 123, 123, 130, 8, false, 126);
    assert_unit(&grid, 10, 131, 131, 146, 16, false, 138);
    /* a grid already equal is left equal */
    assert(standard_form_phrase_grid_follow_repeats(&grid, &repeats) == STANDARD_FORM_ANALYSIS_OK);
    assert(grid.count == 11);
    assert_unit(&grid, 10, 131, 131, 146, 16, false, 138);
    standard_form_phrase_grid_destroy(&grid);
}

static void test_a_repeat_the_grid_cannot_tile_leaves_it_alone(void) {
    StandardFormRepeat unaligned[] = {
        {STANDARD_FORM_REPEAT, 58, 101, 103, 146, 0},  /* no unit starts on 58 */
        {STANDARD_FORM_REPEAT, 57, 100, 102, 145, 0},  /* no unit ends on 100 */
        {STANDARD_FORM_REPEAT, 57, 101, 103, 146, 0},  /* no unit ends on 102 */
        {STANDARD_FORM_REPEAT, 57, 101, 102, 140, 0},  /* no unit ends on 140 */
        {STANDARD_FORM_RETURN, 57, 101, 102, 146, 0},  /* a return is ignored */
        {STANDARD_FORM_REPEAT, 57, 110, 102, 146, 0},  /* the spans overlap */
    };
    for (size_t i = 0; i < sizeof(unaligned) / sizeof(unaligned[0]); i++) {
        StandardFormPhraseGrid grid = mozart_shape();
        StandardFormRepeats repeats = {&unaligned[i], 1};
        assert(standard_form_phrase_grid_follow_repeats(&grid, &repeats) == STANDARD_FORM_ANALYSIS_OK);
        assert(grid.count == 12);
        assert_unit(&grid, 6, 102, 102, 103, 2, false, -1);
        assert_unit(&grid, 11, 131, 131, 146, 16, false, 138);
        standard_form_phrase_grid_destroy(&grid);
    }
    StandardFormPhraseGrid none = {NULL, 0};
    StandardFormRepeat repeat = {STANDARD_FORM_REPEAT, 57, 101, 102, 146, 0};
    StandardFormRepeats repeats = {&repeat, 1};
    assert(standard_form_phrase_grid_follow_repeats(&none, &repeats) == STANDARD_FORM_ANALYSIS_OK && none.count == 0);
    assert(standard_form_phrase_grid_follow_repeats(NULL, &repeats) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_phrase_grid_follow_repeats(&none, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
}

static void test_bad_calls_are_refused(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    varied_measures(&f, 1, 4);
    StandardFormPhraseGrid grid = {NULL, 0};
    StandardFormPhrases phrases;
    StandardFormThemes themes;
    StandardFormSegmentation segmentation;
    memset(&segmentation, 0, sizeof(segmentation));
    assert(standard_form_find_phrase_grid(&f.input, &grid) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT); /* not finished */
    form_finish(&f);
    assert(standard_form_find_phrase_grid(NULL, &grid) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_find_phrase_grid(&f.input, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_segment_phrases_on_grid(&f.input, NULL, NULL, &phrases) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_segment_phrases_on_grid(NULL, NULL, &grid, &phrases) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardFormConfig bad = standard_form_config_default();
    bad.similarity_min = 0.0;
    assert(standard_form_segment_phrases_on_grid(&f.input, &bad, &grid, &phrases) == STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    phrases.items = NULL;
    phrases.count = 0;
    assert(standard_form_identify_themes_on_grid(&f.input, &phrases, &segmentation, NULL, NULL, &themes) ==
           STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_identify_themes_on_grid(&f.input, &phrases, &segmentation, &grid, &bad, &themes) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    standard_form_phrase_grid_destroy(NULL);
    form_destroy(&f);
}

int main(void) {
    test_clementis_shape_gives_its_five_units();
    test_two_perfect_cadences_make_one_unit_of_sixteen();
    test_a_tie_goes_to_the_earlier_final_start();
    test_a_unit_end_without_a_cadence_ends_a_phrase_only_past_sixteen_measures();
    test_a_period_inside_one_unit_is_paired();
    test_two_phrases_across_two_units_are_not_paired();
    test_a_pickup_measure_counts_in_no_length();
    test_no_notes_give_no_units();
    test_a_last_note_held_into_the_next_measure_ends_the_last_phrase();
    test_a_grid_follows_a_written_out_repeat();
    test_a_repeat_the_grid_cannot_tile_leaves_it_alone();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_phrase_grid: ok\n");
    return 0;
}
