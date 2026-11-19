#ifndef URSATZ_COMPOSER_API_SEED_H
#define URSATZ_COMPOSER_API_SEED_H

#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/corpus_seed.h>
#include <plan/plan_constraint.h>

/* Seeding from the corpus passages a request gives (R-619). Private to the library and the command line. */

/* The most themes a run suggests, the seeds tried for them and the largest seed. */
#define GROW_MAX_SUGGESTIONS 16
#define GROW_MAX_SEED_TRIES 16
#define GROW_SEED_MAX 4294967295UL

/* The corpus passage a motive was taken from, as {piece, start, end, score}. */
cJSON *api_seed_json(const CorpusSeed *seed);

/* The seed as the plan records it. */
UrsatzStatus api_seed_source(const CorpusSeed *seed, PlanSource *out, UrsatzError *error);

/* The motive of one of the passages the request gives, in the meter and key. */
UrsatzStatus api_seed_motive(const KnowledgeSnapshot *knowledge, const CorpusPassages *passages, const PlanMeter *meter,
                               const SpelledKey *key, unsigned long seed, CorpusSeed *out, UrsatzError *error);

/* "3 stale documents (analyzed by another version; run the corpus again)", or "" when none is stale. */
void api_seed_stale_notice(const CorpusSeed *seed, char *out, size_t size);

#endif
