#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_SYNCOPATION_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_SYNCOPATION_H

#include "interpretation/interpretation.h"
#include "meter_analysis_input.h"

/*
 * Syncopation from real music (standard-meter's THROUGH_STRONGER rule, syncopation_rules.h). A note is syncopated when it is
 * attacked at a weaker metric level and sustained through a stronger level, and no note of its stream is attacked at that
 * stronger position. A tie across a barline is one long note (the caller merges it), so it is found the same way. The stream is
 * one line at a time (config.syncopation_all_notes false) or all the notes together (true: a position another line attacks
 * does not count as sustained through).
 *
 * The strength is the difference of the two levels (1 to 4): an off-beat note held over the beat is 1, a note on a beat held
 * over the barline is 2, an off-beat note held over the barline is 3. A note is reported once, at the strongest position it is
 * held through (the first of them, if two have the same level). Times are snapped per the configuration, and a note whose onset
 * or end moved by the snap is reported with an uncertainty of 0.1 (otherwise 0).
 *
 * The claim is an Interpretation targeting the note id: claim type "standard-meter/syncopation".
 */

#define STANDARD_METER_SYNCOPATION_CLAIM_TYPE "standard-meter/syncopation"

typedef struct {
    char note_id[STANDARD_METER_ID_MAX];
    int line;
    int measure;
    double beat;           /* where the note is attacked: 1-based, fractional */
    int attack_level;      /* StandardMeterLevel */
    int through_level;     /* StandardMeterLevel of the position it is held through */
    unsigned strength;     /* attack_level - through_level */
    int through_measure;
    double through_beat;
} StandardMeterSyncopationClaim;

typedef struct {
    StandardMeterSyncopationClaim claim;
    double uncertainty;
    Interpretation *interpretation; /* owned */
} StandardMeterSyncopation;

typedef struct {
    StandardMeterSyncopation *items; /* in onset order within each stream, the streams in line order */
    size_t count;
} StandardMeterSyncopations;

/* Finds the syncopations. `config` may be NULL for the defaults. *out is untouched on failure. */
StandardMeterAnalysisStatus standard_meter_find_syncopations(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                             StandardMeterSyncopations *out);

void standard_meter_syncopations_destroy(StandardMeterSyncopations *syncopations);

#endif
