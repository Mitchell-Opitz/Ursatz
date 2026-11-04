#include "meter_analysis_position.h"

#include <stdlib.h>

#include "beat_grid.h"
#include "core/rational.h"
#include "meter_class_rules.h"
#include "time/meter_arithmetic.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/score_time.h"
#include "time/time_rational.h"
#include "time/time_signature.h"

static bool time_of(Rational q, MusicalTime **out) {
    return q.num >= 0 && musical_time_create((long)q.num, (unsigned long)q.den, out) == MUSICAL_TIME_OK;
}

static bool rational_of_length(const Duration *duration, Rational *out) {
    return rational_make(duration_numerator(duration), (long long)duration_denominator(duration), out) == RATIONAL_OK;
}

bool standard_meter_measure_grid(const MeterMap *map, int index, StandardMeterMeasureGrid *grid) {
    if (grid == NULL || !score_time_measure_bounds(map, index, &grid->start, &grid->end)) {
        return false;
    }
    const TimeSignature *signature = NULL;
    if (metrical_position_reading_signature(map, index, &signature) != METRICAL_POSITION_OK) {
        return false;
    }
    Duration *beat = NULL;
    unsigned beats = 0;
    if (!standard_meter_beat_grid(signature, &beat, &beats)) {
        duration_destroy(beat);
        return false;
    }
    bool beat_ok = rational_of_length(beat, &grid->beat);
    duration_destroy(beat);
    if (!beat_ok) {
        return false;
    }
    grid->beats = beats;
    grid->division = standard_meter_type(signature) == METER_TYPE_COMPOUND ? 3 : 2;

    MusicalTime *at = NULL;
    MetricalPosition *position = NULL;
    if (!time_of(grid->start, &at)) {
        return false;
    }
    bool ok = metrical_position_from_time(map, at, &position) == METRICAL_POSITION_OK;
    musical_time_destroy(at);
    if (!ok) {
        return false;
    }
    Rational offset;
    ok = rational_of_length(metrical_position_offset(position), &offset) &&
         rational_sub(grid->start, offset, &grid->base) == RATIONAL_OK;
    metrical_position_destroy(position);
    return ok;
}

static bool position_at(const MeterMap *map, Rational time, MetricalPosition **position, const TimeSignature **signature) {
    MusicalTime *at = NULL;
    if (map == NULL || !time_of(time, &at)) {
        return false;
    }
    bool ok = metrical_position_from_time(map, at, position) == METRICAL_POSITION_OK;
    musical_time_destroy(at);
    if (!ok) {
        return false;
    }
    if (metrical_position_reading_signature(map, metrical_position_measure(*position), signature) != METRICAL_POSITION_OK) {
        metrical_position_destroy(*position);
        *position = NULL;
        return false;
    }
    return true;
}

bool standard_meter_level_at(const MeterMap *map, Rational time, StandardMeterLevel *level) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (level == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    bool ok = standard_meter_metric_level(signature, position, level);
    metrical_position_destroy(position);
    return ok;
}

bool standard_meter_beat_of(const MeterMap *map, Rational time, int *measure, double *beat) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (measure == NULL || beat == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    Duration *unit = NULL;
    unsigned beats = 0;
    bool ok = standard_meter_beat_grid(signature, &unit, &beats);
    Rational length;
    Rational offset;
    ok = ok && rational_of_length(unit, &length) && rational_of_length(metrical_position_offset(position), &offset);
    if (ok) {
        *measure = metrical_position_measure(position);
        *beat = 1.0 + rational_to_double(offset) / rational_to_double(length);
    }
    duration_destroy(unit);
    metrical_position_destroy(position);
    return ok;
}
