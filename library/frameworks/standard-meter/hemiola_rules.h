#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_HEMIOLA_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_HEMIOLA_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Hemiola rules for standard-meter-v1. A hemiola is a stretch of triple meter that is felt as three
 * duple units: six pulses that the meter groups 3 + 3 are heard 2 + 2 + 2. The pulse is the beat of a
 * simple triple meter (3/4, 3/2) and the eighth of 6/8, which the meter reads as two groups of three.
 * Pulse indices run 0 to 5 across the window.
 *
 *   METER      where a hemiola can stand and the window it fills: six pulses, which are two measures of
 *              3/4 or 3/2 (any simple triple meter) and one measure of 6/8 (any compound duple meter).
 *              Every other signature, 12/8 and the irregular meters included, has no hemiola here
 *   DURATION   evidence from the attacks: the window's attacks fall on the starts of the three duple
 *              units: indices 2 and 4 are attacked and 1, 3 and 5 are not (index 0 is free, because the
 *              window starts on the downbeat in both readings)
 *   CHORD      evidence from the harmony: the chord changes fall on those same pulses (indices 2 and 4
 *              change, 1, 3 and 5 do not)
 *   ACCENT     evidence from the long notes: notes of two pulses or more begin at indices 2 and 4, and no
 *              note at all is attacked at index 3, the pulse the triple reading accents
 *   EVIDENCE   how many of the evidence types must agree, and how many are available: a type whose input
 *              the caller does not have is not available, and the count needed is never less than
 *              the minimum, so one type alone is never a hemiola
 *
 * The inputs are flags the caller has already read from the music; this framework does not know what a
 * note or a chord is.
 */

#define STANDARD_METER_HEMIOLA_PULSES 6

typedef enum {
    STANDARD_METER_HEMIOLA_RULE_METER = 0,
    STANDARD_METER_HEMIOLA_RULE_DURATION,
    STANDARD_METER_HEMIOLA_RULE_CHORD,
    STANDARD_METER_HEMIOLA_RULE_ACCENT,
    STANDARD_METER_HEMIOLA_RULE_EVIDENCE,
    STANDARD_METER_HEMIOLA_RULE_COUNT
} StandardMeterHemiolaRule;

const Rule *standard_meter_hemiola_rule(StandardMeterHemiolaRule which);
const Constraint *standard_meter_hemiola_constraint(StandardMeterHemiolaRule which);

/*
 * METER: the pulse length and the number of measures a hemiola window fills in this signature. True for a
 * simple triple meter (a top number of 3: the pulse is the bottom unit, two measures) and for a compound
 * duple meter (6/8: the pulse is the bottom unit, one measure). False, leaving the outputs untouched, for
 * any other signature, for a NULL argument or on allocation failure. On success the caller owns *out_pulse.
 */
bool standard_meter_hemiola_window(const TimeSignature *time_signature, Duration **out_pulse, unsigned *out_measures);

/* DURATION: the attacks (a flag per pulse of the window) read as three duple units. */
bool standard_meter_hemiola_duration_evidence(const bool attack[STANDARD_METER_HEMIOLA_PULSES]);

/* CHORD: the chord changes (a flag per pulse) read as three duple units. */
bool standard_meter_hemiola_chord_evidence(const bool change[STANDARD_METER_HEMIOLA_PULSES]);

/* ACCENT: the long-note onsets and the attacks of any note (a flag per pulse each). */
bool standard_meter_hemiola_accent_evidence(const bool long_onset[STANDARD_METER_HEMIOLA_PULSES],
                                            const bool attack[STANDARD_METER_HEMIOLA_PULSES]);

/* EVIDENCE: true when `available` evidence types are at least `minimum` and `agreeing` of them are at
   least `minimum`. A minimum of 0 is read as 1. */
bool standard_meter_hemiola_evidence_met(unsigned agreeing, unsigned available, unsigned minimum);

#endif
