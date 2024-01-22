#include <math.h>

#include "analysis_motion.h"
#include "analysis_test_util.h"

/* Motion between voice pairs: each classification is the existing rule's, as an Interpretation on a note-id shell. */

static MotionType motion_between(TestGrid *g, size_t segment, size_t upper, size_t lower, bool *found) {
    StandardCounterpointPreparedGrid prepared;
    assert(standard_counterpoint_grid_prepare(&g->grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointMotions motions;
    assert(standard_counterpoint_classify_motions(&prepared, &motions) == STANDARD_COUNTERPOINT_MOTION_OK);
    MotionType type = MOTION_TYPE_STATIC;
    *found = false;
    for (size_t i = 0; i < motions.count; i++) {
        if (motions.items[i].segment_from == segment && motions.items[i].upper_voice == upper &&
            motions.items[i].lower_voice == lower) {
            type = motions.items[i].type;
            *found = true;
        }
    }
    standard_counterpoint_motions_destroy(&motions);
    standard_counterpoint_grid_destroy(&prepared);
    return type;
}

static void test_the_four_motion_types_and_static(void) {
    /* two voices, top then bottom */
    const int rows[][TEST_GRID_MAX_VOICES] = {
        {72, 60}, {74, 62},   /* both up a step: parallel */
        {76, 64}, {72, 62},   /* upper down a third, lower holds... see below */
    };
    (void)rows;
    bool found;
    {
        const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 62}}; /* C5/C4 to D5/D4: same direction, same distance */
        TestGrid g;
        test_grid_init(&g, 2, 2, r);
        assert(motion_between(&g, 0, 0, 1, &found) == MOTION_TYPE_PARALLEL && found);
        test_grid_destroy(&g);
    }
    {
        const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {76, 62}}; /* upper up a third, lower up a step: similar */
        TestGrid g;
        test_grid_init(&g, 2, 2, r);
        assert(motion_between(&g, 0, 0, 1, &found) == MOTION_TYPE_SIMILAR);
        test_grid_destroy(&g);
    }
    {
        const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 59}}; /* up and down: contrary */
        TestGrid g;
        test_grid_init(&g, 2, 2, r);
        assert(motion_between(&g, 0, 0, 1, &found) == MOTION_TYPE_CONTRARY);
        test_grid_destroy(&g);
    }
    {
        const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 60}}; /* the lower holds: oblique */
        TestGrid g;
        test_grid_init(&g, 2, 2, r);
        assert(motion_between(&g, 0, 0, 1, &found) == MOTION_TYPE_OBLIQUE);
        test_grid_destroy(&g);
    }
    {
        const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {72, 60}}; /* neither moves: static */
        TestGrid g;
        test_grid_init(&g, 2, 2, r);
        assert(motion_between(&g, 0, 0, 1, &found) == MOTION_TYPE_STATIC);
        test_grid_destroy(&g);
    }
}

