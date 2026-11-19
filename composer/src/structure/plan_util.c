#include "structure/plan_util.h"

#include <string.h>

#include <model/time/note_value.h>
#include <model/time/time_signature.h>
#include <theory/meter/metric_position_rules.h>

#include "util/position.h"

bool plan_in_subtree(const Plan *plan, NodeId node, NodeId root) {
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (node == root) {
            return true;
        }
    }
    return false;
}

bool plan_melody_span_of(const Plan *plan, NodeId node, LaneSpanId *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            *out = ids[i];
            return true;
        }
    }
    return false;
}

bool plan_section_function_of(const Plan *plan, NodeId node, SectionFunction *out) {
    const SectionDescription *section = plan_node_section(plan, node);
    if (!section || !section->has_function) {
        return false;
    }
    *out = section->function;
    return true;
}

PieceResult piece_result_of_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

bool plan_add_given_node(Plan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out) {
    return plan_add_node(plan, parent, kind, span, NULL, out) == PLAN_OK &&
           plan_node_set_state(plan, *out, PLAN_STATE_GIVEN) == PLAN_OK;
}

const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT] = {"any", "leading", "trailing", "middle"};

const char *plan_fragment_position_name(long position) {
    return position >= 0 && position < PLAN_FRAGMENT_POSITION_NAME_COUNT ? plan_fragment_position_names[position] : "any";
}

const char *piece_result_name(PieceResult result) {
    switch (result) {
    case PIECE_OK:
        return "ok";
    case PIECE_ERR_NULL:
        return "null argument";
    case PIECE_ERR_ALLOC:
        return "allocation failed";
    case PIECE_ERR_MOTIVE_TOO_LONG:
        return "motive too long";
    case PIECE_ERR_BAD_REQUEST:
        return "bad request";
    case PIECE_ERR_STUCK:
        return "stuck";
    case PIECE_ERR_BAD_SIZE:
        return "bad size";
    }
    return "unknown";
}

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

bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    TimeSignature *signature = NULL;
    unsigned beats[RHYTHM_MAX_CELLS];
    size_t count = 0;
    if (!cells || !meter_signature(meter, &signature)) {
        return false;
    }
    bool ok = standard_meter_accent_beats(signature, beats, NULL, RHYTHM_MAX_CELLS, &count);
    bool compound = time_signature_is_compound(signature);
    size_t groups = time_signature_group_count(signature);
    time_signature_destroy(signature);
    if (!ok || count == 0) {
        return false;
    }
    /* The meters this operator writes: simple duple, triple and quadruple over quarters, and 6/8. */
    if (compound && meter->beat_unit == 8 && meter->beats_per_bar == 6) {
        *cells = groups; /* the two dotted-quarter beats */
        return true;
    }
    if (meter->beat_unit == 4 && meter->beats_per_bar >= 2 && meter->beats_per_bar <= 4) {
        *cells = count;
        return true;
    }
    return false;
}

bool harmony_meter_supported(const PlanMeter *meter) {
    size_t cells;
    return harmony_rhythm_cells_per_bar(meter, &cells);
}

bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    return harmony_rhythm_cells_per_bar(meter, cells);
}

NodeId seam_section_at(const Plan *plan, Rational time) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
            rational_cmp(plan_node_span(plan, id).start, time) == 0 &&
            plan_section_has_role(plan, id, SECTION_ROLE_SEAM)) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* The notes of a lane over the target that sound inside it, from its start. False if any span there is open. */
bool accomp_read_line(const Plan *plan, PlanLaneKind lane, int voice, PlanSpan span, Rational origin,
                      Rational length, AccompLine *out, size_t *count) {
    LaneSpanId ids[ACCOMP_MAX_LINE];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, ACCOMP_MAX_LINE);
    *count = 0;
    if (found == 0 || found > ACCOMP_MAX_LINE) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        Rational base;
        const PlanNotes *notes;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
        base = plan_lane_span_span(plan, ids[i]).start;
        notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational duration = notes->notes[n].duration;
            if (!position_is_positive(duration)) {
                return false;
            }
            Rational onset = position_add(position_sub(base, origin), notes->notes[n].offset);
            Rational end = position_add(onset, duration);
            if (rational_cmp(end, POSITION_ZERO) <= 0 || rational_cmp(onset, length) >= 0) {
                continue;
            }
            if (*count >= ACCOMP_MAX_LINE) {
                return false;
            }
            out[*count].onset = rational_cmp(onset, POSITION_ZERO) < 0 ? POSITION_ZERO : onset;
            out[*count].end = rational_cmp(end, length) > 0 ? length : end;
            out[*count].pitch = notes->notes[n].midi;
            (*count)++;
        }
    }
    return true;
}
