#include "analysis/beat_grid_length.h"

#include <stdlib.h>

#include "time/meter_arithmetic.h"
#include "time/time_rational.h"

bool beat_grid_beat_length(void *context, Rational at, Rational *out) {
    const BeatGridContext *grid = (const BeatGridContext *)context;
    int measure = 0;
    Rational measure_end;
    if (meter_map_measure_of(grid->map, at, &measure) != METER_MAP_OK ||
        meter_map_measure_bounds(grid->map, measure, NULL, &measure_end) != METER_MAP_OK) {
        return false;
    }

    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!grid->provider(grid->map, measure, grid->provider_context, &starts, &count) || count == 0) {
        return false;
    }
    bool found = false;
    for (size_t k = 0; k < count && !found; k++) {
        Rational beat_start;
        Rational beat_end = measure_end;
        if (rational_of_musical_time(starts[k], &beat_start) != RATIONAL_OK ||
            (k + 1 < count && rational_of_musical_time(starts[k + 1], &beat_end) != RATIONAL_OK)) {
            break;
        }
        if (rational_cmp(beat_start, at) <= 0 && rational_cmp(at, beat_end) < 0) {
            found = rational_sub(beat_end, beat_start, out) == RATIONAL_OK;
            break;
        }
    }
    for (size_t k = 0; k < count; k++) {
        musical_time_destroy(starts[k]);
    }
    free(starts);
    return found;
}
