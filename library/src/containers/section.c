#include "containers/section.h"

#include <stdlib.h>

struct Section {
    EntityID *id;
    EntityID **phrase_ids;
    size_t phrase_id_count;
};

static EntityID **copy_id_array(EntityID **ids, size_t count) {
    if (count == 0) {
        return NULL;
    }
    EntityID **copy = (EntityID **)malloc(sizeof(EntityID *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = ids[i];
    }
    return copy;
}

static void destroy_id_array(EntityID **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

SectionResult section_create(EntityID *id, EntityID **phrase_ids, size_t phrase_id_count, Section **out) {
    if (id == NULL) {
        return SECTION_ERR_NULL_ID;
    }
    for (size_t i = 0; i < phrase_id_count; i++) {
        if (phrase_ids[i] == NULL) {
            return SECTION_ERR_NULL_PHRASE_ID;
        }
    }

    Section *section = (Section *)malloc(sizeof(Section));
    if (section == NULL) {
        return SECTION_ERR_ALLOC;
    }

    section->phrase_ids = copy_id_array(phrase_ids, phrase_id_count);
    if (phrase_id_count > 0 && section->phrase_ids == NULL) {
        free(section);
        return SECTION_ERR_ALLOC;
    }

    section->id = id;
    section->phrase_id_count = phrase_id_count;

    *out = section;
    return SECTION_OK;
}

void section_destroy(Section *section) {
    if (section == NULL) {
        return;
    }
    entity_id_destroy(section->id);
    destroy_id_array(section->phrase_ids, section->phrase_id_count);
    free(section);
}

const EntityID *section_id(const Section *section) {
    return section->id;
}

size_t section_phrase_id_count(const Section *section) {
    return section->phrase_id_count;
}

const EntityID *section_phrase_id_at(const Section *section, size_t index) {
    return section->phrase_ids[index];
}

bool section_identity_equals(const Section *a, const Section *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}