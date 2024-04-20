#include "analysis_claim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/entity.h"
#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "interpretation/uncertainty.h"
#include "theory/framework.h"

#define CONFIDENCE_DENOMINATOR 1000000UL
#define SHELL_TYPE "standard-form/note-group"

Structure *standard_form_analysis_group(const char *shell_id, const char *const *note_ids, size_t count) {
    if (shell_id == NULL || shell_id[0] == '\0' || note_ids == NULL || count == 0) {
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
        entity_create(id, SHELL_TYPE, version, NULL, provenance, &entity) == ENTITY_OK &&
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

Interpretation *standard_form_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    if (interpretation_id == NULL || interpretation_id[0] == '\0' || target_id == NULL || target_id[0] == '\0' ||
        claim_type == NULL || content == NULL || size == 0 || method == NULL || method[0] == '\0' ||
        !(uncertainty_value >= 0.0 && uncertainty_value <= 1.0)) {
        return NULL;
    }
    const Framework *framework = standard_form_framework();
    if (framework == NULL) {
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
        entity_id_create(entity_id_value(framework_id(framework)), &framework_target) != ENTITY_ID_OK) {
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
    if (interpretation_create(id, target, claim, NULL, NULL, framework_reference, uncertainty, "standard-form-v1", method,
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

bool standard_form_analysis_emit(const char *kind, const char *key, const char *const *note_ids, size_t note_count,
                                 const char *claim_type, const void *claim, size_t claim_size, double uncertainty,
                                 Structure **shell, Interpretation **interpretation) {
    if (kind == NULL || key == NULL || shell == NULL || interpretation == NULL) {
        return false;
    }
    char shell_id[400];
    char interpretation_id[400];
    snprintf(shell_id, sizeof(shell_id), "standard-form-%s-notes-%s", kind, key);
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-form-%s-%s", kind, key);
    Structure *made_shell = standard_form_analysis_group(shell_id, note_ids, note_count);
    if (made_shell == NULL) {
        return false;
    }
    Interpretation *made =
        standard_form_analysis_interpretation(interpretation_id, entity_id_value(entity_id(structure_entity(made_shell))),
                                              claim_type, claim, claim_size, uncertainty, kind);
    if (made == NULL) {
        structure_destroy(made_shell);
        return false;
    }
    *shell = made_shell;
    *interpretation = made;
    return true;
}

Relationship *standard_form_analysis_relationship(const char *kind, const char *key, const char *source_id,
                                                  const char *target_id, const char *predicate) {
    if (kind == NULL || key == NULL || source_id == NULL || target_id == NULL || predicate == NULL ||
        predicate[0] == '\0') {
        return NULL;
    }
    char relationship_id[400];
    snprintf(relationship_id, sizeof(relationship_id), "standard-form-%s-%s", kind, key);
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
