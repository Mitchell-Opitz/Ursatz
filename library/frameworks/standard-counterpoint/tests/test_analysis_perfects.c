#include <math.h>

#include "analysis_perfects.h"
#include "analysis_test_util.h"

/* Parallel and direct perfect intervals: the existing rules, read across a grid of separated voices. */

static StandardCounterpointViolations check(TestGrid *g, StandardCounterpointPreparedGrid *prepared) {
    assert(standard_counterpoint_grid_prepare(&g->grid, prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointViolations found;
    assert(standard_counterpoint_check_perfects(prepared, &found) == STANDARD_COUNTERPOINT_PERFECTS_OK);
    return found;
}

static size_t count_of(const StandardCounterpointViolations *v, StandardCounterpointViolationRule rule) {
    size_t n = 0;
    for (size_t i = 0; i < v->count; i++) {
        n += v->items[i].rule == rule ? 1 : 0;
    }
    return n;
}

static void expect(const int rows[][TEST_GRID_MAX_VOICES], size_t segments, size_t voices, size_t fifths,
                   size_t octaves, size_t directs) {
    TestGrid g;
    test_grid_init(&g, segments, voices, rows);
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointViolations found = check(&g, &prepared);
    assert(count_of(&found, STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_FIFTHS) == fifths);
    assert(count_of(&found, STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_OCTAVES) == octaves);
    assert(count_of(&found, STANDARD_COUNTERPOINT_VIOLATION_DIRECT_PERFECTS) == directs);
    assert(found.count == fifths + octaves + directs);
    standard_counterpoint_violations_destroy(&found);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

static void test_parallel_fifths(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{67, 60}, {69, 62}}; /* G4/C4 to A4/D4 */
    expect(r, 2, 2, 1, 0, 0);
    const int compound[][TEST_GRID_MAX_VOICES] = {{79, 60}, {81, 62}}; /* a twelfth to a twelfth */
    expect(compound, 2, 2, 1, 0, 0);
}

static void test_parallel_octaves_and_unisons(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 62}};
    expect(r, 2, 2, 0, 1, 0);
    const int compound[][TEST_GRID_MAX_VOICES] = {{84, 60}, {86, 62}}; /* two octaves */
    expect(compound, 2, 2, 0, 1, 0);
    const int unisons[][TEST_GRID_MAX_VOICES] = {{60, 60}, {62, 62}};
    expect(unisons, 2, 2, 0, 1, 0);
}

static void test_a_repeated_fifth_is_not_motion(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{67, 60}, {67, 60}}; /* the same pitches again */
    expect(r, 2, 2, 0, 0, 0);
    const int one_moves[][TEST_GRID_MAX_VOICES] = {{67, 60}, {69, 60}}; /* oblique */
    expect(one_moves, 2, 2, 0, 0, 0);
    const int contrary[][TEST_GRID_MAX_VOICES] = {{67, 60}, {71, 58}}; /* a fifth, then a twelfth... by contrary motion */
    expect(contrary, 2, 2, 0, 0, 0);
}

static void test_direct_octave_in_the_outer_voices(void) {
    /* E5/G4 (a sixth) to C5/C4 (an octave): similar motion, the soprano leaps a third: flagged. */
    const int leap[][TEST_GRID_MAX_VOICES] = {{76, 67}, {72, 60}};
    expect(leap, 2, 2, 0, 0, 1);
    /* D5/G4 (a fifth) to C5/C4 (an octave): similar motion, the soprano steps: not flagged. */
    const int step[][TEST_GRID_MAX_VOICES] = {{74, 67}, {72, 60}};
    expect(step, 2, 2, 0, 0, 0);
}

static void test_direct_fifth_in_inner_voices_is_not_flagged(void) {
    /* Four voices; the tenor and alto (voices 1 and 2) arrive at a fifth by similar motion with a leap in the alto's
       place; the outer voices move by contrary motion into a sixth. */
    const int r[][TEST_GRID_MAX_VOICES] = {{76, 71, 62, 48}, {74, 67, 60, 52}};
    TestGrid g;
    test_grid_init(&g, 2, 4, r);
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointViolations found = check(&g, &prepared);
    assert(count_of(&found, STANDARD_COUNTERPOINT_VIOLATION_DIRECT_PERFECTS) == 0);
    standard_counterpoint_violations_destroy(&found);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

static void test_parallels_are_found_in_any_pair_and_rests_are_skipped(void) {
    /* three voices: the middle and the bottom make parallel fifths; the top voice rests in the second segment */
    const int r[][TEST_GRID_MAX_VOICES] = {{76, 67, 60}, {0, 69, 62}};
    TestGrid g;
    test_grid_init(&g, 2, 3, r);
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointViolations found = check(&g, &prepared);
    assert(found.count == 1 && found.items[0].rule == STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_FIFTHS);
    assert(found.items[0].upper_voice == 1 && found.items[0].lower_voice == 2);
    standard_counterpoint_violations_destroy(&found);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

static void test_output(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{67, 60}, {69, 62}};
    TestGrid g;
    test_grid_init(&g, 2, 2, r);
    g.cells[1].confidence = 0.7;
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointViolations found = check(&g, &prepared);
    assert(found.count == 1);
    const StandardCounterpointViolation *v = &found.items[0];
    assert(fabs(v->uncertainty - 0.3) < 1e-12 && v->arrival_interval == 5);
    assert(structure_event_id_count(v->shell) == 4);
    assert(strcmp(entity_id_value(interpretation_target_id(v->interpretation)),
                  entity_id_value(entity_id(structure_entity(v->shell)))) == 0);
    assert(interpretation_framework_reference(v->interpretation) != NULL);
    StandardCounterpointViolationClaim claim;
    assert(standard_counterpoint_violation_claim_read(v->interpretation, &claim));
    assert(strcmp(claim.rule_name, "parallel-fifths") == 0 && claim.upper_voice == 0 && claim.lower_voice == 1 &&
           claim.segment_from == 0 && claim.segment_to == 1 && claim.arrival_interval == 5);
    assert(strcmp(standard_counterpoint_violation_rule_name(STANDARD_COUNTERPOINT_VIOLATION_DIRECT_PERFECTS),
                  "direct-perfects") == 0);
    standard_counterpoint_violations_destroy(&found);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

int main(void) {
    test_parallel_fifths();
    test_parallel_octaves_and_unisons();
    test_a_repeated_fifth_is_not_motion();
    test_direct_octave_in_the_outer_voices();
    test_direct_fifth_in_inner_voices_is_not_flagged();
    test_parallels_are_found_in_any_pair_and_rests_are_skipped();
    test_output();
    printf("standard-counterpoint analysis perfects: all tests passed\n");
    return 0;
}
