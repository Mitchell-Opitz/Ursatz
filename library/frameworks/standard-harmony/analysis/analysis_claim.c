#include "analysis_claim.h"

#include <stdbool.h>


#include "harmony_framework.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "theory/framework.h"

Interpretation *harmony_analysis_make_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    const Framework *framework = standard_harmony_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "standard-harmony", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
}

Structure *harmony_analysis_note_group(const char *shell_id, const char *const *note_ids, size_t count) {
    return claim_interpretation_note_group("standard-harmony", shell_id, note_ids, count);
}
