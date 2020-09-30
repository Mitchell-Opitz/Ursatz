#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_BEAT_TIE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_BEAT_TIE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Beat-boundary tie rules for standard-meter-v1. Ties join same-pitch
 * notes (that rule is standard-rhythm-v1's); here they also serve like
 * beams, to make the metrical structure visible within a measure:
 *
 *   BEAT_SPLIT  a note that starts off the beat and runs past the next
 *               beat hides where that beat begins: a quarter note
 *               starting on the "+" of beat 1 buries the start of beat
 *               2. Writing it as two eighth notes tied across the beat
 *               boundary shows the beat.
 *
 * A note that starts on a beat is not split, however many beats it spans
 * (a half note on beat 1 of 4/4 is written as one note). Beats follow the
 * meter class: in 6/8 a beat is a dotted quarter, so an eighth note
 * starting on the second division and running a sixteenth into the next
 * beat is split at the beat line. This is a
 * clarity convention, not an obligation: a tie across the beat is the
 * clearer way to write the rhythm, not the only legal one.
 */

typedef enum {
    STANDARD_METER_BEAT_TIE_RULE_BEAT_SPLIT = 0,
    STANDARD_METER_BEAT_TIE_RULE_COUNT
} StandardMeterBeatTieRule;

const Rule *standard_meter_beat_tie_rule(StandardMeterBeatTieRule which);
const Constraint *standard_meter_beat_tie_constraint(StandardMeterBeatTieRule which);

/*
 * BEAT_SPLIT: for a note starting `start` into the measure and lasting
 * `length` (non-zero), when it starts off the beat and runs past the
 * next beat boundary, returns true and sets *out_first to the length of
 * the part up to that boundary (the second part is the rest). Returns
 * false, leaving *out_first untouched, when there is nothing to split
 * -- it starts on a beat, or ends by the next boundary -- when the start
 * is outside the measure, the signature is unclassified, or on
 * allocation failure. On success the caller owns *out_first.
 */
bool standard_meter_beat_split(const TimeSignature *time_signature, const Duration *start, const Duration *length,
                               Duration **out_first);

#endif
