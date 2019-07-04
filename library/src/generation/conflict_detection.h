#ifndef MUSIC_GENERATION_CONFLICT_DETECTION_H
#define MUSIC_GENERATION_CONFLICT_DETECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * ConflictDetection (Concept, Registry #24): an explicitly reported
 * conflict between two or more constraints/preferences produced by
 * ConstraintExtraction (generation/constraint_extraction.h, TDD
 * Sec156). Conflicting requirements MUST NOT be silently dropped (TDD
 * Sec156) -- this type exists precisely so a conflict is a first-class,
 * inspectable record rather than a requirement quietly disappearing
 * from the executable constraint set during normalization.
 *
 * A Concept, not a Category: ConflictDetection carries no EntityID of
 * its own -- the same treatment FeatureExtraction (analysis/
 * feature_extraction.h) and ConstraintExtraction get. It is a value
 * describing what was found in conflict, not an identified object in
 * the canonical IR. The DETECTION algorithm that discovers conflicting
 * requirements is a generation-pipeline concern (music-generation, L8)
 * and out of scope for this pass, the same "stub, not logic" treatment
 * constraint_evaluate/preference_rank get -- this type only defines the
 * shape of what a detection reports.
 *
 * conflicting_ids is a required, owned array of EntityID references
 * (count >= 2 -- a conflict is between multiple requirements, never a
 * single one) to the conflicting Constraint/Preference objects
 * (rules/constraint.h, rules/preference.h) -- referenced by id only,
 * the same array-of-references-plus-count shape Fragmentation gives
 * extracted_event_ids (transform/fragmentation.h). Referencing by id
 * rather than embedding lets one conflict cite a Constraint and a
 * Preference together, or more than two requirements at once, without
 * this type needing to know which concrete kind each one is.
 *
 * description is a required, non-empty descriptor string explaining the
 * conflict in human-readable terms -- no dedicated ConflictDescription
 * type exists yet, the same placeholder treatment Transformation gives
 * its own dimension fields.
 *
 * priority, severity, and weight are kept as three DISTINCT fields, not
 * synonyms (TDD Sec157):
 *   - priority is the explicit resolution-order rank among competing
 *     conflicts (lower value resolves first); a plain ranking number,
 *     not a magnitude.
 *   - severity is how serious the conflict itself is, independent of
 *     resolution order -- a fixed vocabulary (TDD Sec157 does not
 *     require severity to be a rational fraction, only that it not be
 *     conflated with priority/weight).
 *   - weight is an exact rational fraction (numerator/denominator,
 *     denominator > 0, Law 19 -- never float/double), the same
 *     representation Constraint's WEIGHTED type and Evidence's strength
 *     use: how much this conflict should influence a decision, as a
 *     magnitude rather than a rank or a category.
 */

typedef enum {
    CONFLICT_SEVERITY_LOW = 0,
    CONFLICT_SEVERITY_MEDIUM,
    CONFLICT_SEVERITY_HIGH,
    CONFLICT_SEVERITY_CRITICAL
} ConflictSeverity;

typedef struct ConflictDetection ConflictDetection;

typedef enum {
    CONFLICT_DETECTION_OK = 0,
    CONFLICT_DETECTION_ERR_NULL_CONFLICTING_IDS,
    CONFLICT_DETECTION_ERR_NULL_CONFLICTING_ID,
    CONFLICT_DETECTION_ERR_NULL_DESCRIPTION,
    CONFLICT_DETECTION_ERR_NON_POSITIVE_WEIGHT_DENOMINATOR,
    CONFLICT_DETECTION_ERR_ALLOC
} ConflictDetectionResult;

/*
 * Takes ownership of conflicting_ids and each EntityID it points to on
 * success (destroyed by conflict_detection_destroy). conflicting_ids
 * must be non-NULL with conflicting_id_count >= 2; no entry may be
 * NULL. description is copied, required (non-empty). priority is an
 * unconstrained rank. severity is a required enum value. weight_
 * denominator must be > 0.
 */
ConflictDetectionResult conflict_detection_create(EntityID **conflicting_ids, size_t conflicting_id_count,
                                                   const char *description, unsigned long priority,
                                                   ConflictSeverity severity, unsigned long weight_numerator,
                                                   unsigned long weight_denominator, ConflictDetection **out);
void conflict_detection_destroy(ConflictDetection *conflict);

size_t conflict_detection_conflicting_id_count(const ConflictDetection *conflict);
const EntityID *conflict_detection_conflicting_id_at(const ConflictDetection *conflict, size_t index);
const char *conflict_detection_description(const ConflictDetection *conflict);
unsigned long conflict_detection_priority(const ConflictDetection *conflict);
ConflictSeverity conflict_detection_severity(const ConflictDetection *conflict);
unsigned long conflict_detection_weight_numerator(const ConflictDetection *conflict);
unsigned long conflict_detection_weight_denominator(const ConflictDetection *conflict);

#endif