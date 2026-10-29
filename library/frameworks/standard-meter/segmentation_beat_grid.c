#include "segmentation_beat_grid.h"

#include <limits.h>
#include <stdlib.h>

#include "metric_position_rules.h"
#include "core/rational.h"
#include "time/time_rational.h"
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
    Rational start;
    Rational end;
    if (rational_of_musical_time(time_span_start(span), &start) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(span), &end) != RATIONAL_OK) {
        time_span_destroy(span);
        return false;
    }

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
    Rational beat;
    Rational offset;
    bool converted = rational_of_duration(beat_duration, &beat) == RATIONAL_OK &&
                     rational_of_duration(metrical_position_offset(first_position), &offset) == RATIONAL_OK;
    duration_destroy(beat_duration);
    metrical_position_destroy(first_position);
    if (!converted) {
        return false;
    }

    size_t capacity = 8;
    size_t count = 0;
    MusicalTime **starts = (MusicalTime **)malloc(capacity * sizeof(MusicalTime *));
    if (starts == NULL) {
        return false;
    }

    /* The measure's own start is its first beat start. The beat lines after it are the multiples of
       the beat length past the offset, as times, until the measure ends. */
    if (musical_time_from_rational(start, &starts[count]) != TIME_RATIONAL_OK) {
        free(starts);
        return false;
    }
    count++;

    /* First multiple of the beat strictly past the offset: floor(offset / beat) + 1. */
    Rational quotient = {0, 1};
    long long k = 1;
    bool failed = rational_div(offset, beat, &quotient) != RATIONAL_OK || rational_floor(quotient, &k) != RATIONAL_OK ||
                  k == LLONG_MAX;
    k++;

    while (!failed) {
        Rational line = {0, 1};
        Rational past = {0, 1};
        Rational at = {0, 1};
        if (rational_mul_int(beat, k, &line) != RATIONAL_OK || rational_sub(line, offset, &past) != RATIONAL_OK ||
            rational_add(start, past, &at) != RATIONAL_OK) {
            failed = true;
            break;
        }
        if (rational_cmp(at, end) >= 0) {
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
        if (musical_time_from_rational(at, &starts[count]) != TIME_RATIONAL_OK) {
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
