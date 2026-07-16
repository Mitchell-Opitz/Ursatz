#include "score_test_util.h"

/* score_quantize: played times snapped to the beat grid, gaps absorbed, and its use by musicxml_export_write_score. */

static ScoreQuantized quantize(const TestBag *bag, const MeterMap *meter) {
    ScoreQuantized quantized;
    assert(score_quantize(bag->events, bag->count, bag->part, bag->staff, meter, NULL, &quantized) ==
           SCORE_QUANTIZE_OK);
    assert(quantized.count == bag->count);
    return quantized;
}

/* The note at index i starts at sn/sd and ends at en/ed (whole notes, reduced). */
static void expect_note(const ScoreQuantized *q, size_t i, unsigned long long sn, unsigned long long sd,
                        unsigned long long en, unsigned long long ed) {
    if (q->start_num[i] != sn || q->start_den[i] != sd || q->end_num[i] != en || q->end_den[i] != ed) {
        fprintf(stderr, "note %zu: got %llu/%llu - %llu/%llu, expected %llu/%llu - %llu/%llu\n", i, q->start_num[i],
                q->start_den[i], q->end_num[i], q->end_den[i], sn, sd, en, ed);
        abort();
    }
}

static void test_straight_beat(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 1, 100, 12, 100, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 13, 100, 12, 100, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "c", 26, 100, 11, 100, PITCH_LETTER_E, NULL);
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 1, 8);
    expect_note(&q, 1, 1, 8, 1, 4);
    expect_note(&q, 2, 1, 4, 3, 8);
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_triplet_beat(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 12, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 83, 1000, 1, 12, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "c", 167, 1000, 1, 12, PITCH_LETTER_E, NULL);
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 1, 12);
    expect_note(&q, 1, 1, 12, 1, 6);
    expect_note(&q, 2, 1, 6, 1, 4);
    score_quantized_destroy(&q);
    bag_free(&bag);

    /* The same beat with triplets not allowed snaps to the 32nd grid. */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "b", 1, 12, 1, 12, PITCH_LETTER_D, NULL);
    ScoreQuantizeOptions options = score_quantize_options_default();
    options.allow_triplets = false;
    ScoreQuantized straight;
    assert(score_quantize(bag.events, bag.count, bag.part, bag.staff, NULL, &options, &straight) == SCORE_QUANTIZE_OK);
    expect_note(&straight, 0, 3, 32, 5, 32);
    score_quantized_destroy(&straight);
    bag_free(&bag);
}

static void test_compound_beat_is_dotted(void) {
    /* 6/8: the beat is a dotted quarter, so three eighths are one beat of straight values. */
    const TuMeter six_eight[] = {{6, 8, 0, 1}};
    MeterMap *meter = tu_meter_map(six_eight, 1);
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 8, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 1, 8, 1, 8, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "c", 1, 4, 1, 8, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "d", 3, 8, 3, 8, PITCH_LETTER_F, NULL);
    ScoreQuantized q = quantize(&bag, meter);
    expect_note(&q, 2, 1, 4, 3, 8);
    expect_note(&q, 3, 3, 8, 3, 4);
    score_quantized_destroy(&q);
    bag_free(&bag);
    meter_map_destroy(meter);
}

static void test_gap_absorbed(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 3, 16, PITCH_LETTER_C, NULL); /* gap 1/16 is more than a quarter of 3/16 */
    bag_add_note(&bag, "b", 1, 4, 7, 32, PITCH_LETTER_D, NULL); /* gap 1/32 is within a quarter of 7/32 */
    bag_add_note(&bag, "c", 1, 2, 1, 8, PITCH_LETTER_E, NULL);
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 3, 16);
    expect_note(&q, 1, 1, 4, 1, 2);
    expect_note(&q, 2, 1, 2, 5, 8);
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_gap_of_a_beat_kept(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 1, PITCH_LETTER_C, NULL); /* a whole note allows a gap of a quarter ... */
    bag_add_note(&bag, "b", 5, 4, 1, 4, PITCH_LETTER_D, NULL); /* ... which here is a whole beat */
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 1, 1);
    expect_note(&q, 1, 5, 4, 3, 2);
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_gap_looks_only_inside_one_voice(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 3, 16, PITCH_LETTER_C, "v1");
    bag_add_note(&bag, "b", 1, 4, 3, 16, PITCH_LETTER_D, "v1");
    bag_add_note(&bag, "c", 7, 32, 1, 32, PITCH_LETTER_E, "v2");
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 3, 16); /* the onset of v2 at 7/32 is not v1's next onset */
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_chord_formed(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 1, 200, 24, 100, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 0, 1, 12, 50, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "c", 1, 100, 24, 100, PITCH_LETTER_G, NULL);
    ScoreQuantized q = quantize(&bag, NULL);
    for (size_t i = 0; i < 3; i++) {
        expect_note(&q, i, 0, 1, 1, 4);
    }
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_barline_kept(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 3, 4, 26, 100, PITCH_LETTER_C, NULL);    /* ends just past the barline */
    bag_add_note(&bag, "b", 199, 200, 1, 4, PITCH_LETTER_D, NULL);   /* starts just before it */
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 3, 4, 1, 1);
    assert(q.start_num[1] == 1 && q.start_den[1] == 1);
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_never_shorter_than_a_step(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 200, PITCH_LETTER_C, NULL);
    ScoreQuantized q = quantize(&bag, NULL);
    expect_note(&q, 0, 0, 1, 1, 32);
    score_quantized_destroy(&q);
    bag_free(&bag);
}

