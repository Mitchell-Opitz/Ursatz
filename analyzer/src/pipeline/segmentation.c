#include "pipeline/segmentation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "pipeline/pitch_class_util.h"
#include "pitch/pitch_arithmetic.h"
#include "pitch/pitch_spelling.h"
#include "semantics/sonority_segmentation.h"
#include "standard-meter/metric_position_rules.h"
#include "standard-meter/segmentation_beat_grid.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "time/time_span.h"

typedef struct {
    const char *id;
    size_t index;
} NoteKey;

static int compare_note_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

/* The strength of an attack at onset, read in the signature of its measure; weak if it cannot be read. */
BeatStrength beat_strength_at(const MeterMap *map, const MusicalTime *onset) {
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    MetricalPosition *position = NULL;
    if (metrical_position_from_time(map, onset, &position) != METRICAL_POSITION_OK) {
        return strength;
    }
    const TimeSignature *reading = NULL;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) ==
        METRICAL_POSITION_OK) {
        BeatStrength found;
        if (standard_meter_beat_strength(reading, position, &found)) {
            strength = found;
        }
    }
    metrical_position_destroy(position);
    return strength;
}

int segment_bass_pitch_class(const Segment *segment) {
    int lowest = segment->members[0].pitch;
    for (size_t i = 1; i < segment->member_count; i++) {
        if (segment->members[i].pitch < lowest) {
            lowest = segment->members[i].pitch;
        }
    }
    return pitch_class_of(lowest);
}
