#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_BEAMING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_BEAMING_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/time_signature.h"

/*
 * Beaming rules for standard-meter-v1. A beam connects notes together,
 * grouping them by beat so the beat unit is visible:
 *
 *   BEAMABLE         only notes with flags -- the eighth note and
 *                    shorter -- are beamed; quarter notes and longer
 *                    never are
 *   NOTES_PER_BEAT   how many of a value make up one beat depends on the
 *                    time signature: four sixteenth notes per beat when
 *                    the beat is a quarter note, two when it is an
 *                    eighth, so the same sixteenths beam in fours in
 *                    x/4 and in twos in x/8
 *   WITHIN_BEAT      a beam group stays inside one beat, so beaming
 *                    changes with the time signature; mixed values
 *                    (eighths with sixteenths) may share a beam when
 *                    together they stay inside one beat
 *
 * Partial beams for mixed groupings and the vocal-music convention of
 * beaming only notes sung on one syllable are not covered. Where a note
 * that crosses a beat is split and tied instead is tie_rules.h in this
 * module.
 */

typedef enum {
    STANDARD_METER_BEAMING_RULE_BEAMABLE = 0,
    STANDARD_METER_BEAMING_RULE_NOTES_PER_BEAT,
    STANDARD_METER_BEAMING_RULE_WITHIN_BEAT,
    STANDARD_METER_BEAMING_RULE_COUNT
} StandardMeterBeamingRule;

const Rule *standard_meter_beaming_rule(StandardMeterBeamingRule which);
const Constraint *standard_meter_beaming_constraint(StandardMeterBeamingRule which);

/* BEAMABLE: true for the eighth note and shorter (dots do not change it). */
bool standard_meter_value_beamable(const NoteValue *note_value);

/*
 * NOTES_PER_BEAT: how many of this value fill one beat (2^(value
 * exponent - beat unit exponent)). False, leaving *out_count untouched,
 * when the value is dotted or longer than the beat.
 */
bool standard_meter_notes_per_beat(const TimeSignature *time_signature, const NoteValue *note_value,
                                   unsigned long *out_count);

/*
 * WITHIN_BEAT: true when a group starting `start` into the measure and
 * lasting `length` (a non-zero Duration) lies inside a single beat of
 * the measure -- it may end exactly on the beat boundary but not cross
 * it. False when the start is outside the measure.
 */
bool standard_meter_beam_group_within_beat(const TimeSignature *time_signature, const Duration *start,
                                           const Duration *length);

#endif
