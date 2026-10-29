#include "analysis_claim.h"

#include <stdio.h>

#include "core/entity.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "theory/framework.h"

Structure *standard_form_analysis_group(const char *shell_id, const char *const *note_ids, size_t count) {
    return claim_interpretation_note_group("standard-form", shell_id, note_ids, count);
}

Interpretation *standard_form_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    const Framework *framework = standard_form_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "standard-form", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
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
    return claim_interpretation_relationship("standard-form", kind, key, source_id, target_id, predicate);
}
