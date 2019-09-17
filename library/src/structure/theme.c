#include "structure/theme.h"

#include <stdlib.h>

struct Theme {
    EntityID *id;
    EntityID **phrase_ids;
    size_t phrase_id_count;
};

ThemeResult theme_create(EntityID *id, EntityID **phrase_ids, size_t phrase_id_count, Theme **out) {
    if (id == NULL) {
        return THEME_ERR_NULL_ID;
    }
    if (phrase_ids == NULL || phrase_id_count == 0) {
        return THEME_ERR_NULL_PHRASE_IDS;
    }
    for (size_t i = 0; i < phrase_id_count; i++) {
        if (phrase_ids[i] == NULL) {
            return THEME_ERR_NULL_PHRASE_ID;
        }
    }

    Theme *theme = (Theme *)malloc(sizeof(Theme));
    if (theme == NULL) {
        return THEME_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * phrase_id_count);
    if (owned_ids == NULL) {
        free(theme);
        return THEME_ERR_ALLOC;
    }
    for (size_t i = 0; i < phrase_id_count; i++) {
        owned_ids[i] = phrase_ids[i];
    }

    theme->id = id;
    theme->phrase_ids = owned_ids;
    theme->phrase_id_count = phrase_id_count;

    *out = theme;
    return THEME_OK;
}

void theme_destroy(Theme *theme) {
    if (theme == NULL) {
        return;
    }
    entity_id_destroy(theme->id);
    for (size_t i = 0; i < theme->phrase_id_count; i++) {
        entity_id_destroy(theme->phrase_ids[i]);
    }
    free(theme->phrase_ids);
    free(theme);
}

const EntityID *theme_id(const Theme *theme) {
    return theme->id;
}

size_t theme_phrase_id_count(const Theme *theme) {
    return theme->phrase_id_count;
}

const EntityID *theme_phrase_id_at(const Theme *theme, size_t index) {
    return theme->phrase_ids[index];
}

bool theme_identity_equals(const Theme *a, const Theme *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}