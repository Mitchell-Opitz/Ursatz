#include "galant_exemplar.h"

bool galant_exemplar_valid(const GalantExemplar *exemplar) {
    if (exemplar == NULL || exemplar->stages == NULL || exemplar->tonic == NULL || exemplar->stage_count == 0 ||
        exemplar->stage_count > GALANT_MAX_STAGES) {
        return false;
    }
    for (size_t i = 0; i < exemplar->stage_count; i++) {
        const GalantExemplarStage *stage = &exemplar->stages[i];
        if (stage->melody == NULL || stage->bass == NULL || !beat_strength_valid(stage->strength)) {
            return false;
        }
    }
    return true;
}
