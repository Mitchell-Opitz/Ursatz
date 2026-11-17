#include "theory/meter/beat_grid.h"

#include <stddef.h>

#include "model/core/rational.h"
#include "theory/meter/meter_class_rules.h"

bool standard_meter_beat_start(const TimeSignature *time_signature, unsigned index, Duration *out) {
    Duration beat;
    unsigned beats;
    if (time_signature == NULL || out == NULL || !standard_meter_beat_duration(time_signature, &beat)) {
        return false;
    }
    Rational start;
    return standard_meter_beats_per_measure(time_signature, &beats) && index <= beats &&
           rational_mul_int(beat.value, (long long)index, &start) == RATIONAL_OK &&
           duration_of(start, out) == DURATION_OK;
}
