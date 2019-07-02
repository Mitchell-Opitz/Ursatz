#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/generation_record.h"
#include "identity/entity_id.h"

/*
 * Construction test: GenerationRecord is the durable record of a
 * generation run -- seed, generator_version, framework_version (a KNOWN
 * GAP, optional until Framework has real versioning), configuration,
 * and provenance.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    long config_content = 7;
    Value *configuration;
    assert(value_create("GeneratorConfiguration", &config_content, sizeof(config_content), &configuration) ==
           VALUE_OK);

    EntityID *id = make_id("generation-record-1");
    GenerationRecord *record;
    assert(generation_record_create(id, 42, "1.0.0", "common-practice-tonal-harmony-1.2", configuration,
                                     make_absent_provenance(), &record) == GENERATION_RECORD_OK);

    assert(generation_record_random_seed(record) == 42);
    assert(strcmp(generation_record_generator_version(record), "1.0.0") == 0);
    assert(strcmp(generation_record_framework_version(record), "common-practice-tonal-harmony-1.2") == 0);
    size_t config_size;
    const long *config_value = (const long *)value_content(generation_record_configuration(record), &config_size);
    assert(config_size == sizeof(long));
    assert(*config_value == 7);
    assert(provenance_reference_state(generation_record_provenance(record)) == PROVENANCE_REFERENCE_ABSENT);

    /* Minimal GenerationRecord: no configuration, framework_version not available (KNOWN GAP). */
    EntityID *minimal_id = make_id("generation-record-minimal");
    GenerationRecord *minimal;
    assert(generation_record_create(minimal_id, 0, "1.0.0", NULL, NULL, make_absent_provenance(), &minimal) ==
           GENERATION_RECORD_OK);
    assert(generation_record_random_seed(minimal) == 0);
    assert(generation_record_framework_version(minimal) == NULL);
    assert(generation_record_configuration(minimal) == NULL);

    assert(generation_record_identity_equals(record, record));
    assert(!generation_record_identity_equals(record, minimal));

    /* Rejects a missing generator_version. */
    EntityID *no_version_id = make_id("generation-record-rejects-no-version");
    GenerationRecord *rejected;
    assert(generation_record_create(no_version_id, 0, NULL, NULL, NULL, make_absent_provenance(), &rejected) ==
           GENERATION_RECORD_ERR_NULL_GENERATOR_VERSION);
    entity_id_destroy(no_version_id);

    generation_record_destroy(record);
    generation_record_destroy(minimal);

    printf("test_generation_record: ok\n");
    return 0;
}