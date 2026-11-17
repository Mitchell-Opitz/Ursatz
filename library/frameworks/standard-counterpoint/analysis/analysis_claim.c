#include "analysis_claim.h"

#include <stdbool.h>

#include "theory/counterpoint/framework.h"
#include "model/identity/entity_id.h"
#include "model/interpretation/claim_interpretation.h"
#include "theory/framework/framework.h"

Structure *standard_counterpoint_analysis_note_group(const char *shell_id, const char *const *note_ids, size_t count) {
    return claim_interpretation_note_group("standard-counterpoint", shell_id, note_ids, count);
}

Interpretation *standard_counterpoint_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                              const char *claim_type, const void *content,
                                                              size_t size, double uncertainty_value,
                                                              const char *method) {
    const Framework *framework = standard_counterpoint_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "standard-counterpoint", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
}
