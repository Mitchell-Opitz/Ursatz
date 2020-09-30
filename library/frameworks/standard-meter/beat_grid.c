#include "beat_grid.h"

#include "meter_class_rules.h"

bool standard_meter_beat_at(const TimeSignature *time_signature, const Duration *offset, unsigned *out_index,
                            bool *out_on_beat) {
    if (time_signature == NULL || offset == NULL) {
        return false;
    }
    Duration *beat = NULL;
    unsigned beats;
    if (!standard_meter_beat_duration(time_signature, &beat) ||
        !standard_meter_beats_per_measure(time_signature, &beats)) {
        duration_destroy(beat);
        return false;
    }

    /* offset / beat = (offset_n * beat_d) / (offset_d * beat_n) */
    unsigned long numerator = (unsigned long)duration_numerator(offset) * duration_denominator(beat);
    unsigned long denominator = duration_denominator(offset) * (unsigned long)duration_numerator(beat);
    duration_destroy(beat);

    unsigned long index = numerator / denominator;
    if (index >= beats) {
        return false;
    }
    *out_index = (unsigned)index;
    *out_on_beat = numerator % denominator == 0;
    return true;
}

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
