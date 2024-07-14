#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_HYPERMETER_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_HYPERMETER_H

#include "interpretation/interpretation.h"
#include "meter_analysis_input.h"
#include "relationship/relationship.h"

/*
 * Hypermeter from phrase and cadence placement (standard-meter's hypermeter rules, hypermeter_rules.h). Each phrase is read on
 * its own, in measures counted from the MeterMap (a pickup measure is not counted; a phrase that starts in one starts at the
 * first full measure):
 *
 *   length         the hypermeasure length, 2 or 4 measures, that fills the phrase and holds the cadence arrival in the last
 *                  hypermeasure (the LENGTH rule). The phrase's score is the confidence of every hypermeasure in it. A phrase of
 *                  1 or 3 measures fits neither length: it takes the better score, and the remainder is a short hypermeasure
 *   expansion      with an expansion from the form adapter the phrase is first read WITHOUT its added measures (an internal
 *                  expansion adds hyperbeats to the last hypermeasure, an external one adds a hypermeasure after it) and a
 *                  contraction is read with the measures it lacks (the last hypermeasure is the shorter by that many)
 *   elision        a phrase whose cadence is elided with the next, when a next phrase exists, gets a reinterpretation: its last
 *                  hyperbeat is the next phrase's first ("4 = 1"); the arrival measure is the next phrase's first measure
 *
 * Hyperdownbeats are the first measure of every hypermeasure. Hypermeasures are numbered from 0 in each phrase and tile the
 * phrase exactly.
 *
 * A hypermeasure is a Relationship over its measures (from the entity "standard-meter-measure-<first>" to
 * "standard-meter-measure-<last>", predicate "hypermeasure"); a reinterpretation is a Relationship from the ending phrase's id to
 * the next phrase's id (predicate "hypermetric-reinterpretation"). Each has an Interpretation targeting the Relationship's id,
 * with the claim types below and an uncertainty of 1 - score.
 */

#define STANDARD_METER_HYPERMEASURE_CLAIM_TYPE "standard-meter/hypermeasure"
#define STANDARD_METER_REINTERPRETATION_CLAIM_TYPE "standard-meter/hypermetric-reinterpretation"

typedef enum {
    STANDARD_METER_HYPERMEASURE_REGULAR = 0, /* the nominal length */
    STANDARD_METER_HYPERMEASURE_SHORT,       /* a remainder of fewer measures */
    STANDARD_METER_HYPERMEASURE_EXPANDED,    /* an internal expansion added hyperbeats */
    STANDARD_METER_HYPERMEASURE_EXTENSION,   /* an external expansion: a hypermeasure of its own after the cadence */
    STANDARD_METER_HYPERMEASURE_CONTRACTED   /* a contraction took hyperbeats away */
} StandardMeterHypermeasureKind;

typedef struct {
    int phrase;       /* the index of the phrase in the input */
    int hypermeasure; /* the index in the phrase, from 0 */
    int first_measure;
    int last_measure;
    int measures;
    unsigned nominal_length; /* the phrase's hypermeasure length, 2 or 4 */
    int kind;                /* StandardMeterHypermeasureKind */
    int extra_hyperbeats;    /* measures - nominal_length: positive when expanded, negative when short or contracted */
    int contains_cadence;    /* the cadence arrival is in this hypermeasure */
    double score;
} StandardMeterHypermeasureClaim;

typedef struct {
    StandardMeterHypermeasureClaim claim;
    double uncertainty;
    Relationship *relationship;     /* owned */
    Interpretation *interpretation; /* owned */
} StandardMeterHypermeasure;

typedef struct {
    int phrase;
    int next_phrase;
    int measure;              /* the elided measure: the ending phrase's last hyperbeat and the next phrase's first */
    unsigned count_in_ending; /* its hyperbeat count in the ending phrase (4 in "4 = 1") */
    unsigned count_in_next;   /* 1 */
} StandardMeterReinterpretationClaim;

typedef struct {
    StandardMeterReinterpretationClaim claim;
    double uncertainty;
    Relationship *relationship;     /* owned */
    Interpretation *interpretation; /* owned */
} StandardMeterReinterpretation;

/* What was chosen for one phrase. `hypermeasure_first` indexes the hypermeasures array. */
typedef struct {
    int phrase;
    int first_measure;
    int measures;
    unsigned length;
    double score;
    int arrival_measure; /* -1 for none or elided */
    size_t hypermeasure_first;
    size_t hypermeasure_count;
} StandardMeterPhraseHypermeter;

typedef struct {
    StandardMeterPhraseHypermeter *phrases; /* one per input phrase that has a measure, in order */
    size_t phrase_count;
    StandardMeterHypermeasure *hypermeasures; /* in time order */
    size_t hypermeasure_count;
    StandardMeterReinterpretation *reinterpretations;
    size_t reinterpretation_count;
} StandardMeterHypermeter;

/* Infers the hypermeter. `config` may be NULL for the defaults. *out is untouched on failure. */
StandardMeterAnalysisStatus standard_meter_find_hypermeter(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                           StandardMeterHypermeter *out);

void standard_meter_hypermeter_destroy(StandardMeterHypermeter *hypermeter);

#endif
