#include "relationship/predicate_definition.h"

#include <stdlib.h>
#include <string.h>

struct PredicateDefinition {
    EntityID *id;
    char *name;
    char *inverse;
    bool symmetric;
    bool transitive;
    char **allowed_source_types;
    size_t allowed_source_type_count;
    char **allowed_target_types;
    size_t allowed_target_type_count;
    bool contextual;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

static PredicateDefinitionResult copy_type_list(const char **types, size_t count, char ***out) {
    if (count == 0) {
        *out = NULL;
        return PREDICATE_DEFINITION_OK;
    }
    if (types == NULL) {
        return PREDICATE_DEFINITION_ERR_NULL_TYPE_ENTRY;
    }
    for (size_t i = 0; i < count; i++) {
        if (types[i] == NULL) {
            return PREDICATE_DEFINITION_ERR_NULL_TYPE_ENTRY;
        }
    }

    char **copies = (char **)malloc(sizeof(char *) * count);
    if (copies == NULL) {
        return PREDICATE_DEFINITION_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        copies[i] = copy_string(types[i]);
        if (copies[i] == NULL) {
            for (size_t j = 0; j < i; j++) {
                free(copies[j]);
            }
            free(copies);
            return PREDICATE_DEFINITION_ERR_ALLOC;
        }
    }

    *out = copies;
    return PREDICATE_DEFINITION_OK;
}

static void free_type_list(char **types, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(types[i]);
    }
    free(types);
}

PredicateDefinitionResult predicate_definition_create(EntityID *id, const char *name, const char *inverse,
                                                       bool symmetric, bool transitive,
                                                       const char **allowed_source_types,
                                                       size_t allowed_source_type_count,
                                                       const char **allowed_target_types,
                                                       size_t allowed_target_type_count, bool contextual,
                                                       PredicateDefinition **out) {
    if (id == NULL) {
        return PREDICATE_DEFINITION_ERR_NULL_ID;
    }
    if (name == NULL || name[0] == '\0') {
        return PREDICATE_DEFINITION_ERR_NULL_NAME;
    }

    PredicateDefinition *definition = (PredicateDefinition *)malloc(sizeof(PredicateDefinition));
    if (definition == NULL) {
        return PREDICATE_DEFINITION_ERR_ALLOC;
    }

    definition->name = copy_string(name);
    if (definition->name == NULL) {
        free(definition);
        return PREDICATE_DEFINITION_ERR_ALLOC;
    }

    definition->inverse = NULL;
    if (inverse != NULL) {
        definition->inverse = copy_string(inverse);
        if (definition->inverse == NULL) {
            free(definition->name);
            free(definition);
            return PREDICATE_DEFINITION_ERR_ALLOC;
        }
    }

    PredicateDefinitionResult source_result =
        copy_type_list(allowed_source_types, allowed_source_type_count, &definition->allowed_source_types);
    if (source_result != PREDICATE_DEFINITION_OK) {
        free(definition->inverse);
        free(definition->name);
        free(definition);
        return source_result;
    }

    PredicateDefinitionResult target_result =
        copy_type_list(allowed_target_types, allowed_target_type_count, &definition->allowed_target_types);
    if (target_result != PREDICATE_DEFINITION_OK) {
        free_type_list(definition->allowed_source_types, allowed_source_type_count);
        free(definition->inverse);
        free(definition->name);
        free(definition);
        return target_result;
    }

    definition->id = id;
    definition->symmetric = symmetric;
    definition->transitive = transitive;
    definition->allowed_source_type_count = allowed_source_type_count;
    definition->allowed_target_type_count = allowed_target_type_count;
    definition->contextual = contextual;

    *out = definition;
    return PREDICATE_DEFINITION_OK;
}

void predicate_definition_destroy(PredicateDefinition *definition) {
    if (definition == NULL) {
        return;
    }
    entity_id_destroy(definition->id);
    free(definition->name);
    free(definition->inverse);
    free_type_list(definition->allowed_source_types, definition->allowed_source_type_count);
    free_type_list(definition->allowed_target_types, definition->allowed_target_type_count);
    free(definition);
}

const EntityID *predicate_definition_id(const PredicateDefinition *definition) {
    return definition->id;
}

const char *predicate_definition_name(const PredicateDefinition *definition) {
    return definition->name;
}

const char *predicate_definition_inverse(const PredicateDefinition *definition) {
    return definition->inverse;
}

bool predicate_definition_symmetric(const PredicateDefinition *definition) {
    return definition->symmetric;
}

bool predicate_definition_transitive(const PredicateDefinition *definition) {
    return definition->transitive;
}

size_t predicate_definition_allowed_source_type_count(const PredicateDefinition *definition) {
    return definition->allowed_source_type_count;
}

const char *predicate_definition_allowed_source_type_at(const PredicateDefinition *definition, size_t index) {
    return definition->allowed_source_types[index];
}

size_t predicate_definition_allowed_target_type_count(const PredicateDefinition *definition) {
    return definition->allowed_target_type_count;
}

const char *predicate_definition_allowed_target_type_at(const PredicateDefinition *definition, size_t index) {
    return definition->allowed_target_types[index];
}

bool predicate_definition_contextual(const PredicateDefinition *definition) {
    return definition->contextual;
}