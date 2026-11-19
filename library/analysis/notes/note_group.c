#include "analysis/notes/note_group.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/entity.h"
#include "model/core/provenance_reference.h"
#include "model/identity/entity_id.h"
#include "model/identity/schema_version.h"

Structure *analysis_note_group(const char *framework_name, const char *shell_id, const char *const *note_ids,
                                           size_t count) {
    if (framework_name == NULL || framework_name[0] == '\0' || shell_id == NULL || shell_id[0] == '\0' || note_ids == NULL ||
        count == 0) {
        return NULL;
    }
    char shell_type[400];
    if (snprintf(shell_type, sizeof(shell_type), "%s/note-group", framework_name) >= (int)sizeof(shell_type)) {
        return NULL;
    }
    EntityID **ids = (EntityID **)calloc(count, sizeof(EntityID *));
    if (ids == NULL) {
        return NULL;
    }
    size_t kept = 0;
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        if (note_ids[i] == NULL || note_ids[i][0] == '\0') {
            ok = false;
            break;
        }
        bool seen = false;
        for (size_t j = 0; j < i; j++) {
            seen = seen || strcmp(note_ids[j], note_ids[i]) == 0;
        }
        if (!seen) {
            ok = entity_id_create(note_ids[i], &ids[kept++]) == ENTITY_ID_OK;
        }
    }
    EntityID *id = NULL;
    SchemaVersion *version = NULL;
    ProvenanceReference *provenance = NULL;
    Entity *entity = NULL;
    Structure *structure = NULL;
    if (ok && entity_id_create(shell_id, &id) == ENTITY_ID_OK && schema_version_create(1, &version) == SCHEMA_VERSION_OK &&
        provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK &&
        entity_create(id, shell_type, version, NULL, provenance, &entity) == ENTITY_OK &&
        structure_create(entity, ids, kept, &structure) == STRUCTURE_OK) {
        free(ids); /* structure_create took the ids; the array is ours */
        return structure;
    }
    if (entity != NULL) {
        entity_destroy(entity);
    } else {
        entity_id_destroy(id);
        schema_version_destroy(version);
        provenance_reference_destroy(provenance);
    }
    for (size_t i = 0; i < kept; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
    return NULL;
}
