#ifndef MUSIC_GENERATION_GENERATOR_DISPATCH_H
#define MUSIC_GENERATION_GENERATOR_DISPATCH_H

#include "core/value.h"
#include "generation/candidate.h"
#include "generation/generation_plan.h"
#include "generation/generator.h"
#include "generation/generator_registry.h"

/*
 * Generator dispatch: runs one registered Generator by name.
 *
 *   generator_dispatch(registry, name, plan, random_seed, configuration,
 *                      &candidate_set)
 *
 * finds the Generator registered under name and calls its generate() with the
 * plan, the seed and the configuration, exactly as given. On
 * GENERATOR_DISPATCH_OK *out_set is the CandidateSet it returned, which the
 * caller owns (destroy with candidate_set_destroy). On any other result
 * *out_set is left untouched and nothing is returned for the caller to free.
 *
 * What dispatch does NOT do: it does not evaluate or rank the candidates
 * (that is CandidateEvaluator, L8; Constraint and Preference are not consulted
 * here), does not choose a Generator, does not retry, and does not call
 * generate() more than once -- a deterministic Generator's repeatability is
 * its own contract (generator.h, candidate.h), checked by tests, not by a
 * second call at run time. It never modifies the plan.
 *
 * It does check, cheaply, that what came back belongs to this run: the
 * returned CandidateSet's own random_seed equals the seed passed and its
 * generator_version equals the Generator's version. A Generator that returns a
 * set recording another seed or version breaks reproducibility, so that set is
 * destroyed and the result is GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION.
 *
 * Results:
 *   ERR_NULL_ARGUMENT     registry, name, plan or out_set is NULL (configuration
 *                         may be NULL: it is optional)
 *   ERR_UNKNOWN_GENERATOR no Generator is registered under name
 *   ERR_GENERATION_FAILED the Generator returned any result other than OK or
 *                         ALLOC, or returned OK without a CandidateSet
 *   ERR_CONTRACT_VIOLATION see above
 *   ERR_ALLOC             the Generator returned GENERATOR_ERR_ALLOC
 */

typedef enum {
    GENERATOR_DISPATCH_OK = 0,
    GENERATOR_DISPATCH_ERR_NULL_ARGUMENT,
    GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR,
    GENERATOR_DISPATCH_ERR_GENERATION_FAILED,
    GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION,
    GENERATOR_DISPATCH_ERR_ALLOC
} GeneratorDispatchResult;

GeneratorDispatchResult generator_dispatch(const GeneratorRegistry *registry, const char *name,
                                           const GenerationPlan *plan, unsigned long random_seed,
                                           const Value *configuration, CandidateSet **out_set);

#endif
