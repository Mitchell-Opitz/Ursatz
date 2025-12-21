#include "generation/generator_registry.h"

#include <stdlib.h>
#include <string.h>

struct GeneratorRegistry {
    const Generator **generators;
    size_t count;
    size_t capacity;
};

static int non_empty(const char *text) {
    return text != NULL && text[0] != '\0';
}

static int is_valid(const Generator *generator) {
    return non_empty(generator->name) && non_empty(generator->version) && generator->generate != NULL &&
           generator->supported_inputs != NULL && generator->supported_outputs != NULL &&
           generator->configuration_description != NULL && generator->constraints_description != NULL;
}

GeneratorRegistryResult generator_registry_create(GeneratorRegistry **out) {
    if (out == NULL) {
        return GENERATOR_REGISTRY_ERR_NULL_ARGUMENT;
    }
    GeneratorRegistry *registry = (GeneratorRegistry *)calloc(1, sizeof(*registry));
    if (registry == NULL) {
        return GENERATOR_REGISTRY_ERR_ALLOC;
    }
    *out = registry;
    return GENERATOR_REGISTRY_OK;
}

void generator_registry_destroy(GeneratorRegistry *registry) {
    if (registry == NULL) {
        return;
    }
    free(registry->generators);
    free(registry);
}

GeneratorRegistryResult generator_registry_register(GeneratorRegistry *registry, const Generator *generator) {
    if (registry == NULL || generator == NULL) {
        return GENERATOR_REGISTRY_ERR_NULL_ARGUMENT;
    }
    if (!is_valid(generator)) {
        return GENERATOR_REGISTRY_ERR_INVALID_GENERATOR;
    }
    if (generator_registry_lookup(registry, generator->name) != NULL) {
        return GENERATOR_REGISTRY_ERR_DUPLICATE_NAME;
    }
    if (registry->count == registry->capacity) {
        size_t capacity = registry->capacity == 0 ? 8 : registry->capacity * 2;
        const Generator **grown = (const Generator **)realloc((void *)registry->generators, capacity * sizeof(*grown));
        if (grown == NULL) {
            return GENERATOR_REGISTRY_ERR_ALLOC;
        }
        registry->generators = grown;
        registry->capacity = capacity;
    }
    registry->generators[registry->count++] = generator;
    return GENERATOR_REGISTRY_OK;
}

const Generator *generator_registry_lookup(const GeneratorRegistry *registry, const char *name) {
    if (registry == NULL || name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->generators[i]->name, name) == 0) {
            return registry->generators[i];
        }
    }
    return NULL;
}

size_t generator_registry_count(const GeneratorRegistry *registry) {
    return registry == NULL ? 0 : registry->count;
}

const Generator *generator_registry_at(const GeneratorRegistry *registry, size_t index) {
    if (registry == NULL || index >= registry->count) {
        return NULL;
    }
    return registry->generators[index];
}
