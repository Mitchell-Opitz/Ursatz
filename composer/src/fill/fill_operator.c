#include <ursatz_composer/fill_operator.h>

#include <stdlib.h>
#include <string.h>

bool fill_candidates_add(FillCandidateList *list, const char *label, void *payload, FillPayloadDestroyFn destroy) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 4;
        FillCandidate *items = realloc(list->items, capacity * sizeof(FillCandidate));
        if (!items) {
            if (destroy) {
                destroy(payload);
            }
            return false;
        }
        list->items = items;
        list->capacity = capacity;
    }
    FillCandidate *candidate = &list->items[list->count++];
    memset(candidate, 0, sizeof(*candidate));
    if (label) {
        strncpy(candidate->label, label, FILL_LABEL_MAX - 1);
    }
    candidate->payload = payload;
    candidate->destroy = destroy;
    return true;
}

void fill_candidates_clear(FillCandidateList *list) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].destroy) {
            list->items[i].destroy(list->items[i].payload);
        }
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

struct FillOperatorRegistry {
    FillOperator *operators;
    size_t count;
    size_t capacity;
};

FillOperatorRegistry *fill_registry_create(void) {
    return calloc(1, sizeof(FillOperatorRegistry));
}

void fill_registry_destroy(FillOperatorRegistry *registry) {
    if (registry) {
        free(registry->operators);
        free(registry);
    }
}

FillResult fill_registry_register(FillOperatorRegistry *registry, const FillOperator *operator_) {
    if (!registry || !operator_) {
        return FILL_ERR_NULL;
    }
    const char *name = operator_->generator.name;
    const char *version = operator_->generator.version;
    if (!name || name[0] == '\0' || !version || version[0] == '\0' || !operator_->applies || !operator_->generate ||
        !operator_->apply || operator_->kind < FILL_OPERATOR_REFINE || operator_->kind > FILL_OPERATOR_FILL) {
        return FILL_ERR_BAD_OPERATOR;
    }
    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->operators[i].generator.name, name) == 0) {
            return FILL_ERR_DUPLICATE_OPERATOR;
        }
    }
    if (registry->count == registry->capacity) {
        size_t capacity = registry->capacity ? registry->capacity * 2 : 8;
        FillOperator *grown = realloc(registry->operators, capacity * sizeof(FillOperator));
        if (!grown) {
            return FILL_ERR_ALLOC;
        }
        registry->operators = grown;
        registry->capacity = capacity;
    }
    registry->operators[registry->count++] = *operator_;
    return FILL_OK;
}

size_t fill_registry_count(const FillOperatorRegistry *registry) {
    return registry ? registry->count : 0;
}

const FillOperator *fill_registry_at(const FillOperatorRegistry *registry, size_t index) {
    return registry && index < registry->count ? &registry->operators[index] : NULL;
}
