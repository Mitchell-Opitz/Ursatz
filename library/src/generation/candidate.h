#ifndef MUSIC_GENERATION_CANDIDATE_H
#define MUSIC_GENERATION_CANDIDATE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"

/*
 * Candidate (Category / base type, Registry #27): a proposed result
 * pre-acceptance (TDD Sec158) -- material, evaluations, violations,
 * score, provenance. Shared with music-analysis per the Registry:
 * wherever music-analysis needs the same "proposed, not yet accepted"
 * shape (e.g. a candidate interpretation before it is accepted), it
 * uses this same Candidate type rather than defining its own.
 *
 * material_id is a required EntityID reference to the candidate's
 * proposed material (e.g. a Structure, structure/structure.h) --
 * referenced by id only, never embedded, the same pattern Fragmentation
 * gives source_structure_id (transform/fragmentation.h): the material
 * itself lives in the canonical IR, Candidate only points to it.
 *
 * evaluations and violations are independent, owned arrays-of-
 * Interpretation-pointers-plus-count (interpretation/interpretation.h,
 * L6) -- kept as two SEPARATE fields, matching TDD Sec158's own field
 * list, rather than one array a caller must filter. Reusing
 * Interpretation rather than inventing a new evaluation type is
 * deliberate: CandidateEvaluator (Registry #28, a later commit) MUST
 * keep its findings explainable (TDD Sec159) -- backed by evidence and
 * carrying its own uncertainty, never a bare pass/fail -- which is
 * exactly what Interpretation already provides, the same reasoning-
 * trail shape Analyzer gives AnalysisResult (analysis/analyzer.h,
 * Registry #12). Both arrays may be empty (count == 0, pass NULL for
 * the corresponding array): a Candidate not yet evaluated still exists
 * validly, the same "found nothing is not an error" treatment
 * AnalysisResult gives its own arrays.
 *
 * score is an exact rational fraction (score_numerator/
 * score_denominator, denominator > 0, Law 19 -- never float/double),
 * the same representation Evidence's strength and Constraint's WEIGHTED
 * weight use: an aggregate summary metric, not a substitute for the
 * reasoning trail evaluations/violations carry.
 *
 * provenance is a required ProvenanceReference: every Candidate records
 * what produced it (typically the Generator invocation that proposed
 * it).
 */

typedef struct Candidate Candidate;

typedef enum {
    CANDIDATE_OK = 0,
    CANDIDATE_ERR_NULL_ID,
    CANDIDATE_ERR_NULL_MATERIAL_ID,
    CANDIDATE_ERR_NULL_EVALUATION,
    CANDIDATE_ERR_NULL_VIOLATION,
    CANDIDATE_ERR_NON_POSITIVE_SCORE_DENOMINATOR,
    CANDIDATE_ERR_NULL_PROVENANCE,
    CANDIDATE_ERR_ALLOC
} CandidateResult;

/*
 * Takes ownership of id, material_id, each Interpretation in
 * evaluations (if any), each Interpretation in violations (if any), and
 * provenance on success (all destroyed by candidate_destroy).
 * evaluations/violations are optional -- pass NULL with the matching
 * count == 0 for either not supplied. No entry of a non-empty array may
 * be NULL. score_denominator must be > 0.
 */
CandidateResult candidate_create(EntityID *id, EntityID *material_id, Interpretation **evaluations,
                                  size_t evaluation_count, Interpretation **violations, size_t violation_count,
                                  unsigned long score_numerator, unsigned long score_denominator,
                                  ProvenanceReference *provenance, Candidate **out);
void candidate_destroy(Candidate *candidate);

const EntityID *candidate_id(const Candidate *candidate);
const EntityID *candidate_material_id(const Candidate *candidate);
size_t candidate_evaluation_count(const Candidate *candidate);
const Interpretation *candidate_evaluation_at(const Candidate *candidate, size_t index);
size_t candidate_violation_count(const Candidate *candidate);
const Interpretation *candidate_violation_at(const Candidate *candidate, size_t index);
unsigned long candidate_score_numerator(const Candidate *candidate);
unsigned long candidate_score_denominator(const Candidate *candidate);
const ProvenanceReference *candidate_provenance(const Candidate *candidate);

bool candidate_identity_equals(const Candidate *a, const Candidate *b);

