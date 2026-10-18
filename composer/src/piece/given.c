#include <ursatz_composer/given.h>

#include <stdlib.h>
#include <string.h>

#include <harmonic_function.h>
#include <harmony_functions.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "piece/form_return_internal.h"

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
    case GIVEN_ERR_NOT_REALIZED:
        return "section not realized";
    case GIVEN_ERR_LENGTH:
        return "section of another length";
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

GivenResult plan_give_chords(PiecePlan *plan, NodeId section, const GivenChord *chords, size_t count) {
    Placement placement;
    GivenResult result = read_placement(plan, section, 0, &placement);
    if (result != GIVEN_OK) {
        return result;
    }
    if (!chords || count == 0 || count > 4096) {
        return GIVEN_ERR_BAD_NUMERAL;
    }
    PlanHarmony *payloads = calloc(count, sizeof(PlanHarmony));
    PlanSpan *spans = calloc(count, sizeof(PlanSpan));
    if (!payloads || !spans) {
        free(payloads);
        free(spans);
        return GIVEN_ERR_PLAN;
    }
    for (size_t i = 0; result == GIVEN_OK && i < count; i++) {
        PlanKey key;
        if (chords[i].onset.num < 0 || chords[i].onset.den <= 0 || chords[i].length.num <= 0 ||
            chords[i].length.den <= 0 ||
            plan_time_add(placement.section.start, chords[i].onset, &spans[i].start) != PLAN_OK) {
            result = GIVEN_ERR_BAD_NUMERAL;
            break;
        }
        spans[i].length = chords[i].length;
        result = check_room(plan, &placement, PLAN_LANE_HARMONY, 0, spans[i], &key);
        if (result == GIVEN_OK && !numeral_payload(key.mode, chords[i].numeral, &payloads[i])) {
            result = GIVEN_ERR_BAD_NUMERAL;
        }
        for (size_t j = 0; result == GIVEN_OK && j < i; j++) {
            PlanTime end_i, end_j;
            if (plan_span_end(spans[i], &end_i) != PLAN_OK || plan_span_end(spans[j], &end_j) != PLAN_OK) {
                result = GIVEN_ERR_PLAN;
            } else if (plan_time_compare(spans[i].start, end_j) < 0 && plan_time_compare(spans[j].start, end_i) < 0) {
                result = GIVEN_ERR_OVERLAP;
            }
        }
    }
    for (size_t i = 0; result == GIVEN_OK && i < count; i++) {
        PlanLanePayload payload;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        payload.harmony = payloads[i];
        if (plan_add_lane_span(plan, section, PLAN_LANE_HARMONY, 0, spans[i], &payload, &id) != PLAN_OK ||
            plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) != PLAN_OK) {
            result = GIVEN_ERR_PLAN;
        }
    }
    free(payloads);
    free(spans);
    return result;
}

bool given_cell_length(const PlanMeter *meter, PlanTime *length) {
    size_t cells;
    PlanTime bar;
    return meter && length && harmony_rhythm_cells_per_bar(meter, &cells) &&
           plan_time_make(meter->beats_per_bar, (unsigned long)meter->beat_unit * cells, &bar) == PLAN_OK &&
           ((*length = bar), true);
}

/* ------------------------------------------------------------- a section */

static void drop_motive_refs(PlanConstraint *constraints, size_t *count) {
    size_t kept = 0;
    for (size_t i = 0; i < *count; i++) {
        if (constraints[i].kind != PLAN_CONSTRAINT_MOTIVE_REF) {
            constraints[kept++] = constraints[i];
        }
    }
    *count = kept;
}

/* The pickup of a section: the length of its PICKUP constraint, else nothing. */
static PlanTime section_pickup(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_PICKUP) {
            return constraint->params.pickup;
        }
    }
    return (PlanTime){0, 1};
}

/* Whether the copy's spans lie from the pickup before the section to its end, and hold pitches a note can have. */
static GivenResult copy_fits(const PiecePlan *plan, NodeId section, const ReturnCopy *copy) {
    PlanSpan span = plan_node_span(plan, section);
    PlanSpan room = span;
    PlanTime pickup = section_pickup(plan, section);
    PlanTime before;
    if (plan_time_make(-pickup.num, pickup.den, &before) != PLAN_OK ||
        plan_time_add(span.start, before, &room.start) != PLAN_OK ||
        plan_time_add(span.length, pickup, &room.length) != PLAN_OK) {
        return GIVEN_ERR_LENGTH;
    }
    for (size_t i = 0; i < copy->span_count; i++) {
        const ReturnSpan *given = &copy->spans[i];
        if (!plan_span_contains(room, given->span)) {
            return GIVEN_ERR_LENGTH;
        }
        for (size_t n = 0; n < given->note_count; n++) {
            if (given->notes[n].midi_pitch < 0 || given->notes[n].midi_pitch > 127) {
                return GIVEN_ERR_BAD_NOTES;
            }
        }
        if (plan_lane_find(plan, given->lane, given->voice, given->span, NULL, 0) != 0) {
            return GIVEN_ERR_OVERLAP;
        }
    }
    return GIVEN_OK;
}

