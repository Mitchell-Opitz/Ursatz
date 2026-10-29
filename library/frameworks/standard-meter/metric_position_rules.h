#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_METRIC_POSITION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_METRIC_POSITION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/beat_strength.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/time_signature.h"

/*
 * Metric position rules for standard-meter-v1: reading where a note falls in a measure as a beat, a
 * place inside that beat, and a BeatStrength. The kernel supplies the position as measure + offset
 * (time/metrical_position.h, a pure value with no beat claim); this framework reads it with the
 * signature's beat unit, so the same functions serve simple and compound meters:
 *
 *   BEAT_POSITION  the offset divides into the beat it falls in (1-based) and where it falls inside
 *                  that beat, an exact fraction of the beat (0 on the beat): 3/8 into 4/4 is beat 2
 *                  at 1/2, and 3/8 into 6/8 is beat 2 at 0. The beat comes from the meter class
 *                  (meter_class_rules.h, beat_grid.h): the bottom unit in simple meter, a dotted unit
 *                  in compound. An irregular meter (5/8, 7/8) is unclassified and has no grouping
 *                  defined here, so every bottom-unit pulse is a beat.
 *   METRIC_LEVEL   the position's level in the meter, from strongest to weakest:
 *                    downbeat    beat 1, on the beat
 *                    secondary   beat 3 of a quadruple meter (4/4, 12/8), on the beat
 *                    beat        any other beat, on the beat
 *                    off-beat    a place on the beat's division (the "and" at half a beat in simple
 *                                meter; the "la" and "li" at a third and two thirds in compound)
 *                    finer       anything between those (a subdivision, "e" and "a", "ta", and finer)
 *   BEAT_STRENGTH  the levels mapped onto the shared BeatStrength (semantics/beat_strength.h), which
 *                  has three values and gets none added: the downbeat is STRONG, the secondary beat
 *                  MEDIUM, and every other level WEAK. This is the convention of
 *                  frameworks/standard-counterpoint (four notes against one: strong, weak, medium,
 *                  weak), so a bar of 4/4 reads S W M W; a bar of 3/4 reads S W W. BeatStrength
 *                  alone cannot tell a beat from an off-beat; the metric level and the beat position
 *                  keep that distinction for a caller that needs it.
 *
 * Positions are offsets inside a measure, so the offset must be less than the measure's length (the
 * reading signature's: see metrical_position_reading_signature in time/metrical_position.h, which
 * gives the signature to pass for a pickup measure). Every function is false, leaving its output
 * untouched, for a NULL argument, an offset at or past the end of the measure, an offset whose
 * arithmetic does not fit (checked, never wrapped), or on allocation failure.
 */

typedef enum {
    STANDARD_METER_METRIC_RULE_BEAT_POSITION = 0,
    STANDARD_METER_METRIC_RULE_METRIC_LEVEL,
    STANDARD_METER_METRIC_RULE_BEAT_STRENGTH,
    STANDARD_METER_METRIC_RULE_COUNT
} StandardMeterMetricRule;

typedef enum {
    STANDARD_METER_LEVEL_DOWNBEAT = 0,
    STANDARD_METER_LEVEL_SECONDARY_BEAT,
    STANDARD_METER_LEVEL_BEAT,
    STANDARD_METER_LEVEL_OFFBEAT,
    STANDARD_METER_LEVEL_FINER
} StandardMeterLevel;

/* The beat a position falls in and where it falls inside that beat. */
typedef struct {
    unsigned beat;                          /* 1-based */
    long subdivision_numerator;             /* the place inside the beat, an exact fraction of the beat */
    unsigned long subdivision_denominator;  /* in lowest terms; 0 over 1 is on the beat */
} StandardMeterBeatPosition;

/*
 * The beat grid a signature is read with here: how long one beat is and how many a measure holds. A
 * classified meter takes both from the meter class (meter_class_rules.h): the bottom unit in simple
 * meter, a dotted unit in compound. An unclassified one (5/8, 7/8) has no grouping defined, so each
 * bottom-unit pulse is a beat. False, leaving the outputs untouched, for a NULL argument or on
 * allocation failure. On success the caller owns *out_beat.
 */
bool standard_meter_beat_grid(const TimeSignature *time_signature, Duration **out_beat, unsigned *out_beats);

const Rule *standard_meter_metric_rule(StandardMeterMetricRule which);
const Constraint *standard_meter_metric_constraint(StandardMeterMetricRule which);

/* BEAT_POSITION. */
bool standard_meter_beat_position(const TimeSignature *time_signature, const MetricalPosition *position,
                                  StandardMeterBeatPosition *out);

/* METRIC_LEVEL. */
bool standard_meter_metric_level(const TimeSignature *time_signature, const MetricalPosition *position,
                                 StandardMeterLevel *out);

/* BEAT_STRENGTH: the metric level mapped onto the shared BeatStrength. */
bool standard_meter_beat_strength(const TimeSignature *time_signature, const MetricalPosition *position,
                                  BeatStrength *out);

/* BEAT_STRENGTH: the mapping from a level alone: downbeat STRONG, secondary beat MEDIUM, the rest
   WEAK. */
BeatStrength standard_meter_level_strength(StandardMeterLevel level);

/*
 * The accented beats of one bar: the 0-based beat indexes whose standard_meter_beat_strength on the
 * beat is STRONG or MEDIUM, in order, with their strengths (strengths may be NULL). 4/4 and 12/8 give
 * beats 0 (strong) and 2 (medium); 2/4, 3/4 and 6/8 give beat 0 alone, as the downbeat is the only
 * accent outside a quadruple meter. *count is the number written. False, leaving the outputs
 * untouched, for a NULL signature, beats or count, a capacity too small for the accents, or on
 * allocation failure.
 */
bool standard_meter_accent_beats(const TimeSignature *time_signature, unsigned *beats, BeatStrength *strengths,
                                 size_t capacity, size_t *count);

#endif
