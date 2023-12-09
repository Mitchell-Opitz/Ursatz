#include "analysis_six_four.h"
#include "analysis_test_util.h"

/* 6/4 chords: cadential, passing, neighbor, arpeggiating, and one that matches none. The fixture's segments are
   quarter notes in 4/4: index 0 is beat 1 (strong), 1 beat 2 (weak), 2 beat 3 (medium), 3 beat 4 (weak). */

static HarmonySixFourResult *classify(Fixture *f, HarmonySegment **segments) {
    assert(harmony_analysis_prepare(f->inputs, f->count, segments) == HARMONY_ANALYSIS_OK);
    HarmonySixFourResult *results = NULL;
    assert(harmony_analysis_classify_six_fours(*segments, f->count, &results) == HARMONY_SIX_FOUR_OK);
    return results;
}

static void expect(const HarmonySixFourResult *r, SixFourType type) {
    assert(r->is_six_four && r->classified && r->type == type && r->interpretation != NULL);
    HarmonySixFourClaim claim;
    assert(harmony_six_four_claim_read(r->interpretation, &claim) && claim.type == (int)type);
}

static void test_cadential(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);  /* IV */
    fixture_add(&f, 2, TRIAD_MINOR, 3, 1, 4, 6);  /* ii6 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);  /* I6/4 on beat 3 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);  /* V on beat 4: weaker, same bass */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    assert(segments[2].chord.cadential);
    expect(&results[2], SIX_FOUR_TYPE_CADENTIAL);
    assert(!results[3].is_six_four);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_i64_on_a_weaker_beat_than_the_v_is_not_cadential(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 1, 6, 6);
    fixture_add(&f, 2, TRIAD_MINOR, 3, 1, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);  /* I6/4 on beat 4 (weak) */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);  /* V on beat 1 of the next measure (strong) */
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    assert(!segments[3].chord.cadential);
    assert(results[3].is_six_four && !results[3].classified && results[3].interpretation == NULL);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_passing(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);  /* I, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 2, 2, 2);  /* V6/4 over re, beat 2 (weak) */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 1);  /* I6 */
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    expect(&results[1], SIX_FOUR_TYPE_PASSING);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_neighbor(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);  /* I over do */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 2, 1, 4);  /* IV6/4 over do: the upper voices move up and back */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);  /* I over do */
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    expect(&results[1], SIX_FOUR_TYPE_NEIGHBOR);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_arpeggiating(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);  /* I over do, soprano mi */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);  /* I6/4: the bass leaps to sol, the soprano holds */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    expect(&results[1], SIX_FOUR_TYPE_ARPEGGIATING);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_unclassified_and_runs(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 2, TRIAD_MINOR, 3, 0, 2, 4);  /* ii */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);  /* I6/4 (two segments): leaps in, leaps out, and is not cadential */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);
    fixture_add(&f, 6, TRIAD_MINOR, 3, 0, 6, 1);  /* vi */
    HarmonySegment *segments = NULL;
    HarmonySixFourResult *results = classify(&f, &segments);
    for (int i = 1; i <= 2; i++) {
        assert(results[i].is_six_four && !results[i].classified && results[i].interpretation == NULL);
    }
    assert(!results[0].is_six_four && !results[3].is_six_four);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);

    /* A cadential 6/4 over two segments, on beats equal to the V's (weak, weak): both segments are classified. */
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);  /* beat 2: weak */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);  /* beat 4: weak, the same strength */
    results = classify(&f, &segments);
    assert(segments[1].chord.cadential && segments[2].chord.cadential);
    expect(&results[1], SIX_FOUR_TYPE_CADENTIAL);
    expect(&results[2], SIX_FOUR_TYPE_CADENTIAL);
    harmony_six_four_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

int main(void) {
    test_cadential();
    test_i64_on_a_weaker_beat_than_the_v_is_not_cadential();
    test_passing();
    test_neighbor();
    test_arpeggiating();
    test_unclassified_and_runs();
    printf("standard-harmony analysis six-fours: all tests passed\n");
    return 0;
}