GivenResult plan_give_section(PiecePlan *plan, NodeId section, const PiecePlan *source, NodeId source_section) {
    Placement placement;
    GivenResult result = read_placement(plan, section, 0, &placement);
    if (result != GIVEN_OK) {
        return result;
    }
    if (!source || !plan_has_node(source, source_section) || plan_node_kind(source, source_section) != PLAN_NODE_SECTION ||
        is_material(source, source_section)) {
        return source ? GIVEN_ERR_NOT_SECTION : GIVEN_ERR_NULL;
    }
    PlanSpan from = plan_node_span(source, source_section);
    PlanMeter meter;
    int shift;
    if (!return_source_ready(source, source_section)) {
        return GIVEN_ERR_NOT_REALIZED;
    }
    if (!return_meter_over(source, from, &meter) || meter.beats_per_bar != placement.meter.beats_per_bar ||
        meter.beat_unit != placement.meter.beat_unit) {
        return GIVEN_ERR_METER;
    }
    if (plan_time_compare(from.length, placement.section.length) != 0) {
        return GIVEN_ERR_LENGTH;
    }
    if (plan_node_child_count(plan, section) != 0) {
        return GIVEN_ERR_OVERLAP;
    }
    if (!return_key_shift(source, from, plan, placement.section, &shift)) {
        return GIVEN_ERR_KEY;
    }
    ReturnCopy copy;
    if (!return_copy_read(source, source_section, &copy)) {
        return GIVEN_ERR_PLAN;
    }
    /* The spans belong to the section and are Given; the nodes keep the states they had. */
    for (size_t i = 0; i < copy.node_count; i++) {
        drop_motive_refs(copy.nodes[i].constraints, &copy.nodes[i].constraint_count);
    }
    for (size_t i = 0; i < copy.span_count; i++) {
        copy.spans[i].owner = RETURN_SECTION;
        copy.spans[i].state = PLAN_STATE_GIVEN;
        drop_motive_refs(copy.spans[i].constraints, &copy.spans[i].constraint_count);
    }
    if (!return_copy_move(&copy, from.start, placement.section.start, shift)) {
        result = GIVEN_ERR_PLAN;
    } else {
        result = copy_fits(plan, section, &copy);
    }
    if (result == GIVEN_OK && (return_copy_write(plan, section, &copy) != PLAN_OK ||
                               plan_node_set_state(plan, section, PLAN_STATE_REALIZED) != PLAN_OK)) {
        result = GIVEN_ERR_PLAN;
    }
    return_copy_destroy(&copy);
    return result;
}

/* ------------------------------------------------------------- the gaps */

#define MAX_GIVEN_SPANS 64

static PlanSpan between(PlanTime from, PlanTime to) {
    PlanSpan span = {from, {0, 1}};
    if (plan_time_make(to.num * (long)from.den - from.num * (long)to.den, to.den * from.den, &span.length) != PLAN_OK) {
        span.length.num = 0;
        span.length.den = 1;
    }
    return span;
}

bool given_touches(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span) {
    LaneSpanId ids[MAX_GIVEN_SPANS];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, MAX_GIVEN_SPANS);
    for (size_t i = 0; i < found && i < MAX_GIVEN_SPANS; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN) {
            return true;
        }
    }
    return false;
}

size_t given_gaps(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, PlanSpan *out, size_t capacity) {
    LaneSpanId ids[MAX_GIVEN_SPANS];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, MAX_GIVEN_SPANS);
    PlanTime end;
    PlanTime at = span.start;
    size_t count = 0;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return 0;
    }
    if (found > MAX_GIVEN_SPANS) {
        found = MAX_GIVEN_SPANS;
    }
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_state(plan, ids[i]) != PLAN_STATE_GIVEN) {
            continue;
        }
        PlanSpan given = plan_lane_span_span(plan, ids[i]);
        PlanTime given_end;
        if (plan_span_end(given, &given_end) != PLAN_OK) {
            continue;
        }
        if (plan_time_compare(given.start, at) > 0) {
            if (out && count < capacity) {
                out[count] = between(at, given.start);
            }
            count++;
        }
        if (plan_time_compare(given_end, at) > 0) {
            at = given_end;
        }
    }
    if (plan_time_compare(at, end) < 0) {
        if (out && count < capacity) {
            out[count] = between(at, end);
        }
        count++;
    }
    return count;
}

/* ------------------------------------------------------------- the snapshot */

