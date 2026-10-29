#include "interpretation/claim_interpretation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/entity.h"
#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "interpretation/uncertainty.h"

#define CONFIDENCE_DENOMINATOR 1000000UL

Interpretation *claim_interpretation_create(const char *framework_id, const char *framework_name,
                                            const char *interpretation_id, const char *target_id, const char *claim_type,
                                            const void *content, size_t size, double uncertainty_value, const char *method) {
    if (framework_id == NULL || framework_id[0] == '\0' || framework_name == NULL || framework_name[0] == '\0' ||
        interpretation_id == NULL || interpretation_id[0] == '\0' || target_id == NULL || target_id[0] == '\0' ||
        claim_type == NULL || content == NULL || size == 0 || method == NULL || method[0] == '\0' ||
        !(uncertainty_value >= 0.0 && uncertainty_value <= 1.0)) {
        return NULL;
    }
    char author[400];
    if (snprintf(author, sizeof(author), "%s-v1", framework_name) >= (int)sizeof(author)) {
        return NULL;
    }
    EntityID *id = NULL;
    EntityID *target = NULL;
    EntityID *framework_target = NULL;
    FrameworkReference *framework_reference = NULL;
    Value *claim = NULL;
    Uncertainty *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    Interpretation *interpretation = NULL;

    if (entity_id_create(interpretation_id, &id) != ENTITY_ID_OK || entity_id_create(target_id, &target) != ENTITY_ID_OK ||
        entity_id_create(framework_id, &framework_target) != ENTITY_ID_OK) {
        goto fail;
    }
    if (framework_reference_create(framework_target, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        goto fail;
    }
    framework_target = NULL; /* owned by the reference now */
    unsigned long confidence = (unsigned long)((1.0 - uncertainty_value) * (double)CONFIDENCE_DENOMINATOR + 0.5);
    if (value_create(claim_type, content, size, &claim) != VALUE_OK ||
        uncertainty_create_scalar_confidence(confidence, CONFIDENCE_DENOMINATOR, &uncertainty) != UNCERTAINTY_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        goto fail;
    }
    if (interpretation_create(id, target, claim, NULL, NULL, framework_reference, uncertainty, author, method,
                              INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) != INTERPRETATION_OK) {
        goto fail;
    }
    return interpretation;

fail:
    entity_id_destroy(id);
    entity_id_destroy(target);
    entity_id_destroy(framework_target);
    framework_reference_destroy(framework_reference);
    value_destroy(claim);
    uncertainty_destroy(uncertainty);
    provenance_reference_destroy(provenance);
    return NULL;
}

Structure *claim_interpretation_note_group(const char *framework_name, const char *shell_id, const char *const *note_ids,
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

Relationship *claim_interpretation_relationship(const char *framework_name, const char *kind, const char *key,
                                                const char *source_id, const char *target_id, const char *predicate) {
    if (framework_name == NULL || kind == NULL || key == NULL || source_id == NULL || target_id == NULL ||
        predicate == NULL || predicate[0] == '\0') {
        return NULL;
    }
    char relationship_id[400];
    snprintf(relationship_id, sizeof(relationship_id), "%s-%s-%s", framework_name, kind, key);
    EntityID *id = NULL;
    EntityID *source = NULL;
    EntityID *target = NULL;
    ProvenanceReference *provenance = NULL;
    Relationship *relationship = NULL;
    if (entity_id_create(relationship_id, &id) == ENTITY_ID_OK && entity_id_create(source_id, &source) == ENTITY_ID_OK &&
        entity_id_create(target_id, &target) == ENTITY_ID_OK &&
        provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK &&
        relationship_create(id, source, predicate, target, NULL, NULL, NULL, NULL, NULL, provenance, &relationship) ==
            RELATIONSHIP_OK) {
        return relationship;
    }
    entity_id_destroy(id);
    entity_id_destroy(source);
    entity_id_destroy(target);
    provenance_reference_destroy(provenance);
    return NULL;
}
