#include "analysis_prolongation.h"
#include "analysis_test_util.h"

/* Prolongations of tonic and dominant: a span from one T (or D) to the next, with every interior chord explained. */

static HarmonyProlongations run(Fixture *f, HarmonySegment **segments) {
    assert(harmony_analysis_prepare(f->inputs, f->count, segments) == HARMONY_ANALYSIS_OK);
    HarmonyProlongations found;
    assert(harmony_analysis_find_prolongations(*segments, f->count, &found) == HARMONY_PROLONGATION_OK);
    return found;
}

static void check_edge(const HarmonyProlongationSpan *span, size_t k, const char *source, const char *target,
                       HarmonyProlongationReason reason) {
    const Relationship *r = span->relationships[k];
    assert(strcmp(relationship_predicate_name(r), "prolongs") == 0);
    assert(strcmp(entity_id_value(relationship_source_id(r)), source) == 0);
    assert(strcmp(entity_id_value(relationship_target_id(r)), target) == 0);
    assert(span->reasons[k] == reason);
    /* The Interpretation targets the Relationship, not a segment. */
    const Interpretation *interpretation = span->interpretations[k];
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), entity_id_value(relationship_id(r))) == 0);
    HarmonyProlongationClaim claim;
    assert(harmony_prolongation_claim_read(interpretation, &claim));
    assert(claim.function == (int)span->function && claim.reason == (int)reason);
}

static void test_tonic_prolongation_through_a_passing_six_four(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);  /* I, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 2, 2, 2);  /* V6/4 over re, beat 2: passing */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 1);  /* I6, beat 3 */
    HarmonySegment *segments = NULL;
    HarmonyProlongations found = run(&f, &segments);
    assert(found.count == 1 && found.spans[0].function == HARMONIC_FUNCTION_TONIC);
    assert(found.spans[0].first_segment == 0 && found.spans[0].closing_segment == 2);
    assert(found.spans[0].interior_count == 1 && found.spans[0].interior_segments[0] == 1);
    check_edge(&found.spans[0], 0, "seg-0", "seg-1", HARMONY_PROLONGATION_REASON_PASSING_SIX_FOUR);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_tonic_line(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 1, 7, 2);  /* V6/5 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 2, 2, 7);  /* V4/3 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 1);       /* I6 */
    HarmonySegment *segments = NULL;
    HarmonyProlongations found = run(&f, &segments);
    /* I V6/5 I V4/3 I6 is a chain: two spans, the second starting where the first ends. */
    assert(found.count == 2);
    assert(found.spans[0].first_segment == 0 && found.spans[0].closing_segment == 2);
    assert(found.spans[1].first_segment == 2 && found.spans[1].closing_segment == 4);
    check_edge(&found.spans[0], 0, "seg-0", "seg-1", HARMONY_PROLONGATION_REASON_TONIC_LINE);
    check_edge(&found.spans[1], 0, "seg-2", "seg-3", HARMONY_PROLONGATION_REASON_TONIC_LINE);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_dominant_prolongation_through_a_neighbor_six_four(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V over sol, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);   /* I6/4 over sol, beat 3 (not weaker than...) */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 4 */
    HarmonySegment *segments = NULL;
    HarmonyProlongations found = run(&f, &segments);
    /* Beat 3 (medium) over the V on beat 4 (weak) is a stronger beat, so this is the cadential 6/4: no neighbor, no
       prolongation of V through it. */
    assert(segments[2].chord.cadential);
    assert(found.count == 0);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);

    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 (weak) */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);   /* I6/4, beat 3 (medium) */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);   /* the same 6/4, a second segment, beat 4 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, measure 2 beat 1 (strong): the 6/4 is weaker */
    found = run(&f, &segments);
    assert(!segments[2].chord.cadential);
    assert(found.count == 1 && found.spans[0].function == HARMONIC_FUNCTION_DOMINANT);
    assert(found.spans[0].first_segment == 1 && found.spans[0].closing_segment == 4);
    assert(found.spans[0].interior_count == 2); /* both segments of the 6/4 */
    check_edge(&found.spans[0], 0, "seg-1", "seg-2", HARMONY_PROLONGATION_REASON_NEIGHBOR_SIX_FOUR);
    check_edge(&found.spans[0], 1, "seg-1", "seg-3", HARMONY_PROLONGATION_REASON_NEIGHBOR_SIX_FOUR);
    HarmonyProlongationScope scope;
    size_t size = 0;
    const Value *value = relationship_scope(found.spans[0].relationships[0]);
    assert(strcmp(value_type(value), HARMONY_PROLONGATION_SCOPE_TYPE) == 0);
    memcpy(&scope, value_content(value, &size), sizeof(scope));
    assert(scope.function == (int)HARMONIC_FUNCTION_DOMINANT && strcmp(scope.closing_segment, "seg-4") == 0);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_dominant_prolongation_through_an_applied_chord(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);                   /* V */
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 2, 4)->applied_to = 5; /* V7/V, leading into V */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);                   /* V */
    HarmonySegment *segments = NULL;
    HarmonyProlongations found = run(&f, &segments);
    assert(found.count == 1 && found.spans[0].function == HARMONIC_FUNCTION_DOMINANT);
    check_edge(&found.spans[0], 0, "seg-0", "seg-1", HARMONY_PROLONGATION_REASON_APPLIED);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_no_prolongation_when_the_interior_is_not_explained(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV: a predominant, not explained */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* the same chord again: nothing between */
    HarmonySegment *segments = NULL;
    HarmonyProlongations found = run(&f, &segments);
    assert(found.count == 0 && found.spans == NULL);
    harmony_prolongations_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

int main(void) {
    test_tonic_prolongation_through_a_passing_six_four();
    test_tonic_line();
    test_dominant_prolongation_through_a_neighbor_six_four();
    test_dominant_prolongation_through_an_applied_chord();
    test_no_prolongation_when_the_interior_is_not_explained();
    printf("standard-harmony analysis prolongations: all tests passed\n");
    return 0;
}
