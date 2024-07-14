#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_HEMIOLA_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_HEMIOLA_H

#include "interpretation/interpretation.h"
#include "meter_analysis_input.h"
#include "structure/structure.h"

/*
 * Hemiola from real music (standard-meter's hemiola rules, hemiola_rules.h). Each window the rule defines for the measure's
 * signature is read in turn: two consecutive measures of a simple triple meter (3/4, 3/2), or one measure of 6/8. Its six
 * pulses are checked for the three evidence types, all notes taken as one stream:
 *
 *   duration   the window's attacks fall on the starts of three duple units
 *   chord      the chord changes (the input's, when it has any) fall on those pulses; unavailable with none
 *   accent     long notes (two pulses or more) begin on the second and third duple unit, and no note is attacked on pulse 4
 *
 * It is a hemiola when at least config.hemiola_min_evidence of the available types agree. An attack that is not on a pulse
 * (an ornament, a run) is ignored by all three. Windows do not overlap: after one is found the search resumes after it. Pickup
 * measures and measures that are not full (a short measure, a change of signature inside the window) are not read. Signatures
 * with no hemiola (12/8, 4/4, the irregular meters) report none. Times are snapped per the configuration.
 *
 * The claim is an Interpretation targeting a Structure shell over the notes attacked on the window's pulses: claim type
 * "standard-meter/hemiola". Its uncertainty is 1 - agreeing / 3.
 */

#define STANDARD_METER_HEMIOLA_CLAIM_TYPE "standard-meter/hemiola"

typedef struct {
    int first_measure;
    int last_measure; /* equal to first_measure in 6/8 */
    int duration_evidence; /* 1 agrees, 0 does not */
    int chord_evidence;    /* 1 agrees, 0 does not, -1 unavailable (no chord changes in the input) */
    int accent_evidence;   /* 1 agrees, 0 does not */
    unsigned agreeing;
    unsigned available;
} StandardMeterHemiolaClaim;

typedef struct {
    StandardMeterHemiolaClaim claim;
    double uncertainty;
    Structure *shell;               /* owned */
    Interpretation *interpretation; /* owned */
} StandardMeterHemiola;

typedef struct {
    StandardMeterHemiola *items; /* in time order */
    size_t count;
} StandardMeterHemiolas;

/* Finds the hemiolas. `config` may be NULL for the defaults. *out is untouched on failure. */
StandardMeterAnalysisStatus standard_meter_find_hemiolas(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                         StandardMeterHemiolas *out);

void standard_meter_hemiolas_destroy(StandardMeterHemiolas *hemiolas);

#endif
