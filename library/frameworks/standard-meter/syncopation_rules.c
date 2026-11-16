#include "syncopation_rules.h"

#include "beat_grid.h"

bool standard_meter_syncopated_through_stronger(StandardMeterLevel attack, StandardMeterLevel sustained,
                                                unsigned *out_strength) {
    if (attack < STANDARD_METER_LEVEL_DOWNBEAT || attack > STANDARD_METER_LEVEL_FINER ||
        sustained < STANDARD_METER_LEVEL_DOWNBEAT || sustained > STANDARD_METER_LEVEL_FINER || sustained >= attack) {
        return false;
    }
    if (out_strength != NULL) {
        *out_strength = (unsigned)(attack - sustained);
    }
    return true;
}
