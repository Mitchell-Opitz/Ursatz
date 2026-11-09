#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_BUILD_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_BUILD_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_cadence.h"
#include "analysis_input.h"

/*
 * Building cadence results from plain data, for the files that make cadences beside analysis_cadence.c (the half
 * readings and the claims at phrase points, analysis_cadence_claims.h). A HarmonyCadenceSpec holds what the result
 * builder reads: the type, the dominant, a cadential 6/4, the arrival, the soprano degree and the competing reading.
 * The result built from it is the one harmony_analysis_find_cadences builds from the same data: the Cadence shell, the
 * Interpretation and the uncertainty by the formula in analysis_cadence.h.
 */

typedef struct {
    CadenceType type;
    size_t dominant_first;
    size_t dominant_last;
    bool has_six_four;
    size_t six_four_first;
    size_t six_four_last;
    size_t arrival_first;
    size_t arrival_last;
    int soprano_degree;
    bool post_cadential;
    double penalty; /* soprano_absent_penalty for an AUTHENTIC_UNSPECIFIED, else 0 */
    int share;      /* thousandths of the primary; 1000 with no alternative */
    bool has_alt;
    HarmonyCadenceReading alt_reading;
    CadenceType alt_type;
    size_t alt_arrival_first;
    size_t alt_arrival_last;
    int alt_soprano_degree;
    double alt_penalty;
} HarmonyCadenceSpec;

/* The result for `spec` (owned by the caller: harmony_cadence_result_destroy). ALLOC on failure, *out untouched. */
HarmonyCadenceAnalysisStatus harmony_cadence_result_build(const HarmonySegment *segments, const HarmonyCadenceSpec *spec,
                                                          HarmonyCadenceResult *out);

/* The data of an existing result, so a copy or a changed one can be built (config NULL for the defaults). */
void harmony_cadence_spec_of(const HarmonyCadenceResult *result, const HarmonyCadenceConfig *config,
                             HarmonyCadenceSpec *spec);

/* Frees what one result owns and clears it. */
void harmony_cadence_result_destroy(HarmonyCadenceResult *result);

/* Whether the config's numbers are usable (finite, not negative). */
bool harmony_cadence_config_is_valid(const HarmonyCadenceConfig *config);

#endif
