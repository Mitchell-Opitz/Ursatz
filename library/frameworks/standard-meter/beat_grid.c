#include "beat_grid.h"

#include <stddef.h>

#include "meter_class_rules.h"

bool standard_meter_beat_start(const TimeSignature *time_signature, unsigned index, Duration **out) {
    Duration *beat = NULL;
    unsigned beats;
    if (time_signature == NULL || !standard_meter_beat_duration(time_signature, &beat)) {
        return false;
    }
    bool ok = standard_meter_beats_per_measure(time_signature, &beats) && index <= beats &&
              duration_create(duration_numerator(beat) * (long)index, duration_denominator(beat), out) ==
                  DURATION_OK;
    duration_destroy(beat);
    return ok;
}
