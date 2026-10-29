#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_CHECK_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_CHECK_H

#include <stdbool.h>
#include <stddef.h>

#include "core/rational.h"
#include "interpretation/interpretation.h"
#include "meter_analysis_input.h"
#include "time/meter_map.h"

/*
 * The meter check: whether the harmonic rhythm and the cadences read better in another meter than the one the music declares.
 * The evidence is neutral: the times (whole notes) at which the harmony changes, the times cadences arrive, and every note
 * onset. The effective reading is the MeterMap's first full measure (scale 1, the length of a leading pickup region as the
 * pickup). Every candidate (2/4, 3/4, 4/4, 3/8, 6/8, 9/8, 12/8, each at scale 1/2, 1 and 2, with each beat offset as a
 * pickup) is scored in [0, 1] from the share of the changes on its downbeats and beats and of the arrivals on its downbeats.
 * The best one that lays different barlines is offered when it beats the effective reading by STANDARD_METER_CHECK_MIN_MARGIN;
 * it is offered, never applied. Of equal scores with equal barlines the one nearest the common onset interval of an eighth
 * note is shown, then the quarter-beat signature. Port of the Analyzer's meter check, on exact Rational; an exact time that
 * does not fit is STANDARD_METER_ANALYSIS_ERR_OVERFLOW, never wrapped.
 *
 * The claim is an Interpretation of type "standard-meter/meter-check" holding a StandardMeterCheck; it has no notes, so the
 * caller names its target (the piece). Its uncertainty is the check's.
 */

#define STANDARD_METER_CHECK_MIN_MARGIN 0.15
#define STANDARD_METER_CHECK_CLAIM_TYPE "standard-meter/meter-check"

typedef struct {
    const Rational *changes; /* in time order */
    size_t change_count;
    const Rational *arrivals;
    size_t arrival_count;
    const Rational *onsets; /* every note onset; unsorted allowed */
    size_t onset_count;
} StandardMeterCheckEvidence;

typedef struct {
    unsigned beats;
    unsigned beat_type;
    Rational scale;
    Rational pickup;
    double score;
} StandardMeterReading;

typedef struct {
    bool present;
    StandardMeterReading effective;
    bool has_other;
    StandardMeterReading other;
    bool has_alternative; /* other beats effective by STANDARD_METER_CHECK_MIN_MARGIN */
    double uncertainty;
} StandardMeterCheck;

/* Runs the check. `present` is false (and the status OK) with no changes or a map with no full measure. *out is zeroed first. */
StandardMeterAnalysisStatus standard_meter_check_meter(const MeterMap *map, const StandardMeterCheckEvidence *evidence,
                                                       StandardMeterCheck *out);

/*
 * The times at which harmony changes, from entries in time order: entry 0, and every entry whose region differs from the
 * previous one (-1 is "no region") or whose label differs (NULL is unlabelled; NULL and NULL are equal). `regions` may be NULL
 * for none. *out is owned, freed with free; NULL with *out_count 0 for no entries.
 */
StandardMeterAnalysisStatus standard_meter_check_changes(const char *const *labels, const long *regions, const Rational *times,
                                                         size_t count, Rational **out, size_t *out_count);

/* An Interpretation of the check targeting `target_id`. NULL on a bad argument, a check that is not present, or failure. */
Interpretation *standard_meter_check_interpretation(const StandardMeterCheck *check, const char *target_id);

/* Reads the check back from its Interpretation. False when it is not a meter-check claim. */
bool standard_meter_check_claim_read(const Interpretation *interpretation, StandardMeterCheck *out);

#endif
