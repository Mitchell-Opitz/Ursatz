#include "harmony/harmony_internal.h"

/*
 * The harmonic rhythm of the meters this operator knows. A chord changes only
 * on a strong beat, and a bar has these (a cell is the time from one to the
 * next):
 *   2/4  beat 1                  one cell
 *   3/4  beat 1                  one cell
 *   4/4  beats 1 and 3           two cells; beat 1 is strong, beat 3 medium
 *   6/8  beats 1 and 4 (the two  two cells; beat 1 is strong, beat 4 medium
 *        dotted quarters)
 */

bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    if (!meter || !cells) {
        return false;
    }
    if (meter->beat_unit == 4 && (meter->beats_per_bar == 2 || meter->beats_per_bar == 3)) {
        *cells = 1;
        return true;
    }
    if (meter->beat_unit == 4 && meter->beats_per_bar == 4) {
        *cells = 2;
        return true;
    }
    if (meter->beat_unit == 8 && meter->beats_per_bar == 6) {
        *cells = 2;
        return true;
    }
    return false;
}

BeatStrength harmony_rhythm_cell_strength(size_t cell_in_bar) {
    return cell_in_bar == 0 ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_MEDIUM;
}
