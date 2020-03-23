#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/thematic_return_detection.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "pitch/pitch_identity.h"
#include "relationship/relationship.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"

/*
 * Behavioral-contract test: NULL input is rejected without touching *out,
 * and fewer than two entries is a valid, empty AnalysisResult /
 * relationship set (no pair to compare).
 *
 * Analysis-regression test: three ThemeOccurrence windows where entries[0]
 * and entries[2] share an identical Contour+RhythmicPattern (a thematic
 * return) and entries[1] does not match either -- pins the resulting
 * "resembles" Relationship (later -> earlier) and Interpretation shape
 * against a future refactor, and confirms a non-matching entry produces
 * no output (requirement 4).
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    return identity;
}

static Contour *make_contour_up_down(const char *p0, const char *p1, const char *p2) {
    PitchIdentity *pitch_ids[3] = {make_pitch(p0), make_pitch(p1), make_pitch(p2)};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static Contour *make_contour_down_up(const char *p0, const char *p1, const char *p2) {
    PitchIdentity *pitch_ids[3] = {make_pitch(p0), make_pitch(p1), make_pitch(p2)};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_UP};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static RhythmicPattern *make_rhythm(size_t count) {
    Duration *durations[3];
    for (size_t i = 0; i < count; i++) {
        assert(duration_create(1, 4, &durations[i]) == DURATION_OK);
    }
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, count, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(thematic_return_detection_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(THEMATIC_RETURN_DETECTION_ANALYZER.name, "ThematicReturnDetection") == 0);

    /* Behavioral contract: 0 or 1 entries is valid and empty (no pair to compare). */
    ThematicReturnDetectionInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(thematic_return_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    Relationship **empty_relationships;
    size_t empty_relationship_count;
    assert(thematic_return_detection_build_relationships(NULL, 0, &empty_relationships, &empty_relationship_count) ==
           THEMATIC_RETURN_DETECTION_REL_OK);
    assert(empty_relationships == NULL);
    assert(empty_relationship_count == 0);

    EntityID *theme_occ_a = make_id("occurrence-theme-a");
    EntityID *theme_occ_b = make_id("occurrence-theme-b");
    EntityID *theme_occ_c = make_id("occurrence-theme-c");

    EntityID *events_a[3] = {make_id("note-a-0"), make_id("note-a-1"), make_id("note-a-2")};
    Contour *contour_a = make_contour_up_down("c\0", "e\0", "d\0");
    RhythmicPattern *rhythm_a = make_rhythm(3);
    MotifWindow window_a = {contour_a, rhythm_a, (const EntityID *const *)events_a, 3};

    /* entries[1]: a contrasting middle section, matches neither a nor c. */
    EntityID *events_b[3] = {make_id("note-b-0"), make_id("note-b-1"), make_id("note-b-2")};
    Contour *contour_b = make_contour_down_up("g\0", "e\0", "f\0");
    RhythmicPattern *rhythm_b = make_rhythm(3);
    MotifWindow window_b = {contour_b, rhythm_b, (const EntityID *const *)events_b, 3};

    /* entries[2]: thematic return of entries[0]'s material, transposed. */
    EntityID *events_c[3] = {make_id("note-c-0"), make_id("note-c-1"), make_id("note-c-2")};
    Contour *contour_c = make_contour_up_down("g\0", "b\0", "a\0");
    RhythmicPattern *rhythm_c = make_rhythm(3);
    MotifWindow window_c = {contour_c, rhythm_c, (const EntityID *const *)events_c, 3};

    ThematicReturnDetectionEntry entries[3] = {
        {theme_occ_a, &window_a},
        {theme_occ_b, &window_b},
        {theme_occ_c, &window_c},
    };
    ThematicReturnDetectionInput input = {entries, 3};

    AnalysisResult *result;
    assert(thematic_return_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)),
                   "relationship-thematic-return-occurrence-theme-c-occurrence-theme-a") == 0);
    size_t claim_size;
    const ThematicReturnDetectionClaim *claim =
        (const ThematicReturnDetectionClaim *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(ThematicReturnDetectionClaim));
    assert(strcmp(claim->later_theme_occurrence_id, "occurrence-theme-c") == 0);
    assert(strcmp(claim->earlier_theme_occurrence_id, "occurrence-theme-a") == 0);
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 1);
    assert(strcmp(interpretation_author(interpretation), "ThematicReturnDetection") == 0);

    analysis_result_destroy(result);

    /* thematic_return_detection_build_relationships mirrors the same single "resembles" link. */
    Relationship **relationships;
    size_t relationship_count;
    assert(thematic_return_detection_build_relationships(entries, 3, &relationships, &relationship_count) ==
           THEMATIC_RETURN_DETECTION_REL_OK);
    assert(relationship_count == 1);
    assert(strcmp(entity_id_value(relationship_source_id(relationships[0])), "occurrence-theme-c") == 0);
    assert(strcmp(relationship_predicate_name(relationships[0]), "resembles") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationships[0])), "occurrence-theme-a") == 0);
    thematic_return_detection_destroy_relationships(relationships, relationship_count);

    entity_id_destroy(theme_occ_a);
    entity_id_destroy(theme_occ_b);
    entity_id_destroy(theme_occ_c);
    for (size_t i = 0; i < 3; i++) {
        entity_id_destroy(events_a[i]);
        entity_id_destroy(events_b[i]);
        entity_id_destroy(events_c[i]);
    }
    contour_destroy(contour_a);
    contour_destroy(contour_b);
    contour_destroy(contour_c);
    rhythmic_pattern_destroy(rhythm_a);
    rhythmic_pattern_destroy(rhythm_b);
    rhythmic_pattern_destroy(rhythm_c);

    printf("test_thematic_return_detection: ok\n");
    return 0;
}
