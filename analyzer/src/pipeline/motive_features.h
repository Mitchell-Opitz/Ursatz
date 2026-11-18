#ifndef URSATZ_ANALYZER_MOTIVE_FEATURES_H
#define URSATZ_ANALYZER_MOTIVE_FEATURES_H

#include <stdbool.h>

#include <chain/chain_result.h>

/* The degree of a pitch class against the major scale of the tonic: "1 b2 2 b3 3 4 #4 5 b6 6 b7 7". Static. */
const char *motive_features_degree_label(int pitch_class, int tonic_pc);

#endif