static void test_arguments(void) {
    ScoreQuantized q;
    assert(score_quantize(NULL, 1, NULL, NULL, NULL, NULL, &q) == SCORE_QUANTIZE_ERR_NULL_ARG);
    assert(score_quantize(NULL, 0, NULL, NULL, NULL, NULL, NULL) == SCORE_QUANTIZE_ERR_NULL_ARG);
    assert(score_quantize(NULL, 0, NULL, NULL, NULL, NULL, &q) == SCORE_QUANTIZE_OK);
    assert(q.count == 0);
    score_quantized_destroy(&q);
    score_quantized_destroy(&q);
    score_quantized_destroy(NULL);

    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    ScoreQuantizeOptions options = score_quantize_options_default();
    assert(options.grid_denominator == 32 && options.allow_triplets);
    assert(options.gap_absorb_numerator == 1 && options.gap_absorb_denominator == 4);
    options.grid_denominator = 0;
    assert(score_quantize(bag.events, bag.count, NULL, NULL, NULL, &options, &q) == SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE);
    assert(q.count == 0 && q.start_num == NULL);
    bag_free(&bag);
}

static char *write_score_xml(const TestBag *bag, const ScoreQuantizeOptions *quantize) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.quantize = quantize;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    return xml;
}

static void test_export_writes_notated_times(void) {
    ScoreQuantizeOptions options = score_quantize_options_default();

    /* Notes already on the grid: the document is byte-identical with and without quantizing. */
    TestBag exact;
    memset(&exact, 0, sizeof(exact));
    bag_add_note(&exact, "a", 0, 1, 1, 8, PITCH_LETTER_C, NULL);
    bag_add_note(&exact, "b", 1, 8, 1, 8, PITCH_LETTER_D, NULL);
    bag_add_note(&exact, "c", 1, 4, 1, 4, PITCH_LETTER_E, NULL);
    char *plain = write_score_xml(&exact, NULL);
    char *quantized_exact = write_score_xml(&exact, &options);
    assert(strcmp(plain, quantized_exact) == 0);

    /* The same notes played loosely: not quantizing keeps the played times, quantizing gives the exact document. */
    TestBag played;
    memset(&played, 0, sizeof(played));
    bag_add_note(&played, "a", 1, 100, 11, 100, PITCH_LETTER_C, NULL);
    bag_add_note(&played, "b", 13, 100, 11, 100, PITCH_LETTER_D, NULL);
    bag_add_note(&played, "c", 26, 100, 24, 100, PITCH_LETTER_E, NULL);
    char *loose = write_score_xml(&played, NULL);
    char *notated = write_score_xml(&played, &options);
    assert(strcmp(loose, plain) != 0);
    assert(strcmp(notated, plain) == 0);

    free(plain);
    free(quantized_exact);
    free(loose);
    free(notated);
    bag_free(&exact);
    bag_free(&played);
}

int main(void) {
    test_straight_beat();
    test_triplet_beat();
    test_compound_beat_is_dotted();
    test_gap_absorbed();
    test_gap_of_a_beat_kept();
    test_gap_looks_only_inside_one_voice();
    test_chord_formed();
    test_barline_kept();
    test_never_shorter_than_a_step();
    test_arguments();
    test_export_writes_notated_times();
    printf("score quantize tests passed\n");
    return 0;
}
