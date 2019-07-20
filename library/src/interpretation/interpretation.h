#ifndef MUSIC_INTERPRETATION_INTERPRETATION_H
#define MUSIC_INTERPRETATION_INTERPRETATION_H

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/uncertainty.h"

/*
 * Interpretation (Category / base type): a contextual claim about
 * musical material (TDD §87).
 *
 * Fields: target (reference), claim, context (reference to Context,
 * #1), evidence (reference), framework (reference to Framework, #2),
 * uncertainty, author, method, status, provenance
 * (ProvenanceReference).
 *
 * RESOLVED (Dependency Spec Open Issue §7.1, was: IMPORTANT
 * CONTRADICTION FLAG, same pattern as the Canonical IR issue at L1):
 * the Registry's "Depends On" column for Interpretation lists
 * "Framework" (L7, music-theory) as a dependency. A literal import
 * would violate the layer rule the same way it did at L1. Resolution,
 * per System Registry v3: Interpretation depends on a FrameworkReference
 * (core/framework_reference.h, L1) instead of a live Framework
 * reference -- the reference type lives at L1, the same reference-only
 * pattern NoteEvent's optional_voice already uses for Voice (L4). No
 * live music-theory import exists here or anywhere in music-interpretation;
 * resolution to an actual Framework happens only at L7+.
 *
 * target is an opaque EntityID reference to the object being
 * interpreted -- referenced, never embedded, the same pattern as
 * Assertion/Observation's subject. Interpretations MUST NOT mutate the
 * canonical objects they claim about (Law 8): this type holds only an
 * EntityID for target and exposes no accessor that resolves it to a
 * mutable object, let alone a mutator reaching into NoteEvent/Sonority/
 * etc.
 *
 * context is an EntityID reference to a Context (music-context, L6);
 * evidence is an EntityID reference to an Evidence
 * (interpretation/evidence.h, this module). Both are opaque references
 * by id, consistent with Relationship's context/evidence fields
 * (relationship/relationship.h) -- neither is embedded. Both are
 * optional (an Interpretation need not narrow to a specific Context or
 * cite a specific Evidence to exist).
 *
 * framework is an optional FrameworkReference (core/framework_reference.h).
 * Optional because not every Interpretation is framework-relative --
 * ScaleDegree/Chord/Key, the Registry's framework-relative interpretation
 * types, do not exist in this codebase yet, and every existing
 * Interpretation producer (the eight analyzers under music-analysis)
 * constructs framework-neutral claims today. Interpretation owns the
 * FrameworkReference wrapper itself (destroyed by interpretation_destroy),
 * the same way it already owns context_id/evidence_id -- it does not own
 * or resolve the Framework the reference points to.
 *
 * uncertainty is a required Uncertainty (interpretation/uncertainty.h).
 *
 * author/method are opaque identifier strings for now (ProvenanceAgent
 * does not exist yet -- same placeholder treatment as Assertion's
 * agent field, interpretation/assertion.h).
 *
 * status is TDD §88's status vocabulary.
 *
 * Multiple competing Interpretations MUST be able to coexist pointing
 * at the same target (TDD §89): this type enforces no uniqueness
 * constraint on target -- nothing here tracks "the" Interpretation for
 * a given target, and constructing two Interpretations with the same
 * target_id is always valid.
 */

typedef enum {
    INTERPRETATION_STATUS_PROPOSED,
    INTERPRETATION_STATUS_SUPPORTED,
    INTERPRETATION_STATUS_ACCEPTED,
    INTERPRETATION_STATUS_REJECTED,
    INTERPRETATION_STATUS_SUPERSEDED,
    INTERPRETATION_STATUS_USER_CONFIRMED,
    INTERPRETATION_STATUS_MACHINE_GENERATED,
    INTERPRETATION_STATUS_IMPORTED,
    INTERPRETATION_STATUS_DISPUTED,
    INTERPRETATION_STATUS_UNKNOWN
} InterpretationStatus;

typedef struct Interpretation Interpretation;

typedef enum {
    INTERPRETATION_OK = 0,
    INTERPRETATION_ERR_NULL_ID,
    INTERPRETATION_ERR_NULL_TARGET_ID,
    INTERPRETATION_ERR_NULL_CLAIM,
    INTERPRETATION_ERR_NULL_UNCERTAINTY,
    INTERPRETATION_ERR_NULL_AUTHOR,
    INTERPRETATION_ERR_NULL_METHOD,
    INTERPRETATION_ERR_NULL_PROVENANCE,
    INTERPRETATION_ERR_ALLOC
} InterpretationResult;

/*
 * Takes ownership of id, target_id, claim, uncertainty, and provenance
 * on success (all required, destroyed by interpretation_destroy).
 * author and method are required (copied, non-empty).
 *
 * context_id, evidence_id, and framework_reference are optional -- pass
 * NULL for any not supplied. When non-NULL, ownership transfers on
 * success and each is destroyed by interpretation_destroy.
 */
InterpretationResult interpretation_create(EntityID *id, EntityID *target_id, Value *claim, EntityID *context_id,
                                            EntityID *evidence_id, FrameworkReference *framework_reference,
                                            Uncertainty *uncertainty, const char *author, const char *method,
                                            InterpretationStatus status, ProvenanceReference *provenance,
                                            Interpretation **out);
void interpretation_destroy(Interpretation *interpretation);

const EntityID *interpretation_id(const Interpretation *interpretation);
const EntityID *interpretation_target_id(const Interpretation *interpretation);
const Value *interpretation_claim(const Interpretation *interpretation);
const EntityID *interpretation_context_id(const Interpretation *interpretation);
const EntityID *interpretation_evidence_id(const Interpretation *interpretation);
const FrameworkReference *interpretation_framework_reference(const Interpretation *interpretation);
const Uncertainty *interpretation_uncertainty(const Interpretation *interpretation);
const char *interpretation_author(const Interpretation *interpretation);
const char *interpretation_method(const Interpretation *interpretation);
InterpretationStatus interpretation_status(const Interpretation *interpretation);
const ProvenanceReference *interpretation_provenance(const Interpretation *interpretation);

#endif