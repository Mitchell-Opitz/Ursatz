#include "generation/generation_record.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

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

    record->generator_version = ursatz_string_copy(generator_version);
    if (record->generator_version == NULL) {
        free(record);
        return GENERATION_RECORD_ERR_ALLOC;
    }

    record->framework_version = NULL;
    if (framework_version != NULL) {
        record->framework_version = ursatz_string_copy(framework_version);
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

const char *generation_record_framework_version(const GenerationRecord *record) {
    return record->framework_version;
}

const Value *generation_record_configuration(const GenerationRecord *record) {
    return record->configuration;
}

const ProvenanceReference *generation_record_provenance(const GenerationRecord *record) {
    return record->provenance;
}

bool generation_record_identity_equals(const GenerationRecord *a, const GenerationRecord *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}