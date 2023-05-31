#include "segmentation_beat_grid.h"

#include <stdlib.h>

#include "metric_position_rules.h"
#include "time/exact_rational.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

static void destroy_starts(MusicalTime **starts, size_t count) {
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(starts[i]);
    }
    free(starts);
}

bool standard_meter_segmentation_beat_grid(const MeterMap *map, int measure, void *context,
                                            MusicalTime ***out_beat_starts, size_t *out_count) {
    (void)context;
    if (map == NULL || out_beat_starts == NULL || out_count == NULL) {
        return false;
    }

    TimeSpan *span = NULL;
    if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
        return false;
    }
    ExactRational start = er_of_time(time_span_start(span));
    ExactRational end = er_of_time(time_span_end(span));

    const TimeSignature *reading = NULL;
    Duration *beat_duration = NULL;
    unsigned beats = 0;
    MetricalPosition *first_position = NULL;
    bool ok = metrical_position_reading_signature(map, measure, &reading) == METRICAL_POSITION_OK &&
              standard_meter_beat_grid(reading, &beat_duration, &beats) &&
              metrical_position_from_time(map, time_span_start(span), &first_position) == METRICAL_POSITION_OK;
    time_span_destroy(span);
    if (!ok) {
        duration_destroy(beat_duration);
        metrical_position_destroy(first_position);
        return false;
    }

    /* How far into a full measure the measure's start is: 0, except for a pickup. */
    ExactRational beat = er_of_duration(beat_duration);
    ExactRational offset = er_of_duration(metrical_position_offset(first_position));
    duration_destroy(beat_duration);
    metrical_position_destroy(first_position);

    size_t capacity = 8;
    size_t count = 0;
    MusicalTime **starts = (MusicalTime **)malloc(capacity * sizeof(MusicalTime *));
    if (starts == NULL) {
        return false;
    }

    /* The measure's own start is its first beat start. The beat lines after it are the multiples of
       the beat length past the offset, as times, until the measure ends. */
    starts[count] = er_to_time(start);
    if (starts[count] == NULL) {
        free(starts);
        return false;
    }
    count++;

    /* First multiple of the beat strictly past the offset: floor(offset / beat) + 1. */
    ExactRational quotient = er_make(0, 1);
    bool failed = er_mul(offset, er_make(beat.d, beat.n), &quotient) != 0;
    unsigned long long k = quotient.n / quotient.d + 1;

    while (!failed) {
        ExactRational line = {0, 1};
        ExactRational past = {0, 1};
        ExactRational at = {0, 1};
        if (er_mul(beat, er_make(k, 1), &line) != 0 || er_sub(line, offset, &past) != 0 ||
            er_add(start, past, &at) != 0) {
            failed = true;
            break;
        }
        if (er_cmp(at, end) >= 0) {
            break;
        }
        if (count == capacity) {
            MusicalTime **grown = (MusicalTime **)realloc(starts, capacity * 2 * sizeof(MusicalTime *));
            if (grown == NULL) {
                failed = true;
                break;
            }
            starts = grown;
            capacity *= 2;
        }
        starts[count] = er_to_time(at);
        if (starts[count] == NULL) {
            failed = true;
            break;
        }
        count++;
        k++;
    }

    if (failed) {
        destroy_starts(starts, count);
        return false;
    }
    *out_beat_starts = starts;
    *out_count = count;
    return true;
}
