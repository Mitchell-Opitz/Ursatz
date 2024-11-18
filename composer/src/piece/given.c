#include <ursatz_composer/given.h>

#include <stdlib.h>
#include <string.h>

#include <harmonic_function.h>
#include <harmony_functions.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

const char *given_result_name(GivenResult result) {
    switch (result) {
    case GIVEN_OK:
        return "ok";
    case GIVEN_ERR_NULL:
        return "null argument";
    case GIVEN_ERR_NOT_SECTION:
        return "not a theme section";
    case GIVEN_ERR_BAD_NOTES:
        return "notes that cannot be placed";
    case GIVEN_ERR_BAD_NUMERAL:
        return "numeral that cannot be read";
    case GIVEN_ERR_BAD_BAR:
        return "bar outside the section";
    case GIVEN_ERR_PAST_END:
        return "past the section's end";
    case GIVEN_ERR_KEY:
        return "not inside one key span";
    case GIVEN_ERR_METER:
        return "no supported meter";
    case GIVEN_ERR_OVERLAP:
        return "overlaps given material";
    case GIVEN_ERR_PLAN:
        return "refused by the plan";
    }
    return "unknown";
}

/* ------------------------------------------------------------- reading the plan */

/* The section, its meter, and where the material starts, read once for the three calls. */
typedef struct {
    PlanSpan section;
    PlanMeter meter;
    PlanTime bar_length;
    PlanTime start;     /* where the material starts */
} Placement;

static bool is_material(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        if (plan_node_constraint_at(plan, section, i)->kind == PLAN_CONSTRAINT_MATERIAL) {
            return true;
        }
    }
    return false;
}

static PlanTime times(PlanTime unit, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make(unit.num * (long)count, unit.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

static GivenResult read_placement(const PiecePlan *plan, NodeId section, size_t bar, Placement *out) {
    LaneSpanId meters[2];
    if (!plan || !out) {
        return GIVEN_ERR_NULL;
    }
    if (!plan_has_node(plan, section) || plan_node_kind(plan, section) != PLAN_NODE_SECTION ||
        is_material(plan, section)) {
        return GIVEN_ERR_NOT_SECTION;
    }
    out->section = plan_node_span(plan, section);
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, out->section, meters, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, meters[0]), out->section)) {
        return GIVEN_ERR_METER;
    }
    out->meter = plan_lane_span_payload(plan, meters[0])->meter;
    size_t cells;
    if (plan_time_make(out->meter.beats_per_bar, (unsigned long)out->meter.beat_unit, &out->bar_length) != PLAN_OK ||
        !harmony_rhythm_cells_per_bar(&out->meter, &cells)) {
        return GIVEN_ERR_METER;
    }
    PlanTime end;
    if (plan_time_add(out->section.start, times(out->bar_length, bar), &out->start) != PLAN_OK ||
        plan_span_end(out->section, &end) != PLAN_OK) {
        return GIVEN_ERR_PLAN;
    }
    if (plan_time_compare(out->start, end) >= 0) {
        return GIVEN_ERR_BAD_BAR;
    }
    return GIVEN_OK;
}

/* Whether span lies in the section, in one key span, and clear of every span of the lane and voice already there. */
static GivenResult check_room(const PiecePlan *plan, const Placement *placement, PlanLaneKind lane, int voice,
                              PlanSpan span, PlanKey *key) {
    LaneSpanId keys[2];
    if (!plan_span_contains(placement->section, span)) {
        return GIVEN_ERR_PAST_END;
    }
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, keys, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, keys[0]), span)) {
        return GIVEN_ERR_KEY;
    }
    if (key) {
        *key = plan_lane_span_payload(plan, keys[0])->key;
    }
    if (plan_lane_find(plan, lane, voice, span, NULL, 0) != 0) {
        return GIVEN_ERR_OVERLAP;
    }
    return GIVEN_OK;
}

/* ------------------------------------------------------------- notes */

/*
 * The notes as a span from the start: the whole bars they fill, checked: in order, not overlapping, pitches in range.
 * A bass line has no rest: its notes tile the span.
 */
static GivenResult note_span(const Placement *placement, const PlanNote *notes, size_t count, bool tiled,
                             PlanSpan *out) {
    PlanTime end = {0, 1};
    if (!notes || count == 0) {
        return GIVEN_ERR_BAD_NOTES;
    }
    for (size_t i = 0; i < count; i++) {
        PlanTime note_end;
        if (notes[i].midi_pitch < 0 || notes[i].midi_pitch > 127 || notes[i].offset.num < 0 ||
            notes[i].duration.num <= 0 || notes[i].offset.den == 0 || notes[i].duration.den == 0 ||
            plan_time_compare(notes[i].offset, end) < 0 || plan_time_add(notes[i].offset, notes[i].duration, &note_end) != PLAN_OK ||
            (tiled && plan_time_compare(notes[i].offset, end) != 0)) {
            return GIVEN_ERR_BAD_NOTES;
        }
        end = note_end;
    }
    /* As many whole bars as the notes reach into. */
    size_t bars = 0;
    PlanTime length = {0, 1};
    while (plan_time_compare(length, end) < 0) {
        if (bars > 4096 || plan_time_add(length, placement->bar_length, &length) != PLAN_OK) {
            return GIVEN_ERR_BAD_NOTES;
        }
        bars++;
    }
    if (tiled && plan_time_compare(length, end) != 0) {
        return GIVEN_ERR_BAD_NOTES;
    }
    out->start = placement->start;
    out->length = length;
    return GIVEN_OK;
}

