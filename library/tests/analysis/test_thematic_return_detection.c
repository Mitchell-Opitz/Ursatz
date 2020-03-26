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

static Contour *make_contour_up_down(const char *p0, const char *p1, const char *p2, const char *p3) {
    PitchIdentity *pitch_ids[4] = {make_pitch(p0), make_pitch(p1), make_pitch(p2), make_pitch(p3)};
    ContourDirection directions[3] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_UP};
    Contour *contour;
    assert(contour_create(pitch_ids, 4, directions, 3, &contour) == CONTOUR_OK);
    return contour;
}

static Contour *make_contour_down_up(const char *p0, const char *p1, const char *p2, const char *p3) {
    PitchIdentity *pitch_ids[4] = {make_pitch(p0), make_pitch(p1), make_pitch(p2), make_pitch(p3)};
    ContourDirection directions[3] = {CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN};
    Contour *contour;
    assert(contour_create(pitch_ids, 4, directions, 3, &contour) == CONTOUR_OK);
    return contour;
}

static RhythmicPattern *make_rhythm(size_t count) {
    Duration *durations[4];
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

    EntityID *events_a[4] = {make_id("note-a-0"), make_id("note-a-1"), make_id("note-a-2"), make_id("note-a-3")};
    Contour *contour_a = make_contour_up_down("c\0", "e\0", "d\0", "f\0");
    RhythmicPattern *rhythm_a = make_rhythm(4);
    MotifWindow window_a = {contour_a, rhythm_a, (const EntityID *const *)events_a, 4};

    /* entries[1]: a contrasting middle section, matches neither a nor c. */
    EntityID *events_b[4] = {make_id("note-b-0"), make_id("note-b-1"), make_id("note-b-2"), make_id("note-b-3")};
    Contour *contour_b = make_contour_down_up("g\0", "e\0", "f\0", "d\0");
    RhythmicPattern *rhythm_b = make_rhythm(4);
    MotifWindow window_b = {contour_b, rhythm_b, (const EntityID *const *)events_b, 4};

    /* entries[2]: thematic return of entries[0]'s material, transposed. */
    EntityID *events_c[4] = {make_id("note-c-0"), make_id("note-c-1"), make_id("note-c-2"), make_id("note-c-3")};
    Contour *contour_c = make_contour_up_down("g\0", "b\0", "a\0", "c\0");
    RhythmicPattern *rhythm_c = make_rhythm(4);
    MotifWindow window_c = {contour_c, rhythm_c, (const EntityID *const *)events_c, 4};

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
    for (size_t i = 0; i < 4; i++) {
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

    /*
     * Over-matching regression (framework-v1-reference-set triage, Bach BWV Anh. 114):
     * below THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH (4), short/generic windows exact-match
     * across unrelated occurrences and used to produce a near-total pairwise "resembles" set.
     * Fixture: 2 genuinely-recurring 4-event themes (N=2 -> 1 resembles pair each = 2 pairs) and
     * 3 short (3-event, below threshold) occurrences (M=3) that are all pairwise identical to
     * each other and would, under the pre-fix exact-match rule with no length floor, also match.
     * Expect exactly the 2 N-derived pairs and nothing involving the M short occurrences.
     */
    EntityID *theme_occ_short_0 = make_id("occurrence-theme-short-0");
    EntityID *theme_occ_short_1 = make_id("occurrence-theme-short-1");
    EntityID *theme_occ_short_2 = make_id("occurrence-theme-short-2");
    EntityID *theme_occ_long_0 = make_id("occurrence-theme-long-0");
    EntityID *theme_occ_long_1 = make_id("occurrence-theme-long-1");
    EntityID *theme_occ_long_2 = make_id("occurrence-theme-long-2");
    EntityID *theme_occ_long_3 = make_id("occurrence-theme-long-3");

    /* M: three short, mutually-identical, below-threshold windows (3 events each). */
    EntityID *short_events[3][3];
    Contour *short_contours[3];
    RhythmicPattern *short_rhythms[3];
    MotifWindow short_windows[3];
    for (size_t i = 0; i < 3; i++) {
        char note_ids[3][32];
        for (size_t j = 0; j < 3; j++) {
            snprintf(note_ids[j], sizeof(note_ids[j]), "note-short-%zu-%zu", i, j);
            short_events[i][j] = make_id(note_ids[j]);
        }
        PitchIdentity *pitch_ids[3] = {make_pitch("c\0"), make_pitch("e\0"), make_pitch("d\0")};
        ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN};
        assert(contour_create(pitch_ids, 3, directions, 2, &short_contours[i]) == CONTOUR_OK);
        short_rhythms[i] = make_rhythm(3);
        short_windows[i] = (MotifWindow){short_contours[i], short_rhythms[i],
                                          (const EntityID *const *)short_events[i], 3};
    }

    /* N: two genuinely-recurring themes, each occurring twice, all at/above the 4-event minimum,
       distinct from each other and from the short windows' shape. */
    EntityID *long_events[4][4];
    Contour *long_contours[4];
    RhythmicPattern *long_rhythms[4];
    MotifWindow long_windows[4];
    const char *long_theme_a_pitches[4] = {"c\0", "e\0", "d\0", "g\0"};
    const char *long_theme_b_pitches[4] = {"a\0", "b\0", "c\0", "d\0"};
    for (size_t i = 0; i < 4; i++) {
        bool is_theme_a = i < 2;
        const char **pitches = is_theme_a ? long_theme_a_pitches : long_theme_b_pitches;
        char note_ids[4][32];
        PitchIdentity *pitch_ids[4];
        for (size_t j = 0; j < 4; j++) {
            snprintf(note_ids[j], sizeof(note_ids[j]), "note-long-%zu-%zu", i, j);
            long_events[i][j] = make_id(note_ids[j]);
            pitch_ids[j] = make_pitch(pitches[j]);
        }
        /* Theme A and Theme B use distinct direction sequences -- Contour equality is
           direction-only (transposition-invariant by construction), so two themes sharing a
           direction sequence would be indistinguishable regardless of pitch content. */
        ContourDirection directions[3];
        if (is_theme_a) {
            directions[0] = CONTOUR_DIRECTION_UP;
            directions[1] = CONTOUR_DIRECTION_DOWN;
            directions[2] = CONTOUR_DIRECTION_UP;
        } else {
            directions[0] = CONTOUR_DIRECTION_DOWN;
            directions[1] = CONTOUR_DIRECTION_UP;
            directions[2] = CONTOUR_DIRECTION_DOWN;
        }
        assert(contour_create(pitch_ids, 4, directions, 3, &long_contours[i]) == CONTOUR_OK);
        long_rhythms[i] = make_rhythm(4);
        long_windows[i] =
            (MotifWindow){long_contours[i], long_rhythms[i], (const EntityID *const *)long_events[i], 4};
    }

    ThematicReturnDetectionEntry regression_entries[7] = {
        {theme_occ_short_0, &short_windows[0]}, {theme_occ_long_0, &long_windows[0]},
        {theme_occ_short_1, &short_windows[1]}, {theme_occ_long_1, &long_windows[1]},
        {theme_occ_short_2, &short_windows[2]}, {theme_occ_long_2, &long_windows[2]},
        {theme_occ_long_3, &long_windows[3]},
    };

    Relationship **regression_relationships;
    size_t regression_relationship_count;
    assert(thematic_return_detection_build_relationships(regression_entries, 7, &regression_relationships,
                                                           &regression_relationship_count) ==
           THEMATIC_RETURN_DETECTION_REL_OK);
    assert(regression_relationship_count == 2);
    for (size_t i = 0; i < regression_relationship_count; i++) {
        assert(strstr(entity_id_value(relationship_source_id(regression_relationships[i])), "long") != NULL);
        assert(strstr(entity_id_value(relationship_target_id(regression_relationships[i])), "long") != NULL);
    }
    thematic_return_detection_destroy_relationships(regression_relationships, regression_relationship_count);

    entity_id_destroy(theme_occ_short_0);
    entity_id_destroy(theme_occ_short_1);
    entity_id_destroy(theme_occ_short_2);
    entity_id_destroy(theme_occ_long_0);
    entity_id_destroy(theme_occ_long_1);
    entity_id_destroy(theme_occ_long_2);
    entity_id_destroy(theme_occ_long_3);
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            entity_id_destroy(short_events[i][j]);
        }
        contour_destroy(short_contours[i]);
        rhythmic_pattern_destroy(short_rhythms[i]);
    }
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
            entity_id_destroy(long_events[i][j]);
        }
        contour_destroy(long_contours[i]);
        rhythmic_pattern_destroy(long_rhythms[i]);
    }

    printf("test_thematic_return_detection: ok\n");
    return 0;
}
