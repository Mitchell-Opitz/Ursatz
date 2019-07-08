#ifndef MUSIC_TRANSFORM_TRANSFORMATION_H
#define MUSIC_TRANSFORM_TRANSFORMATION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Transformation (Category / base type): a pure domain operation
 * producing new/derived objects (TDD §139-140). Every concrete
 * transformation (Transposition, Inversion, Retrograde, ...) is built
 * on this base type.
 *
 * Fields: id, input_type, parameters, output_type,
 * preserved_dimensions, modified_dimensions, mapping_behavior,
 * validation, provenance, deterministic.
 *
 * input_type/output_type are opaque type-tag strings (e.g.
 * "NoteEvent", "Structure") rather than a resolved type reference --
 * no generic type-registry concept exists yet in the canonical IR, so
 * this is the same placeholder treatment Interpretation gives
 * author/method (interpretation/interpretation.h).
 *
 * preserved_dimensions/modified_dimensions/mapping_behavior/validation
 * are required descriptor strings, not enums or lists -- no dedicated
 * Dimension type exists yet at this layer. A concrete transformation
 * (e.g. Transposition) MUST still state these explicitly per its own
 * contract (TDD §141-144); this base type only requires that some
 * non-empty statement is present, not that it be structured.
 *
 * parameters is an opaque, optional Value -- the same opaque-payload
 * pattern Rule gives its scope field (rules/rule.h).
 *
 * provenance is a required ProvenanceReference (core/provenance_reference.h):
 * every Transformation MUST carry provenance for the object(s) it
 * derives, per the base Transformation contract -- not only
 * Ornamentation, whose one-to-many fan-out (transform/ornamentation.h,
 * a later commit) makes provenance the trickiest to get right.
 *
 * deterministic records whether repeated application with the same
 * input/parameters MUST produce an identical output (TDD §139's
 * determinism field). This base type only records the flag; it does
 * not itself execute anything -- Transformation carries no apply()
 * behavior of its own, since evaluation is defined per concrete
 * subtype.
 *
 * Referential transparency (TDD §140): this type has no mutator and no
 * reference back to an input object to mutate -- a Transformation
 * describes an operation's contract, it does not hold or alter the
 * object it was run on.
 *
 * Each concrete transformation module exposes its owned base
 * Transformation (the fields above -- input/output type, dimensions,
 * mapping behavior, determinism, provenance) via a `<module>_transformation()`
 * accessor.
 */

typedef struct Transformation Transformation;

typedef enum {
    TRANSFORMATION_OK = 0,
    TRANSFORMATION_ERR_NULL_ID,
    TRANSFORMATION_ERR_NULL_INPUT_TYPE,
    TRANSFORMATION_ERR_NULL_OUTPUT_TYPE,
    TRANSFORMATION_ERR_NULL_PRESERVED_DIMENSIONS,
    TRANSFORMATION_ERR_NULL_MODIFIED_DIMENSIONS,
    TRANSFORMATION_ERR_NULL_MAPPING_BEHAVIOR,
    TRANSFORMATION_ERR_NULL_VALIDATION,
    TRANSFORMATION_ERR_NULL_PROVENANCE,
    TRANSFORMATION_ERR_ALLOC
} TransformationResult;

/*
 * Takes ownership of id, parameters (if non-NULL), and provenance on
 * success (all destroyed by transformation_destroy). input_type,
 * output_type, preserved_dimensions, modified_dimensions,
 * mapping_behavior, and validation are required (copied, non-empty).
 * parameters is optional -- pass NULL if this transformation takes
 * none.
 */
TransformationResult transformation_create(EntityID *id, const char *input_type, Value *parameters,
                                            const char *output_type, const char *preserved_dimensions,
                                            const char *modified_dimensions, const char *mapping_behavior,
                                            const char *validation, bool deterministic,
                                            ProvenanceReference *provenance, Transformation **out);
void transformation_destroy(Transformation *transformation);

const EntityID *transformation_id(const Transformation *transformation);
const char *transformation_input_type(const Transformation *transformation);
const Value *transformation_parameters(const Transformation *transformation);
const char *transformation_output_type(const Transformation *transformation);
const char *transformation_preserved_dimensions(const Transformation *transformation);
const char *transformation_modified_dimensions(const Transformation *transformation);
const char *transformation_mapping_behavior(const Transformation *transformation);
const char *transformation_validation(const Transformation *transformation);
bool transformation_deterministic(const Transformation *transformation);
const ProvenanceReference *transformation_provenance(const Transformation *transformation);

bool transformation_identity_equals(const Transformation *a, const Transformation *b);

#endif