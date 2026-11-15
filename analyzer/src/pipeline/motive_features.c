#include "pipeline/motive_features.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/style_labels.h>

const char *motive_features_degree_label(int pitch_class, int tonic_pc) {
    return style_label_degree(pitch_class, tonic_pc);
}

void motive_features_destroy(AnalysisMotiveFeatures *features) {
    if (!features) {
        return;
    }
    if (features->ncts) {
        for (size_t i = 0; i < features->note_count; i++) {
            free(features->ncts[i]);
        }
    }
    free(features->ncts);
    free(features->notes);
    free(features->degrees);
    free(features->intervals);
    free(features->steps);
    memset(features, 0, sizeof(*features));
}
