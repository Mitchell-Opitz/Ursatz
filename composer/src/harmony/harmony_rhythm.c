#include "harmony/harmony_internal.h"

#include <standard-meter/metric_position_rules.h>
#include <time/note_value.h>
#include <time/time_signature.h>

/*
 * The harmonic rhythm of the meters this operator knows. A chord changes only
 * on an accented beat, and the cells of a bar are the accented beats the
 * standard meter names (standard_meter_accent_beats; a cell is the time from
 * one to the next):
 *   2/4  beat 1                  one cell
 *   3/4  beat 1                  one cell
 *   4/4  beats 1 and 3           two cells; beat 1 is strong, beat 3 medium
 *   6/8  beat 1                  one cell
 */

#define RHYTHM_MAX_CELLS 4u

static bool meter_signature(const PlanMeter *meter, TimeSignature **out) {
    NoteValue *unit = NULL;
    if (!meter || meter->beats_per_bar < 1 || (meter->beat_unit != 4 && meter->beat_unit != 8)) {
        return false;
    }
    if (note_value_create(meter->beat_unit == 4 ? 2 : 3, &unit) != NOTE_VALUE_OK) {
        return false;
    }
    TimeSignatureResult result = time_signature_create((unsigned)meter->beats_per_bar, unit, out);
    note_value_destroy(unit);
    return result == TIME_SIGNATURE_OK;
}

static bool meter_accents(const PlanMeter *meter, BeatStrength *strengths, size_t *count) {
    TimeSignature *signature = NULL;
    unsigned beats[RHYTHM_MAX_CELLS];
    if (!meter_signature(meter, &signature)) {
        return false;
    }
    bool ok = standard_meter_accent_beats(signature, beats, strengths, RHYTHM_MAX_CELLS, count);
    time_signature_destroy(signature);
    return ok;
}

bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    BeatStrength strengths[RHYTHM_MAX_CELLS];
    size_t count = 0;
    if (!cells || !meter_accents(meter, strengths, &count) || count == 0) {
        return false;
    }
    /* The meters this operator writes: simple duple, triple and quadruple over quarters, and 6/8. */
    bool known = (meter->beat_unit == 4 && meter->beats_per_bar >= 2 && meter->beats_per_bar <= 4) ||
                 (meter->beat_unit == 8 && meter->beats_per_bar == 6);
    if (!known) {
        return false;
    }
    *cells = count;
    return true;
}

bool harmony_meter_supported(const PlanMeter *meter) {
    size_t cells;
    return harmony_rhythm_cells_per_bar(meter, &cells);
}

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
