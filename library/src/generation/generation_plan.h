#ifndef MUSIC_GENERATION_GENERATION_PLAN_H
#define MUSIC_GENERATION_GENERATION_PLAN_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "generation/constraint_extraction.h"
#include "generation/intent.h"
#include "identity/entity_id.h"

/*
 * GenerationPlan (Entity, Registry #25): the structured plan that feeds
 * a Generator (generation/generator.h, a later commit) -- Intent +
 * Constraints + Preferences, assembled into a single object a Generator
 * can consume without re-deriving anything.
 *
 * intent is a required, OWNED Intent (generation/intent.h). Unlike
 * Fragmentation's source_structure_id, which references an existing
 * Structure by id (transform/fragmentation.h) because Structure has a
 * store elsewhere in the canonical IR, Intent has no such store at this
 * layer -- the same "nowhere else for it to live" situation
 * AnalysisResult's owned Interpretations/Evidences are in (analysis/
 * analyzer.h). A GenerationPlan is the one place the Intent that
 * produced it persists.
 *
 * constraints is a required, OWNED ConstraintExtraction (generation/
 * constraint_extraction.h) -- the executable constraint/preference set
 * normalized from intent. Required, not optional: even an Intent with
 * no stated constraints/preferences still normalizes to a valid, empty
 * ConstraintExtraction (see that type's own header) -- a GenerationPlan
 * always carries the result of having run extraction, never a NULL
 * standing in for "extraction wasn't attempted."
 *
 * ConflictDetection (generation/conflict_detection.h) is deliberately
 * NOT a field here: the Registry describes GenerationPlan's fields as
 * exactly Intent + Constraints + Preferences (Registry #25); conflict
 * reports are a diagnostic produced while assembling those constraints,
 * not part of the plan a Generator consumes.
 *
 * provenance is a required ProvenanceReference: every GenerationPlan
 * records what assembled it (typically the extraction/normalization
 * pipeline that produced constraints from intent).
 */

typedef struct GenerationPlan GenerationPlan;

typedef enum {
    GENERATION_PLAN_OK = 0,
    GENERATION_PLAN_ERR_NULL_ID,
    GENERATION_PLAN_ERR_NULL_INTENT,
    GENERATION_PLAN_ERR_NULL_CONSTRAINTS,
    GENERATION_PLAN_ERR_NULL_PROVENANCE,
    GENERATION_PLAN_ERR_ALLOC
} GenerationPlanResult;

/*
 * Takes ownership of id, intent, constraints, and provenance on success
 * (all required, destroyed by generation_plan_destroy).
 */
GenerationPlanResult generation_plan_create(EntityID *id, Intent *intent, ConstraintExtraction *constraints,
                                             ProvenanceReference *provenance, GenerationPlan **out);
void generation_plan_destroy(GenerationPlan *plan);

const EntityID *generation_plan_id(const GenerationPlan *plan);
const Intent *generation_plan_intent(const GenerationPlan *plan);
const ConstraintExtraction *generation_plan_constraints(const GenerationPlan *plan);
const ProvenanceReference *generation_plan_provenance(const GenerationPlan *plan);

bool generation_plan_identity_equals(const GenerationPlan *a, const GenerationPlan *b);

#endif