/*
 * CandidateSet (Category / base type): the owned collection of
 * Candidate objects a Generator (generation/generator.h, Registry #26)
 * returns from generate() -- the same CandidateSet generator.h forward-
 * declares (as an opaque `struct CandidateSet`) without defining. This
 * is that definition; it uses the identical struct tag so both headers
 * name the same type, resolved once both headers are visible together.
 *
 * candidates is a required, owned array-of-Candidate-pointers-plus-
 * count (count may be 0 -- a generation run that produced no viable
 * candidates still returns a valid, empty CandidateSet).
 *
 * random_seed, generator_version, configuration, input_ids, and
 * framework_version are TDD Sec163's reproducibility bundle: a
 * stochastic Generator MUST preserve these so the same call can be
 * reproduced.
 *   - random_seed is the seed generate() was invoked with -- meaningful
 *     only when the producing Generator was itself stochastic
 *     (Generator.deterministic == false), but always recorded.
 *   - generator_version mirrors the producing Generator's own version
 *     field (generation/generator.h) -- required, non-empty.
 *   - configuration is an optional, owned copy of whatever Value was
 *     passed to generate() (NULL when the generator took none).
 *   - input_ids is a required, owned array of EntityID references
 *     (count may be 0) to whatever fed generation -- typically the
 *     GenerationPlan's id and/or its Intent's id (generation/
 *     generation_plan.h, generation/intent.h).
 *   - framework_version is a KNOWN GAP, flagged rather than silently
 *     fabricated: TDD Sec163 requires it, but Framework (theory/
 *     framework.h) carries no version field of its own yet -- the same
 *     unresolved-dependency situation Interpretation flags for its own
 *     omitted framework field. Kept here as an optional, opaque
 *     descriptor string (NULL when not available) until Framework
 *     gains real versioning; not required, so this type does not block
 *     on that gap.
 *
 * provenance is a required ProvenanceReference: what produced this
 * CandidateSet (typically the Generator invocation itself).
 */

typedef struct CandidateSet CandidateSet;

typedef enum {
    CANDIDATE_SET_OK = 0,
    CANDIDATE_SET_ERR_NULL_ID,
    CANDIDATE_SET_ERR_NULL_CANDIDATE,
    CANDIDATE_SET_ERR_NULL_GENERATOR_VERSION,
    CANDIDATE_SET_ERR_NULL_INPUT_ID,
    CANDIDATE_SET_ERR_NULL_PROVENANCE,
    CANDIDATE_SET_ERR_ALLOC
} CandidateSetResult;

/*
 * Takes ownership of id, each Candidate in candidates (if any),
 * configuration (if non-NULL), each EntityID in input_ids (if any), and
 * provenance on success (all destroyed by candidate_set_destroy).
 * candidates and input_ids are each optional -- pass NULL with the
 * matching count == 0 for either not supplied. generator_version is
 * copied, required (non-empty). configuration is optional -- pass NULL
 * if the generator took none. framework_version is copied when
 * non-NULL, optional -- pass NULL when not available (see the KNOWN GAP
 * note above). No entry of a non-empty candidates or input_ids array
 * may be NULL.
 */
CandidateSetResult candidate_set_create(EntityID *id, Candidate **candidates, size_t candidate_count,
                                         unsigned long random_seed, const char *generator_version,
                                         Value *configuration, EntityID **input_ids, size_t input_id_count,
                                         const char *framework_version, ProvenanceReference *provenance,
                                         CandidateSet **out);
void candidate_set_destroy(CandidateSet *set);

const EntityID *candidate_set_id(const CandidateSet *set);
size_t candidate_set_candidate_count(const CandidateSet *set);
const Candidate *candidate_set_candidate_at(const CandidateSet *set, size_t index);
unsigned long candidate_set_random_seed(const CandidateSet *set);
const char *candidate_set_generator_version(const CandidateSet *set);
/* NULL when the generator took no configuration. */
const Value *candidate_set_configuration(const CandidateSet *set);
size_t candidate_set_input_id_count(const CandidateSet *set);
const EntityID *candidate_set_input_id_at(const CandidateSet *set, size_t index);
/* NULL when not available -- see the KNOWN GAP note above. */
const char *candidate_set_framework_version(const CandidateSet *set);
const ProvenanceReference *candidate_set_provenance(const CandidateSet *set);

bool candidate_set_identity_equals(const CandidateSet *a, const CandidateSet *b);

#endif