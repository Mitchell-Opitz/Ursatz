#ifndef MUSIC_GENERATION_INTENT_H
#define MUSIC_GENERATION_INTENT_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Intent (Category / base type, Registry #21): a structured compilation
 * of a natural-language generation request (TDD Sec154). Intent is the
 * ONLY entry point through which natural language may reach the
 * Canonical IR (TDD Sec154) -- nothing in music-generation accepts a raw
 * request string; IntentCompiler (generation/intent_compiler.h, a later
 * commit) is the sole producer of an Intent from natural language, and
 * every downstream module (ConstraintExtraction, GenerationPlan,
 * Generator, ...) consumes Intent, never the original text.
 *
 * Fields per TDD Sec154: purpose, target, style, context, meter, length,
 * constraints, preferences, source_material.
 *
 * purpose and target are required, non-empty descriptor strings (e.g.
 * purpose="counter-melody", target="8-measure phrase in the soprano
 * voice"). No dedicated Purpose/Target type exists yet at this layer, so
 * these are opaque strings -- the same placeholder treatment
 * Transformation gives input_type/output_type (transform/
 * transformation.h) and Interpretation gives author/method
 * (interpretation/interpretation.h).
 *
 * style, meter, and length are optional descriptor strings (NULL when
 * unspecified by the request) for the same reason -- no dedicated
 * Style/Meter/Length type exists yet.
 *
 * context is an optional EntityID reference to a Context (context/
 * context.h, L6) -- referenced by id, never embedded, consistent with
 * Interpretation's context_id.
 *
 * constraints/preferences are the RAW, pre-extraction requirements and
 * wishes carried by the request -- opaque Value payloads, not yet
 * executable Constraint/Preference objects (rules/constraint.h,
 * rules/preference.h). Turning these into an executable constraint set
 * is ConstraintExtraction/Normalization's job (Registry #23, a later
 * commit), not Intent's: Intent only records what the request said,
 * uninterpreted. Each is an owned array-of-Value-pointers-plus-count,
 * the same shape Fragmentation gives extracted_event_ids (transform/
 * fragmentation.h), adapted to Value since these are opaque content
 * rather than entity references. Both arrays are optional -- pass NULL
 * with count 0 when the request stated no constraints or no
 * preferences.
 *
 * source_material is an optional EntityID reference to existing
 * Canonical IR material (e.g. a Structure) the request asks generation
 * to vary, extend, or otherwise build from -- absent when the request
 * asks for material generated from nothing.
 *
 * provenance is a required ProvenanceReference: every Intent records
 * what produced it (typically the IntentCompiler invocation that
 * compiled it).
 *
 * Intent holds no reference to CandidateSet or any other downstream
 * object -- it describes only the request, and is referentially
 * transparent in the same sense Transformation is (TDD Sec140): nothing
 * here mutates in place, and nothing here reaches forward into what a
 * Generator later produces from it.
 */

typedef struct Intent Intent;

typedef enum {
    INTENT_OK = 0,
    INTENT_ERR_NULL_ID,
    INTENT_ERR_NULL_PURPOSE,
    INTENT_ERR_NULL_TARGET,
    INTENT_ERR_NULL_CONSTRAINT,
    INTENT_ERR_NULL_PREFERENCE,
    INTENT_ERR_NULL_PROVENANCE,
    INTENT_ERR_ALLOC
} IntentResult;

/*
 * Takes ownership of id, context_id (if non-NULL), each Value in
 * constraints (if any), each Value in preferences (if any),
 * source_material_id (if non-NULL), and provenance on success (all
 * destroyed by intent_destroy). purpose and target are required
 * (copied, non-empty). style, meter, and length are optional (copied
 * when non-NULL, pass NULL for any not supplied by the request).
 *
 * constraints/preferences are optional -- pass NULL with the matching
 * count == 0 for either not supplied. No entry of a non-empty array may
 * be NULL.
 */
IntentResult intent_create(EntityID *id, const char *purpose, const char *target, const char *style,
                            EntityID *context_id, const char *meter, const char *length, Value **constraints,
                            size_t constraint_count, Value **preferences, size_t preference_count,
                            EntityID *source_material_id, ProvenanceReference *provenance, Intent **out);
void intent_destroy(Intent *intent);

const EntityID *intent_id(const Intent *intent);
const char *intent_purpose(const Intent *intent);
const char *intent_target(const Intent *intent);
const char *intent_style(const Intent *intent);
const EntityID *intent_context_id(const Intent *intent);
const char *intent_meter(const Intent *intent);
const char *intent_length(const Intent *intent);
size_t intent_constraint_count(const Intent *intent);
const Value *intent_constraint_at(const Intent *intent, size_t index);
size_t intent_preference_count(const Intent *intent);
const Value *intent_preference_at(const Intent *intent, size_t index);
const EntityID *intent_source_material_id(const Intent *intent);
const ProvenanceReference *intent_provenance(const Intent *intent);

bool intent_identity_equals(const Intent *a, const Intent *b);

#endif