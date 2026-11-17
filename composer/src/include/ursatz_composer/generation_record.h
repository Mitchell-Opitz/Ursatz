#ifndef URSATZ_COMPOSER_GENERATION_RECORD_H
#define URSATZ_COMPOSER_GENERATION_RECORD_H

#include <model/core/provenance_reference.h>
#include <model/core/value.h>
#include <model/identity/entity_id.h>

/*
 * GenerationRecord (TDD Sec163): the durable record of a fill run: its seed and its versions, kept for
 * reproducibility. A FillRecord (fill_record.h) holds one.
 *
 * random_seed is the seed the run was invoked with, always recorded. generator_version is a required, non-empty
 * string. framework_version is optional (NULL when not available; Framework carries no version yet). configuration is
 * an optional, owned copy of whatever Value configured the run (NULL when none). provenance is a required
 * ProvenanceReference naming what produced the record.
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
 * (Framework has no version yet). configuration is optional -- pass
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

#endif
