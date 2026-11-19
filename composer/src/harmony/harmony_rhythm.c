#include "harmony/harmony_internal.h"
#include "structure/plan_util.h"

#include <theory/meter/metric_position_rules.h>
#include <model/time/note_value.h>
#include <model/time/time_signature.h>

/*
 * The harmonic rhythm of the meters this operator knows. A chord changes only
 * on a beat the Composer chooses to change on (a composing decision): one cell
 * per accent beat of a simple meter (standard_meter_accent_beats), and one per
 * beat group of a compound meter (time_signature_group_count: the dotted
 * notes). Cell 0 is strong and every other cell medium. A cell is the time
 * from one to the next:
 *   2/4  beat 1                  one cell
 *   3/4  beat 1                  one cell
 *   4/4  beats 1 and 3           two cells; beat 1 is strong, beat 3 medium
 *   6/8  beats 1 and 4 (the two  two cells; beat 1 is strong, beat 4 medium
 *        dotted quarters)
 * R-263 (one cell per meter beat, strength by accent, the second 6/8 cell weak)
 * is amended to this, which is what P25 B5 built (R-590): 4/4 has two cells, not
 * four, and the second cell of 6/8 is medium, not weak.
 */

/* The chords a cadence needs before its last: a tonic, the dominant unless the phrase ends on it, and the plagal tonic and IV. */
static size_t cells_before_last(CadenceType cadence) {
    return 1 + (cadence == CADENCE_TYPE_HALF ? 0 : 1) + (cadence == CADENCE_TYPE_PLAGAL ? 2 : 0);
}

size_t harmony_phrase_min_cells(CadenceType cadence, const PlanMeter *meter) {
    size_t cells_per_bar;
    return harmony_rhythm_cells_per_bar(meter, &cells_per_bar) ? cells_before_last(cadence) + 1 : 0;
}

size_t harmony_phrase_room_cells(CadenceType cadence, size_t cells_per_bar) {
    return cells_before_last(cadence) + cells_per_bar;
}

size_t harmony_phrase_min_bars(CadenceType cadence, const PlanMeter *meter) {
    size_t cells_per_bar;
    if (!harmony_rhythm_cells_per_bar(meter, &cells_per_bar)) {
        return 0;
    }
    /* A phrase of whole bars ends in a whole bar, however few cells a span between beats can end in. */
    size_t cells = harmony_phrase_room_cells(cadence, cells_per_bar);
    return (cells + cells_per_bar - 1) / cells_per_bar;
}

BeatStrength harmony_rhythm_cell_strength(size_t cell_in_bar) {
    return cell_in_bar == 0 ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_MEDIUM;
}
