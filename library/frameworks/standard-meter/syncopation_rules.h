#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_SYNCOPATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_SYNCOPATION_RULES_H

#include <stdbool.h>

#include "metric_position_rules.h"
#include "model/time/duration.h"
#include "model/time/time_signature.h"

/*
 * Syncopation rules for standard-meter-v1. Syncopation happens when
 * there are off-beat rhythmic accents; four things make one:
 *
 *   OFF_BEAT  a position is off the beat when it is not the start of a
 *             beat of the signature's meter class (beat_grid.h)
 *   SUSTAIN   ties and dots: a note that starts off the beat and is
 *             sustained across the next beat line (an eighth note tied
 *             to a quarter note, a dotted quarter starting off the beat)
 *             puts its accent off the beat
 *   REST      a rest on the beat, with the music sounding only off it
 *             (a rest at the start of each beat, notes on the divisions)
 *   ACCENT    an accent -- from dynamics, an accent mark -- on a note
 *             that is itself off the beat, even when every division is
 *             filled in
 *   THROUGH_STRONGER  a note attacked at a weaker metric level and sustained
 *             through a stronger one that nothing attacks: the general form
 *             of SUSTAIN, read on metric levels (metric_position_rules.h)
 *             rather than on beat lines, so a tie across a barline, a note
 *             on beat 2 held through beat 3 of a quadruple meter, and an
 *             off-beat note held over the beat are all instances. Its
 *             strength is the difference of the two levels
 *
 * Inputs are plain positions (offsets from the start of the measure),
 * lengths, and an `accented` flag, not NoteEvent/RestEvent/Articulation
 * entities: placing those in a measure needs MeterMap. What counts as an
 * accent is the caller's. A tie is seen here as the long note it makes,
 * so the caller passes the combined length.
 *
 * Every function is false for an unclassified signature (no beat grid)
 * or a position outside the measure.
 */

/*
 * THROUGH_STRONGER: true when a note attacked at metric level `attack` is sustained through a position
 * of level `sustained` that is stronger than the attack (a level nearer the downbeat: downbeat,
 * secondary beat, beat, off-beat, finer). Whether anything attacks that position is the caller's to
 * check: this reads the two levels only. *out_strength, when not NULL, is set to the difference of the
 * levels (1 up to 4) on true and left untouched on false. False too for a level outside the enum.
 */
bool standard_meter_syncopated_through_stronger(StandardMeterLevel attack, StandardMeterLevel sustained,
                                                unsigned *out_strength);

#endif
