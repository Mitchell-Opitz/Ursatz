#include "analysis_melody_emit.h"

#include <stdio.h>

#include "analysis_claim.h"
#include "core/entity.h"
#include "identity/entity_id.h"

bool standard_counterpoint_melody_emit(const char *kind, const char *key, const char *const *note_ids,
                                       size_t note_count, const char *claim_type, const void *claim,
                                       size_t claim_size, double uncertainty, Structure **shell,
                                       Interpretation **interpretation) {
    if (kind == NULL || key == NULL || shell == NULL || interpretation == NULL) {
        return false;
    }
    char shell_id[400];
    char interpretation_id[400];
    snprintf(shell_id, sizeof(shell_id), "standard-counterpoint-melody-notes-%s-%s", kind, key);
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-counterpoint-%s-%s", kind, key);
    Structure *made_shell = standard_counterpoint_analysis_note_group(shell_id, note_ids, note_count);
    if (made_shell == NULL) {
        return false;
    }
    Interpretation *made = standard_counterpoint_analysis_interpretation(
        interpretation_id, entity_id_value(entity_id(structure_entity(made_shell))), claim_type, claim, claim_size,
        uncertainty, kind);
    if (made == NULL) {
        structure_destroy(made_shell);
        return false;
    }
    *shell = made_shell;
    *interpretation = made;
    return true;
}
