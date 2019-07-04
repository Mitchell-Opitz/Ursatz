#include "generation/intent.h"

#include <stdlib.h>
#include <string.h>

struct Intent {
    EntityID *id;
    char *purpose;
    char *target;
    char *style;
    EntityID *context_id;
    char *meter;
    char *length;
    Value **constraints;
    size_t constraint_count;
    Value **preferences;
    size_t preference_count;
    EntityID *source_material_id;
    ProvenanceReference *provenance;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

/* Returns false only on allocation failure; *out is NULL for a NULL value without that being a failure. */
static bool copy_optional_string(const char *value, char **out) {
    if (value == NULL) {
        *out = NULL;
        return true;
    }
    *out = copy_string(value);
    return *out != NULL;
}

static Value **copy_value_pointers(Value **values, size_t count) {
    Value **copy = (Value **)malloc(sizeof(Value *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = values[i];
    }
    return copy;
}

IntentResult intent_create(EntityID *id, const char *purpose, const char *target, const char *style,
                            EntityID *context_id, const char *meter, const char *length, Value **constraints,
                            size_t constraint_count, Value **preferences, size_t preference_count,
                            EntityID *source_material_id, ProvenanceReference *provenance, Intent **out) {
    if (id == NULL) {
        return INTENT_ERR_NULL_ID;
    }
    if (purpose == NULL || purpose[0] == '\0') {
        return INTENT_ERR_NULL_PURPOSE;
    }
    if (target == NULL || target[0] == '\0') {
        return INTENT_ERR_NULL_TARGET;
    }
    if (constraint_count > 0) {
        if (constraints == NULL) {
            return INTENT_ERR_NULL_CONSTRAINT;
        }
        for (size_t i = 0; i < constraint_count; i++) {
            if (constraints[i] == NULL) {
                return INTENT_ERR_NULL_CONSTRAINT;
            }
        }
    }
    if (preference_count > 0) {
        if (preferences == NULL) {
            return INTENT_ERR_NULL_PREFERENCE;
        }
        for (size_t i = 0; i < preference_count; i++) {
            if (preferences[i] == NULL) {
                return INTENT_ERR_NULL_PREFERENCE;
            }
        }
    }
    if (provenance == NULL) {
        return INTENT_ERR_NULL_PROVENANCE;
    }

    Intent *intent = (Intent *)malloc(sizeof(Intent));
    if (intent == NULL) {
        return INTENT_ERR_ALLOC;
    }

    intent->purpose = copy_string(purpose);
    if (intent->purpose == NULL) {
        free(intent);
        return INTENT_ERR_ALLOC;
    }

    intent->target = copy_string(target);
    if (intent->target == NULL) {
        free(intent->purpose);
        free(intent);
        return INTENT_ERR_ALLOC;
    }

    if (!copy_optional_string(style, &intent->style)) {
        free(intent->target);
        free(intent->purpose);
        free(intent);
        return INTENT_ERR_ALLOC;
    }

    if (!copy_optional_string(meter, &intent->meter)) {
        free(intent->style);
        free(intent->target);
        free(intent->purpose);
        free(intent);
        return INTENT_ERR_ALLOC;
    }

    if (!copy_optional_string(length, &intent->length)) {
        free(intent->meter);
        free(intent->style);
        free(intent->target);
        free(intent->purpose);
        free(intent);
        return INTENT_ERR_ALLOC;
    }

    intent->constraints = NULL;
    if (constraint_count > 0) {
        intent->constraints = copy_value_pointers(constraints, constraint_count);
        if (intent->constraints == NULL) {
            free(intent->length);
            free(intent->meter);
            free(intent->style);
            free(intent->target);
            free(intent->purpose);
            free(intent);
            return INTENT_ERR_ALLOC;
        }
    }
    intent->constraint_count = constraint_count;

    intent->preferences = NULL;
    if (preference_count > 0) {
        intent->preferences = copy_value_pointers(preferences, preference_count);
        if (intent->preferences == NULL) {
            free(intent->constraints);
            free(intent->length);
            free(intent->meter);
            free(intent->style);
            free(intent->target);
            free(intent->purpose);
            free(intent);
            return INTENT_ERR_ALLOC;
        }
    }
    intent->preference_count = preference_count;

    intent->id = id;
    intent->context_id = context_id;
    intent->source_material_id = source_material_id;
    intent->provenance = provenance;

    *out = intent;
    return INTENT_OK;
}

void intent_destroy(Intent *intent) {
    if (intent == NULL) {
        return;
    }
    entity_id_destroy(intent->id);
    free(intent->purpose);
    free(intent->target);
    free(intent->style);
    entity_id_destroy(intent->context_id);
    free(intent->meter);
    free(intent->length);
    for (size_t i = 0; i < intent->constraint_count; i++) {
        value_destroy(intent->constraints[i]);
    }
    free(intent->constraints);
    for (size_t i = 0; i < intent->preference_count; i++) {
        value_destroy(intent->preferences[i]);
    }
    free(intent->preferences);
    entity_id_destroy(intent->source_material_id);
    provenance_reference_destroy(intent->provenance);
    free(intent);
}

const EntityID *intent_id(const Intent *intent) {
    return intent->id;
}

const char *intent_purpose(const Intent *intent) {
    return intent->purpose;
}

const char *intent_target(const Intent *intent) {
    return intent->target;
}

const char *intent_style(const Intent *intent) {
    return intent->style;
}

const EntityID *intent_context_id(const Intent *intent) {
    return intent->context_id;
}

const char *intent_meter(const Intent *intent) {
    return intent->meter;
}

const char *intent_length(const Intent *intent) {
    return intent->length;
}

size_t intent_constraint_count(const Intent *intent) {
    return intent->constraint_count;
}

const Value *intent_constraint_at(const Intent *intent, size_t index) {
    return intent->constraints[index];
}

size_t intent_preference_count(const Intent *intent) {
    return intent->preference_count;
}

const Value *intent_preference_at(const Intent *intent, size_t index) {
    return intent->preferences[index];
}

const EntityID *intent_source_material_id(const Intent *intent) {
    return intent->source_material_id;
}

const ProvenanceReference *intent_provenance(const Intent *intent) {
    return intent->provenance;
}

bool intent_identity_equals(const Intent *a, const Intent *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}