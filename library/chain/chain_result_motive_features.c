#include "chain/chain_result_motive_features.h"

#include "theory/tonality/tonal_key.h"

const char *motive_features_degree_label(int pitch_class, int tonic_pc) {
    return standard_tonality_chromatic_degree_label(pitch_class - tonic_pc);
}

