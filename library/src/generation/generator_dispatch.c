#include "generation/generator_dispatch.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"

/* FNV-1a, 64 bit. */
static uint64_t fnv_update(uint64_t hash, const void *bytes, size_t size) {
    const unsigned char *p = (const unsigned char *)bytes;
    for (size_t i = 0; i < size; i++) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* The configuration's hash as lowercase hex, or "none" when there is no configuration. */
static void configuration_hash(const Value *configuration, char out[17]) {
    if (configuration == NULL) {
        snprintf(out, 17, "none");
        return;
    }
    size_t size = 0;
    const void *content = value_content(configuration, &size);
    uint64_t hash = 14695981039346656037ULL;
    hash = fnv_update(hash, value_type(configuration), strlen(value_type(configuration)));
    hash = fnv_update(hash, "", 1);
    if (size > 0 && content != NULL) {
        hash = fnv_update(hash, content, size);
    }
    snprintf(out, 17, "%016" PRIx64, hash);
}

/* A copy of the configuration, or NULL when there is none (or on allocation failure; see *failed). */
static Value *copy_configuration(const Value *configuration, int *failed) {
    *failed = 0;
    if (configuration == NULL) {
        return NULL;
    }
    size_t size = 0;
    const void *content = value_content(configuration, &size);
    Value *copy = NULL;
    if (value_create(value_type(configuration), content != NULL ? content : "", size, &copy) != VALUE_OK) {
        *failed = 1;
        return NULL;
    }
    return copy;
}

/* The GenerationRecord of one successful run, see generator_dispatch.h. NULL on allocation failure. */
static GenerationRecord *build_record(const Generator *generator, const GenerationPlan *plan,
                                      unsigned long random_seed, const Value *configuration) {
    const char *plan_id = entity_id_value(generation_plan_id(plan));
    char hash[17];
    configuration_hash(configuration, hash);

    size_t id_length = strlen(plan_id) + strlen(generator->name) + strlen(generator->version) + 96;
    char *id_text = (char *)malloc(id_length);
    if (id_text == NULL) {
        return NULL;
    }
    snprintf(id_text, id_length, "generation-record:%s:%s:%s:%lu:%s", plan_id, generator->name, generator->version,
             random_seed, hash);

    EntityID *id = NULL;
    EntityID *plan_target = NULL;
    ProvenanceReference *provenance = NULL;
    int failed = 0;
    Value *configuration_copy = copy_configuration(configuration, &failed);
    GenerationRecord *record = NULL;
    if (!failed && entity_id_create(id_text, &id) == ENTITY_ID_OK && entity_id_create(plan_id, &plan_target) == ENTITY_ID_OK) {
        if (provenance_reference_create_present(plan_target, &provenance) == PROVENANCE_REFERENCE_OK) {
            plan_target = NULL; /* owned by the provenance reference now */
            if (generation_record_create(id, random_seed, generator->version, NULL, configuration_copy, provenance,
                                         &record) == GENERATION_RECORD_OK) {
                id = NULL;
                configuration_copy = NULL;
                provenance = NULL;
            }
        }
    }
    free(id_text);
    entity_id_destroy(id);
    entity_id_destroy(plan_target);
    value_destroy(configuration_copy);
    provenance_reference_destroy(provenance);
    return record;
}

GeneratorDispatchResult generator_dispatch(const GeneratorRegistry *registry, const char *name,
                                           const GenerationPlan *plan, unsigned long random_seed,
                                           const Value *configuration, CandidateSet **out_set,
                                           GenerationRecord **out_record) {
    if (registry == NULL || name == NULL || plan == NULL || out_set == NULL || out_record == NULL) {
        return GENERATOR_DISPATCH_ERR_NULL_ARGUMENT;
    }
    const Generator *generator = generator_registry_lookup(registry, name);
    if (generator == NULL) {
        return GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR;
    }

    CandidateSet *set = NULL;
    GeneratorResult generated = generator->generate(plan, random_seed, configuration, &set);
    if (generated == GENERATOR_ERR_ALLOC) {
        return GENERATOR_DISPATCH_ERR_ALLOC;
    }
    if (generated != GENERATOR_OK || set == NULL) {
        return GENERATOR_DISPATCH_ERR_GENERATION_FAILED;
    }

    if (candidate_set_random_seed(set) != random_seed ||
        strcmp(candidate_set_generator_version(set), generator->version) != 0) {
        candidate_set_destroy(set);
        return GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION;
    }

    GenerationRecord *record = build_record(generator, plan, random_seed, configuration);
    if (record == NULL) {
        candidate_set_destroy(set);
        return GENERATOR_DISPATCH_ERR_ALLOC;
    }

    *out_set = set;
    *out_record = record;
    return GENERATOR_DISPATCH_OK;
}
