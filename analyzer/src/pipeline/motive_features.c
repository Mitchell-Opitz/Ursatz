#include "pipeline/motive_features.h"

#include <ursatz_analyzer/style_labels.h>

const char *motive_features_degree_label(int pitch_class, int tonic_pc) {
    return style_label_degree(pitch_class, tonic_pc);
}

