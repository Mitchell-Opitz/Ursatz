#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_DENSITY_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_DENSITY_H

#include "interpretation/interpretation.h"
#include "meter_analysis_input.h"

/*
 * Rhythmic density per phrase: how busy the phrase is and where its attacks fall. One profile per phrase for all its notes
 * (line -1) and one for each line that has a note in it, in line order. A note belongs to a phrase when its onset, snapped per
 * the configuration, is in the phrase's span (start included, end not).
 *
 *   notes, attacks   the notes, and the distinct onsets among them (a chord is one attack)
 *   attacks per beat   for each measure the phrase touches, the attacks inside the phrase's part of the measure over the beats
 *                      that part spans (the beat of the measure's reading, so a dotted quarter in 6/8); the mean of those over
 *                      the measures, and the largest
 *   mean inter-onset   the mean distance between consecutive attacks, in whole notes (0 with fewer than two attacks)
 *   on-beat share      the share of attacks that fall exactly on a beat, and the rest, the off-beat share (both 0 with no attack)
 *   durations          a histogram of the notes' durations as exact rationals, ascending, counting notes (a chord's notes each
 *                      count). At most STANDARD_METER_DENSITY_BINS distinct durations are kept, the shortest first, and
 *                      `bins_truncated` says some were left out
 *
 * The profile is a structured claim, "standard-meter/density", an Interpretation targeting the phrase's id with an uncertainty of
 * 0 (it describes the notes the caller gave, as they are).
 */

#define STANDARD_METER_DENSITY_CLAIM_TYPE "standard-meter/density"
#define STANDARD_METER_DENSITY_BINS 16

typedef struct {
    long numerator;
    unsigned long denominator;
    unsigned count;
} StandardMeterDurationBin;

typedef struct {
    int phrase; /* the index of the phrase in the input */
    int line;   /* -1 for all notes */
    unsigned notes;
    unsigned attacks;
    int measures;
    double attacks_per_beat_mean;
    double attacks_per_beat_max;
    double mean_inter_onset;
    double on_beat_share;
    double off_beat_share;
    unsigned bin_count;
    int bins_truncated;
    StandardMeterDurationBin bins[STANDARD_METER_DENSITY_BINS];
} StandardMeterDensityClaim;

typedef struct {
    StandardMeterDensityClaim claim;
    Interpretation *interpretation; /* owned */
} StandardMeterDensity;

typedef struct {
    StandardMeterDensity *items; /* by phrase, all notes first and then each line */
    size_t count;
} StandardMeterDensities;

/* Profiles every phrase. `config` may be NULL for the defaults. *out is untouched on failure. */
StandardMeterAnalysisStatus standard_meter_find_densities(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                          StandardMeterDensities *out);

void standard_meter_densities_destroy(StandardMeterDensities *densities);

#endif
