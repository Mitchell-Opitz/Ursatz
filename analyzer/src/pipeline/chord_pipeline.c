#include "pipeline/chord_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "interpretation/chord.h"
#include "pipeline/interpretation_util.h"
#include "weighted_chord_identification.h"

bool identify_chords(const Segmentation *segmentation, const KeyEstimate *key, ChordEstimate **out) {
    size_t count = segmentation->segment_count;
    ChordEstimate *estimates = calloc(count > 0 ? count : 1, sizeof(ChordEstimate));
    if (!estimates) {
        return false;
    }

    if (count > 0 && key && key->found && key->is_major) {
        CommonPracticeWeightedResults results = {0};
        if (common_practice_identify_weighted_sequence(segmentation->library_segments, count,
                                                       key->tonic_pitch_class, COMMON_PRACTICE_KEY_MAJOR, NULL,
                                                       &results) == COMMON_PRACTICE_WEIGHTED_OK) {
            for (size_t i = 0; i < count; i++) {
                const CommonPracticeWeightedResult *result = &results.items[i];
                if (!result->matched) {
                    continue;
                }
                estimates[i].label = strdup(chord_label(result->best_chord));
                if (!estimates[i].label) {
                    common_practice_weighted_results_destroy(&results);
                    for (size_t k = 0; k < i; k++) {
                        chord_estimate_destroy(&estimates[k]);
                    }
                    free(estimates);
                    return false;
                }
                estimates[i].found = true;
                estimates[i].confidence = interpretation_confidence(chord_interpretation(result->best_chord));
            }
            common_practice_weighted_results_destroy(&results);
        }
    }

    *out = estimates;
    return true;
}

void chord_estimate_destroy(ChordEstimate *estimate) {
    if (!estimate) {
        return;
    }
    free(estimate->label);
    estimate->label = NULL;
    estimate->found = false;
}
