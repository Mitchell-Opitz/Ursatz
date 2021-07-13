#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_FIRST_SPECIES_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_FIRST_SPECIES_TEST_UTIL_H

#include "counterpoint_test_util.h"

/* A first-species exercise over Fux's Dorian cantus firmus (the lower
   part, D3 to D3), with a counterpoint above it that begins on sol and
   ends with a clausula vera. It breaks no hard rule. Notes are interned in
   the test pool, so these macros may be used once per test function. */
#define FIRST_SPECIES_LOWER(name)                                                                         \
    const PitchSpelling *name[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(G, 0, 3), P(F, 0, 3), \
                                   P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)}
#define FIRST_SPECIES_UPPER(name)                                                                         \
    const PitchSpelling *name[] = {P(A, 0, 3), P(D, 0, 4), P(C, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), \
                                   P(C, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 1, 4), P(D, 0, 4)}

#endif
