#include <ursatz_composer/generation_record.h>

#include <stdlib.h>
#include <string.h>

#include "util/util.h"

struct GenerationRecord {
    EntityID *id;
    unsigned long random_seed;
    char *generator_version;
    char *framework_version;
    Value *configuration;
    ProvenanceReference *provenance;
};

GenerationRecordResult generation_record_create(EntityID *id, unsigned long random_seed,
                                                 const char *generator_version, const char *framework_version,
                                                 Value *configuration, ProvenanceReference *provenance,
                                                 GenerationRecord **out) {
    if (id == NULL) {
        return GENERATION_RECORD_ERR_NULL_ID;
    }
    if (generator_version == NULL || generator_version[0] == '\0') {
        return GENERATION_RECORD_ERR_NULL_GENERATOR_VERSION;
    }
    if (provenance == NULL) {
        return GENERATION_RECORD_ERR_NULL_PROVENANCE;
    }

    GenerationRecord *record = (GenerationRecord *)malloc(sizeof(GenerationRecord));
    if (record == NULL) {
        return GENERATION_RECORD_ERR_ALLOC;
    }

    record->generator_version = util_copy_string(generator_version);
    if (record->generator_version == NULL) {
        free(record);
        return GENERATION_RECORD_ERR_ALLOC;
    }

    record->framework_version = NULL;
    if (framework_version != NULL) {
        record->framework_version = util_copy_string(framework_version);
        if (record->framework_version == NULL) {
            free(record->generator_version);
            free(record);
            return GENERATION_RECORD_ERR_ALLOC;
        }
    }

    record->id = id;
    record->random_seed = random_seed;
    record->configuration = configuration;
    record->provenance = provenance;

    *out = record;
    return GENERATION_RECORD_OK;
}

void generation_record_destroy(GenerationRecord *record) {
    if (record == NULL) {
        return;
    }
    entity_id_destroy(record->id);
    free(record->generator_version);
    free(record->framework_version);
    value_destroy(record->configuration);
    provenance_reference_destroy(record->provenance);
    free(record);
}

const EntityID *generation_record_id(const GenerationRecord *record) {
    return record->id;
}

unsigned long generation_record_random_seed(const GenerationRecord *record) {
    return record->random_seed;
}

const char *generation_record_generator_version(const GenerationRecord *record) {
    return record->generator_version;
}
