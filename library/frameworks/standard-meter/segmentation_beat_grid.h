#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_SEGMENTATION_BEAT_GRID_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_SEGMENTATION_BEAT_GRID_H

#include <stdbool.h>
#include <stddef.h>

#include "time/meter_map.h"
#include "time/musical_time.h"

/*
 * The standard-meter beat-grid provider for adaptive sonority segmentation
 * (semantics/sonority_segmentation.h): the BeatGridProvider the kernel builder is handed. The kernel
 * cannot include a framework, so what a beat is arrives by injection, and this is the standard
 * reading: the beats of a measure come from the signature's meter class (meter_class_rules.h, and
 * standard_meter_beat_grid in metric_position_rules.h for irregular meters), so 6/8 gives two
 * dotted-quarter beats, 3/4 three quarter-note beats, 12/8 four dotted-quarter beats, and 5/8 five
 * eighth-note pulses.
 *
 * Which signature a measure is read in comes from the kernel (metrical_position_reading_signature in
 * time/metrical_position.h). Short measures and pickups are handled by position, not by signature:
 *
 *   - A short measure (cut by a mid-measure meter change) is laid out from its own start and its last
 *     beat is cut at the barline.
 *   - A pickup measure is measure 0: its offset is where a full measure would have started
 *     (MetricalPosition), so beats fall where the full measure's beats would. A one-quarter pickup in
 *     4/4 is the last beat of an imaginary measure, one beat; a pickup that begins partway through a
 *     beat starts with a partial beat, which is the pickup's first beat.
 *
 * The first beat start of every measure is the measure's own start.
 */

/* A BeatGridProvider (semantics/sonority_segmentation.h). context is unused and may be NULL. */
bool standard_meter_segmentation_beat_grid(const MeterMap *map, int measure, void *context,
                                            MusicalTime ***out_beat_starts, size_t *out_count);

#endif