typedef struct {
    PlanLaneKind lane;
    int voice;
    NodeId owner;
    PlanSpan span;
    PlanHarmony harmony;
    PlanNote *notes;
    size_t note_count;
} SnapshotEntry;

struct GivenSnapshot {
    SnapshotEntry *entries;
    size_t count;
};

void given_snapshot_destroy(GivenSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    for (size_t i = 0; i < snapshot->count; i++) {
        free(snapshot->entries[i].notes);
    }
    free(snapshot->entries);
    free(snapshot);
}

/* Whether a lane span is material the user placed: Given, in a lane the user fills, owned by a section. */
static bool placed(const PiecePlan *plan, LaneSpanId id) {
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    return plan_has_lane_span(plan, id) && plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN &&
           (lane == PLAN_LANE_HARMONY || lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY ||
            lane == PLAN_LANE_INNER_VOICE) &&
           plan_node_kind(plan, plan_lane_span_owner(plan, id)) == PLAN_NODE_SECTION;
}

GivenSnapshot *given_snapshot(const PiecePlan *plan) {
    GivenSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    size_t found = 0;
    if (!snapshot || !plan) {
        free(snapshot);
        return NULL;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        found += placed(plan, id);
    }
    snapshot->entries = found ? calloc(found, sizeof(SnapshotEntry)) : NULL;
    if (found && !snapshot->entries) {
        free(snapshot);
        return NULL;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!placed(plan, id)) {
            continue;
        }
        SnapshotEntry *entry = &snapshot->entries[snapshot->count++];
        entry->lane = plan_lane_span_lane(plan, id);
        entry->voice = plan_lane_span_voice(plan, id);
        entry->owner = plan_lane_span_owner(plan, id);
        entry->span = plan_lane_span_span(plan, id);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, id);
        if (entry->lane == PLAN_LANE_HARMONY) {
            entry->harmony = payload->harmony;
        } else if (payload->notes.note_count > 0) {
            entry->notes = malloc(payload->notes.note_count * sizeof(PlanNote));
            if (!entry->notes) {
                given_snapshot_destroy(snapshot);
                return NULL;
            }
            memcpy(entry->notes, payload->notes.notes, payload->notes.note_count * sizeof(PlanNote));
            entry->note_count = payload->notes.note_count;
        }
    }
    return snapshot;
}

static bool same_time(PlanTime a, PlanTime b) {
    return plan_time_compare(a, b) == 0;
}

/* Whether the plan has a Given span like the entry: same lane, voice, owner and span, and the same payload. */
static bool entry_unchanged(const SnapshotEntry *entry, const PiecePlan *plan) {
    LaneSpanId ids[8];
    size_t found = plan_lane_find(plan, entry->lane, entry->voice, entry->span, ids, 8);
    for (size_t i = 0; i < found && i < 8; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) != PLAN_STATE_GIVEN || plan_lane_span_owner(plan, ids[i]) != entry->owner ||
            !same_time(span.start, entry->span.start) || !same_time(span.length, entry->span.length)) {
            continue;
        }
        if (entry->lane == PLAN_LANE_HARMONY) {
            return strcmp(payload->harmony.roman_numeral, entry->harmony.roman_numeral) == 0 &&
                   payload->harmony.function == entry->harmony.function;
        }
        if (payload->notes.note_count != entry->note_count) {
            return false;
        }
        for (size_t n = 0; n < entry->note_count; n++) {
            const PlanNote *was = &entry->notes[n];
            const PlanNote *now = &payload->notes.notes[n];
            if (was->midi_pitch != now->midi_pitch || !same_time(was->offset, now->offset) ||
                !same_time(was->duration, now->duration)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

size_t given_snapshot_report(const GivenSnapshot *snapshot, const PiecePlan *plan, GivenReport *out, size_t capacity) {
    size_t runs = 0;
    for (size_t i = 0; snapshot && plan && i < snapshot->count; i++) {
        const SnapshotEntry *entry = &snapshot->entries[i];
        bool unchanged = entry_unchanged(entry, plan);
        GivenReport *last = runs > 0 && runs <= capacity && out ? &out[runs - 1] : NULL;
        PlanTime last_end;
        if (last && last->lane == entry->lane && last->voice == entry->voice && last->owner == entry->owner &&
            plan_span_end(last->span, &last_end) == PLAN_OK && same_time(last_end, entry->span.start)) {
            PlanTime end;
            if (plan_span_end(entry->span, &end) == PLAN_OK) {
                last->span = between(last->span.start, end);
            }
            last->spans++;
            last->unchanged = last->unchanged && unchanged;
            continue;
        }
        if (out && runs < capacity) {
            out[runs].lane = entry->lane;
            out[runs].voice = entry->voice;
            out[runs].owner = entry->owner;
            out[runs].span = entry->span;
            out[runs].spans = 1;
            out[runs].unchanged = unchanged;
        }
        runs++;
    }
    return runs;
}
