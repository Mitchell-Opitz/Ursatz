#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "generation/conflict_detection.h"
#include "identity/entity_id.h"

/*
 * Construction test: ConflictDetection reports a conflict explicitly --
 * two or more conflicting requirement ids, a description, and
 * priority/severity/weight kept as three distinct fields (TDD Sec157),
 * never collapsed into one.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *conflicting_ids[2] = {make_id("constraint-vocal-range"), make_id("constraint-no-parallel-fifths")};

    ConflictDetection *conflict;
    assert(conflict_detection_create(conflicting_ids, 2, "vocal range constraint excludes every note satisfying the parallel-fifths avoidance constraint",
                                      1, CONFLICT_SEVERITY_HIGH, 3, 4, &conflict) == CONFLICT_DETECTION_OK);

    assert(conflict_detection_conflicting_id_count(conflict) == 2);
    assert(strcmp(entity_id_value(conflict_detection_conflicting_id_at(conflict, 0)), "constraint-vocal-range") ==
           0);
    assert(strcmp(entity_id_value(conflict_detection_conflicting_id_at(conflict, 1)),
                   "constraint-no-parallel-fifths") == 0);
    assert(strstr(conflict_detection_description(conflict), "vocal range") != NULL);
    assert(conflict_detection_priority(conflict) == 1);
    assert(conflict_detection_severity(conflict) == CONFLICT_SEVERITY_HIGH);
    assert(conflict_detection_weight_numerator(conflict) == 3);
    assert(conflict_detection_weight_denominator(conflict) == 4);

    /* priority (rank), severity (category), and weight (magnitude) are independently settable -- not synonyms. */
    EntityID *low_priority_high_severity_ids[2] = {make_id("constraint-a"), make_id("preference-b")};
    ConflictDetection *low_priority_high_severity;
    assert(conflict_detection_create(low_priority_high_severity_ids, 2, "low-priority but critical-severity conflict",
                                      99, CONFLICT_SEVERITY_CRITICAL, 1, 1,
                                      &low_priority_high_severity) == CONFLICT_DETECTION_OK);
    assert(conflict_detection_priority(low_priority_high_severity) == 99);
    assert(conflict_detection_severity(low_priority_high_severity) == CONFLICT_SEVERITY_CRITICAL);

    /* Rejects a single-id "conflict" -- a conflict is between multiple requirements. */
    EntityID *single_id[1] = {make_id("constraint-lonely")};
    ConflictDetection *rejected_single;
    assert(conflict_detection_create(single_id, 1, "not a real conflict", 0, CONFLICT_SEVERITY_LOW, 1, 1,
                                      &rejected_single) == CONFLICT_DETECTION_ERR_NULL_CONFLICTING_IDS);
    entity_id_destroy(single_id[0]);

    /* Rejects a NULL entry among the conflicting ids. */
    EntityID *bad_ids[2] = {make_id("constraint-ok"), NULL};
    ConflictDetection *rejected_null_entry;
    assert(conflict_detection_create(bad_ids, 2, "null entry", 0, CONFLICT_SEVERITY_LOW, 1, 1,
                                      &rejected_null_entry) == CONFLICT_DETECTION_ERR_NULL_CONFLICTING_ID);
    entity_id_destroy(bad_ids[0]);

    /* Rejects a zero weight denominator. */
    EntityID *zero_denom_ids[2] = {make_id("constraint-c"), make_id("constraint-d")};
    ConflictDetection *rejected_zero_denom;
    assert(conflict_detection_create(zero_denom_ids, 2, "zero denominator", 0, CONFLICT_SEVERITY_LOW, 1, 0,
                                      &rejected_zero_denom) == CONFLICT_DETECTION_ERR_NON_POSITIVE_WEIGHT_DENOMINATOR);
    entity_id_destroy(zero_denom_ids[0]);
    entity_id_destroy(zero_denom_ids[1]);

    conflict_detection_destroy(conflict);
    conflict_detection_destroy(low_priority_high_severity);

    printf("test_conflict_detection: ok\n");
    return 0;
}