static GivenResult give_notes(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count,
                              PlanLaneKind lane) {
    Placement placement;
    PlanSpan span;
    GivenResult result = read_placement(plan, section, bar, &placement);
    if (result == GIVEN_OK) {
        result = note_span(&placement, notes, count, lane == PLAN_LANE_BASS, &span);
    }
    if (result == GIVEN_OK) {
        result = check_room(plan, &placement, lane, 0, span, NULL);
    }
    if (result != GIVEN_OK) {
        return result;
    }
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = notes;
    payload.notes.note_count = count;
    if (plan_add_lane_span(plan, section, lane, 0, span, &payload, &id) != PLAN_OK ||
        plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) != PLAN_OK) {
        return GIVEN_ERR_PLAN;
    }
    return GIVEN_OK;
}

GivenResult plan_give_melody(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count) {
    return give_notes(plan, section, bar, notes, count, PLAN_LANE_MELODY);
}

GivenResult plan_give_bass(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count) {
    return give_notes(plan, section, bar, notes, count, PLAN_LANE_BASS);
}

/* ------------------------------------------------------------- a progression */

static PlanHarmonicFunction plan_function_of(HarmonicFunction function) {
    switch (function) {
    case HARMONIC_FUNCTION_TONIC:
        return PLAN_FUNCTION_TONIC;
    case HARMONIC_FUNCTION_PREDOMINANT:
        return PLAN_FUNCTION_PREDOMINANT;
    case HARMONIC_FUNCTION_DOMINANT:
        return PLAN_FUNCTION_DOMINANT;
    default:
        return PLAN_FUNCTION_UNKNOWN;
    }
}

/* The payload of a numeral in a mode: a diatonic chord with its function, else an applied chord. False when unreadable. */
static bool numeral_payload(PlanKeyMode mode, const char *numeral, PlanHarmony *out) {
    HarmonyChord chord;
    HarmonyAppliedChord applied;
    int inversion;
    memset(out, 0, sizeof(*out));
    if (!numeral || strlen(numeral) >= PLAN_ROMAN_NUMERAL_MAX) {
        return false;
    }
    strcpy(out->roman_numeral, numeral);
    if (harmony_roman_parse(mode, numeral, &chord)) {
        HarmonicFunction function;
        out->function = harmony_function_of(harmony_mode_of(mode), &chord, &function) ? plan_function_of(function)
                                                                                    : PLAN_FUNCTION_UNKNOWN;
        return true;
    }
    if (harmony_roman_parse_applied_inverted(mode, numeral, &applied, &inversion)) {
        out->function = PLAN_FUNCTION_DOMINANT;
        return true;
    }
    return false;
}

GivenResult plan_give_progression(PiecePlan *plan, NodeId section, size_t bar, const char *const *numerals, size_t count,
                                  size_t cells_per_chord) {
    Placement placement;
    size_t cells;
    GivenResult result = read_placement(plan, section, bar, &placement);
    if (result != GIVEN_OK) {
        return result;
    }
    if (!numerals || count == 0 || cells_per_chord == 0 || count > 4096 || cells_per_chord > 4096) {
        return GIVEN_ERR_BAD_NUMERAL;
    }
    PlanTime cell_length;
    if (!harmony_rhythm_cells_per_bar(&placement.meter, &cells) ||
        plan_time_make(placement.bar_length.num, placement.bar_length.den * cells, &cell_length) != PLAN_OK) {
        return GIVEN_ERR_METER;
    }
    PlanSpan whole = {placement.start, times(cell_length, count * cells_per_chord)};
    PlanKey key;
    result = check_room(plan, &placement, PLAN_LANE_HARMONY, 0, whole, &key);
    if (result != GIVEN_OK) {
        return result;
    }
    PlanHarmony *payloads = calloc(count, sizeof(PlanHarmony));
    if (!payloads) {
        return GIVEN_ERR_PLAN;
    }
    for (size_t i = 0; i < count; i++) {
        if (!numeral_payload(key.mode, numerals[i], &payloads[i])) {
            free(payloads);
            return GIVEN_ERR_BAD_NUMERAL;
        }
    }
    result = GIVEN_OK;
    for (size_t i = 0; result == GIVEN_OK && i < count; i++) {
        PlanSpan span = {{0, 1}, times(cell_length, cells_per_chord)};
        PlanLanePayload payload;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        payload.harmony = payloads[i];
        if (plan_time_add(placement.start, times(cell_length, i * cells_per_chord), &span.start) != PLAN_OK ||
            plan_add_lane_span(plan, section, PLAN_LANE_HARMONY, 0, span, &payload, &id) != PLAN_OK ||
            plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) != PLAN_OK) {
            result = GIVEN_ERR_PLAN;
        }
    }
    free(payloads);
    return result;
}
