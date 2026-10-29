#include "containers/section.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Section {
    EntityID *id;
    EntityID **phrase_ids;
    size_t phrase_id_count;
};

SectionResult section_create(EntityID *id, EntityID **phrase_ids, size_t phrase_id_count, Section **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, phrase_ids, phrase_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return SECTION_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return SECTION_ERR_NULL_PHRASE_ID;
        default:
            return SECTION_ERR_ALLOC;
    }

    Section *section = (Section *)malloc(sizeof(Section));
    if (section == NULL) {
        free(copy);
        return SECTION_ERR_ALLOC;
    }

    section->phrase_ids = copy;
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
    id_array_destroy(section->phrase_ids, section->phrase_id_count);
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