static void test_every_pair_of_consecutive_segments_and_rests(void) {
    /* three voices over three segments; the middle voice rests in the last */
    const int r[][TEST_GRID_MAX_VOICES] = {{72, 67, 60}, {74, 69, 62}, {76, 0, 64}};
    TestGrid g;
    test_grid_init(&g, 3, 3, r);
    StandardCounterpointPreparedGrid prepared;
    assert(standard_counterpoint_grid_prepare(&g.grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointMotions motions;
    assert(standard_counterpoint_classify_motions(&prepared, &motions) == STANDARD_COUNTERPOINT_MOTION_OK);
    /* segments 0-1: 3 pairs; segments 1-2: only the pair (0, 2), the middle voice resting */
    assert(motions.count == 4);
    const StandardCounterpointMotion *last = &motions.items[3];
    assert(last->segment_from == 1 && last->upper_voice == 0 && last->lower_voice == 2);
    standard_counterpoint_motions_destroy(&motions);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

static void test_interpretation_shell_and_uncertainty(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 62}};
    TestGrid g;
    test_grid_init(&g, 2, 2, r);
    g.cells[3].confidence = 0.6; /* the lower voice's second note */
    StandardCounterpointPreparedGrid prepared;
    assert(standard_counterpoint_grid_prepare(&g.grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointMotions motions;
    assert(standard_counterpoint_classify_motions(&prepared, &motions) == STANDARD_COUNTERPOINT_MOTION_OK);
    assert(motions.count == 1);
    const StandardCounterpointMotion *m = &motions.items[0];
    assert(fabs(m->uncertainty - 0.4) < 1e-12);
    assert(structure_event_id_count(m->shell) == 4);
    assert(strcmp(entity_id_value(interpretation_target_id(m->interpretation)),
                  entity_id_value(entity_id(structure_entity(m->shell)))) == 0);
    assert(interpretation_framework_reference(m->interpretation) != NULL);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(m->interpretation)) == 600000UL);
    StandardCounterpointMotionClaim claim;
    assert(standard_counterpoint_motion_claim_read(m->interpretation, &claim));
    assert(claim.motion_type == (int)MOTION_TYPE_PARALLEL && claim.upper_voice == 0 && claim.lower_voice == 1 &&
           claim.segment_from == 0 && claim.segment_to == 1);
    standard_counterpoint_motions_destroy(&motions);
    standard_counterpoint_grid_destroy(&prepared);
    test_grid_destroy(&g);
}

static void test_a_held_note_is_one_note_id_and_a_spelling_wins(void) {
    const int r[][TEST_GRID_MAX_VOICES] = {{72, 60}, {74, 60}};
    TestGrid g;
    test_grid_init(&g, 2, 2, r);
    StandardCounterpointPreparedGrid prepared;
    assert(standard_counterpoint_grid_prepare(&g.grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointMotions motions;
    assert(standard_counterpoint_classify_motions(&prepared, &motions) == STANDARD_COUNTERPOINT_MOTION_OK);
    assert(motions.items[0].type == MOTION_TYPE_OBLIQUE);
    assert(structure_event_id_count(motions.items[0].shell) == 3); /* the held lower note once */
    standard_counterpoint_motions_destroy(&motions);
    standard_counterpoint_grid_destroy(&prepared);

    /* A given spelling is used over the semitone: 61 spelled D-flat 4 is the letter D, not C-sharp. */
    PitchSpelling *d_flat = NULL;
    assert(pitch_spelling_create(PITCH_LETTER_D, -1, 4, &d_flat) == PITCH_SPELLING_OK);
    g.cells[2].spelling = d_flat; /* the upper voice, segment 1: D-flat */
    g.cells[2].pitch = NULL;
    assert(standard_counterpoint_grid_prepare(&g.grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    assert(pitch_spelling_letter(standard_counterpoint_grid_spelling(&prepared, 1, 0)) == PITCH_LETTER_D);
    assert(pitch_spelling_letter(standard_counterpoint_grid_spelling(&prepared, 0, 1)) == PITCH_LETTER_C);
    standard_counterpoint_grid_destroy(&prepared);
    pitch_spelling_destroy(d_flat);
    test_grid_destroy(&g);
}

static void test_a_bad_grid_is_rejected(void) {
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointVoiceGrid empty = {0, 0, NULL, NULL};
    assert(standard_counterpoint_grid_prepare(&empty, &prepared) == STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID);
    assert(standard_counterpoint_grid_prepare(NULL, &prepared) == STANDARD_COUNTERPOINT_GRID_ERR_NULL_ARGUMENT);
}

int main(void) {
    test_the_four_motion_types_and_static();
    test_every_pair_of_consecutive_segments_and_rests();
    test_interpretation_shell_and_uncertainty();
    test_a_held_note_is_one_note_id_and_a_spelling_wins();
    test_a_bad_grid_is_rejected();
    printf("standard-counterpoint analysis motion: all tests passed\n");
    return 0;
}
