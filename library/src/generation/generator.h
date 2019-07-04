#ifndef MUSIC_GENERATION_GENERATOR_H
#define MUSIC_GENERATION_GENERATOR_H

#include <stdbool.h>

#include "core/value.h"
#include "generation/generation_plan.h"

/*
 * Generator (Interface, Registry #26): generate(plan) -> CandidateSet
 * (TDD Sec160). Like Analyzer (analysis/analyzer.h, Registry #12) and
 * IntentCompiler (generation/intent_compiler.h, Registry #22), Generator
 * is a function-pointer interface, not a data type with its own create/
 * destroy: a concrete generator is a module that implements GeneratorFn
 * and exposes it, alongside its declared metadata, as a Generator value
 * -- it does not subclass or extend this struct.
 *
 * CandidateSet is an OPAQUE FORWARD DECLARATION here. Candidate
 * (Registry #27) and the CandidateSet it collects into land in a later
 * commit -- Generator's own contract (what it declares about itself,
 * and the shape of generate()) does not depend on CandidateSet's
 * internal layout, only on it being a distinct owned result type a
 * caller receives back. This header never dereferences CandidateSet,
 * only holds and returns pointers to it -- the same forward-reference
 * treatment an opaque type gets in any pimpl-style C interface.
 *
 * TDD Sec160 requires a Generator to DECLARE, not merely implement:
 * supported inputs/outputs, configuration, constraints, determinism,
 * and version. These are metadata fields on the Generator value itself,
 * not something a caller must infer from behavior -- the same
 * "declares the contract" shape Transformation gives input_type/
 * output_type/preserved_dimensions/modified_dimensions/mapping_behavior
 * (transform/transformation.h), adapted here to a generator's own
 * capabilities rather than one operation's effect on data:
 *   - supported_inputs/supported_outputs: opaque type-tag strings (e.g.
 *     "GenerationPlan"/"CandidateSet") -- the same placeholder
 *     treatment Transformation's own input_type/output_type get, since
 *     no generic type-registry concept exists yet in the canonical IR.
 *   - configuration_description: a required descriptor string stating
 *     what configuration (if any) this generator accepts -- e.g. "none"
 *     when it takes none. No dedicated ConfigurationSchema type exists
 *     yet, so this is a string, not a structured schema.
 *   - constraints_description: a required descriptor string stating
 *     what Constraint/Preference vocabulary (rules/constraint.h,
 *     rules/preference.h) this generator honors.
 *   - deterministic: whether repeated calls with the same plan,
 *     random_seed, and configuration MUST produce identical output --
 *     the same determinism flag Transformation carries (TDD Sec139).
 *   - version: a required, non-empty generator_version string.
 *
 * generate(plan, random_seed, configuration) -> out: plan is a
 * borrowed, required GenerationPlan (generation/generation_plan.h).
 * random_seed is REQUIRED on every call (TDD Sec163): a deterministic
 * generator is free to ignore it, but a stochastic one (deterministic
 * == false) MUST consume it and MUST be reproducible from it -- calling
 * generate() again with the same plan/random_seed/configuration MUST
 * reproduce the same CandidateSet. Actually recording seed/
 * generator_version/configuration/input_ids/framework_version for that
 * reproducibility (TDD Sec163) is CandidateSet's job, via whatever
 * provenance Candidate carries (Registry #27, a later commit) -- this
 * interface only guarantees the seed reaches the generator; it does not
 * itself define where the record of it is kept. configuration is a
 * borrowed, optional Value (core/value.h) -- not owned, may be NULL
 * when the generator takes none (configuration_description == "none").
 *
 * On GENERATOR_OK, *out is a newly-created CandidateSet the caller
 * owns; on any other result, *out is left untouched. A concrete
 * generator MUST NOT mutate plan (TDD Sec140 referential transparency,
 * the same rule Transformation and Analyzer follow).
 */

typedef struct CandidateSet CandidateSet;

typedef enum {
    GENERATOR_OK = 0,
    GENERATOR_ERR_NULL_PLAN,
    GENERATOR_ERR_NULL_OUT,
    GENERATOR_ERR_ALLOC,
    GENERATOR_ERR_GENERATION_FAILED
} GeneratorResult;

typedef GeneratorResult (*GeneratorFn)(const GenerationPlan *plan, unsigned long random_seed,
                                       const Value *configuration, CandidateSet **out);

typedef struct {
    const char *name;
    const char *supported_inputs;
    const char *supported_outputs;
    const char *configuration_description;
    const char *constraints_description;
    bool deterministic;
    const char *version;
    GeneratorFn generate;
} Generator;

#endif