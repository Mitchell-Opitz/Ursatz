#ifndef MUSIC_GENERATION_CONSTRAINT_EXTRACTION_H
#define MUSIC_GENERATION_CONSTRAINT_EXTRACTION_H

#include <stddef.h>

#include "rules/constraint.h"
#include "rules/preference.h"

/*
 * ConstraintExtraction (Concept, Registry #23): the executable
 * constraint set an Intent (generation/intent.h) normalizes into (TDD
 * Sec155) -- Intent's raw, pre-extraction constraints/preferences
 * (opaque Value payloads, uninterpreted) turned into actual L7
 * Constraint (rules/constraint.h) and Preference (rules/preference.h)
 * objects a Generator can evaluate against.
 *
 * A Concept, not a Category: ConstraintExtraction carries no EntityID
 * of its own -- the same treatment FeatureExtraction gets (analysis/
 * feature_extraction.h, Registry #13). It is a value describing what an
 * extraction produced, not an identified object in the canonical IR.
 * The extraction/normalization ALGORITHM that turns Intent's opaque
 * Values into concrete Constraint/Preference objects is a
 * generation-pipeline concern (music-generation, L8) and out of scope
 * for this pass -- the same "stub, not logic" treatment
 * constraint_evaluate and preference_rank get in their own headers.
 * This type only defines the shape of that extraction's result.
 *
 * constraints/preferences are independent owned arrays-of-pointers-plus-
 * count, the same shape Fragmentation gives extracted_event_ids
 * (transform/fragmentation.h) and AnalysisResult gives its
 * interpretations/evidences (analysis/analyzer.h) -- adapted here to
 * Constraint/Preference. Both may be empty (count == 0, pass NULL for
 * the corresponding array): an Intent that stated no constraints or no
 * preferences still normalizes to a valid, empty ConstraintExtraction,
 * not an error. There is no forced 1:1 relationship between the two
 * arrays, or back to Intent's own constraint/preference counts -- one
 * raw Intent constraint may normalize to zero, one, or multiple
 * Constraint objects (e.g. a compound requirement split apart), the
 * same many-to-many freedom AnalysisResult allows between
 * Interpretation and Evidence.
 */

typedef struct ConstraintExtraction ConstraintExtraction;

typedef enum {
    CONSTRAINT_EXTRACTION_OK = 0,
    CONSTRAINT_EXTRACTION_ERR_NULL_CONSTRAINT,
    CONSTRAINT_EXTRACTION_ERR_NULL_PREFERENCE,
    CONSTRAINT_EXTRACTION_ERR_ALLOC
} ConstraintExtractionResult;

/*
 * Takes ownership of each Constraint in constraints and each Preference
 * in preferences on success (both copied into internally-owned arrays,
 * destroyed by constraint_extraction_destroy). Pass NULL with the
 * matching count == 0 for either not supplied. No entry of a non-empty
 * array may be NULL.
 */
ConstraintExtractionResult constraint_extraction_create(Constraint **constraints, size_t constraint_count,
                                                          Preference **preferences, size_t preference_count,
                                                          ConstraintExtraction **out);
void constraint_extraction_destroy(ConstraintExtraction *extraction);

size_t constraint_extraction_constraint_count(const ConstraintExtraction *extraction);
const Constraint *constraint_extraction_constraint_at(const ConstraintExtraction *extraction, size_t index);
size_t constraint_extraction_preference_count(const ConstraintExtraction *extraction);
const Preference *constraint_extraction_preference_at(const ConstraintExtraction *extraction, size_t index);

#endif