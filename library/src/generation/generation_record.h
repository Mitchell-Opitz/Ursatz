#ifndef MUSIC_GENERATION_GENERATION_RECORD_H
#define MUSIC_GENERATION_GENERATION_RECORD_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * GenerationRecord (Entity, Registry item, TDD Sec163): the durable
 * record of a generation run -- seed, versions, and configuration, kept
 * for reproducibility. Distinct from Candidate/Generator's own seed/
 * version fields (generation/candidate.h, generation/generator.h):
 * CandidateSet carries this same reproducibility bundle scoped to the
 * one generate() call that produced it, and Generator's own version
 * field is a declared CAPABILITY of the generator itself, present
 * whether or not it was ever invoked. GenerationRecord is neither of
 * those -- it is the standalone, durable log entry that a generation
 * run happened, independent of whether the CandidateSet it produced
 * still exists anywhere.
 *
 * random_seed is the seed the generation run was invoked with --
 * meaningful only when the run was stochastic, but always recorded
 * (the same "always recorded" treatment CandidateSet gives its own
 * random_seed field).
 *
 * generator_version is a required, non-empty string mirroring the
 * producing Generator's own version field.
 *
 * framework_version is a KNOWN GAP, flagged rather than silently
 * fabricated: TDD Sec163 requires it, but Framework (theory/
 * framework.h) carries no version field of its own yet -- the same
 * unresolved-dependency situation CandidateSet flags for its own
 * framework_version field. Kept here as an optional, opaque descriptor
 * string (NULL when not available) until Framework gains real
 * versioning; not required, so this type does not block on that gap.
 *
 * configuration is an optional, owned copy of whatever Value configured
 * the run (NULL when the generator took none) -- the same
 * configuration field CandidateSet carries.
 *
 * provenance is a required ProvenanceReference (the only dependency
 * TDD Sec163 names for this type, cross-cutting per core/
 * provenance_reference.h): what produced this record (typically the
 * Generator invocation itself).
 */

typedef struct GenerationRecord GenerationRecord;

typedef enum {
    GENERATION_RECORD_OK = 0,
    GENERATION_RECORD_ERR_NULL_ID,
    GENERATION_RECORD_ERR_NULL_GENERATOR_VERSION,
    GENERATION_RECORD_ERR_NULL_PROVENANCE,
    GENERATION_RECORD_ERR_ALLOC
} GenerationRecordResult;

/*
 * Takes ownership of id, configuration (if non-NULL), and provenance on
 * success (all destroyed by generation_record_destroy).
 * generator_version is copied, required (non-empty). framework_version
 * is copied when non-NULL, optional -- pass NULL when not available
 * (see the KNOWN GAP note above). configuration is optional -- pass
 * NULL if the run took none.
 */
GenerationRecordResult generation_record_create(EntityID *id, unsigned long random_seed,
                                                 const char *generator_version, const char *framework_version,
                                                 Value *configuration, ProvenanceReference *provenance,
                                                 GenerationRecord **out);
void generation_record_destroy(GenerationRecord *record);

const EntityID *generation_record_id(const GenerationRecord *record);
unsigned long generation_record_random_seed(const GenerationRecord *record);
const char *generation_record_generator_version(const GenerationRecord *record);
/* NULL when not available -- see the KNOWN GAP note above. */
const char *generation_record_framework_version(const GenerationRecord *record);
/* NULL when the generation run took no configuration. */
const Value *generation_record_configuration(const GenerationRecord *record);
const ProvenanceReference *generation_record_provenance(const GenerationRecord *record);

bool generation_record_identity_equals(const GenerationRecord *a, const GenerationRecord *b);

#endif