#include "util/plan_util.h"

#include <model/identity/entity_id.h>
#include <model/time/meter_arithmetic.h>
#include <model/time/meter_map.h>
#include <model/time/note_value.h>

const CadenceType plan_cadence_types[PLAN_CADENCE_COUNT] = {CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_IMPERFECT_AUTHENTIC,
                                                            CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE,
                                                            CADENCE_TYPE_PLAGAL};
const char *const plan_cadence_names[PLAN_CADENCE_COUNT] = {"PAC", "IAC", "HC", "DC", "PC"};
const char *const plan_cadence_file_names[PLAN_CADENCE_COUNT] = {"pac", "iac", "hc", "dc", "pc"};

bool plan_meter_signature(PlanMeter meter, TimeSignature **out) {
    int exponent = -1;
    for (int candidate = TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT; candidate <= TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT;
         candidate++) {
        if (meter.beat_unit == (1 << candidate)) {
            exponent = candidate;
        }
    }
    if (out == NULL || exponent < 0 || meter.beats_per_bar < 1) {
        return false;
    }
    NoteValue *unit = NULL;
    if (note_value_create(exponent, &unit) != NOTE_VALUE_OK) {
        return false;
    }
    TimeSignatureResult made = time_signature_create((unsigned)meter.beats_per_bar, unit, out);
    note_value_destroy(unit);
    return made == TIME_SIGNATURE_OK;
}

/* The grid of the first measure of a one-region map of the meter. The map owns the signature the grid borrows, so the
   caller destroys *map when it is done with the grid. */
static bool first_measure_grid(PlanMeter meter, MeterMap **map, MeterGrid *grid) {
    TimeSignature *signature = NULL;
    EntityID *id = NULL;
    bool ok = false;
    *map = NULL;
    if (plan_meter_signature(meter, &signature) && entity_id_create("composer-plan-meter", &id) == ENTITY_ID_OK) {
        MeterMapChangePoint start = {signature, musical_time_of(rational_ratio(0, 1))};
        if (meter_map_create(id, &start, 1, map) != METER_MAP_OK) {
            entity_id_destroy(id);
        } else if (meter_map_grid_at(*map, start.start, grid) == METER_MAP_OK) {
            ok = true;
        } else {
            meter_map_destroy(*map);
            *map = NULL;
        }
    }
    time_signature_destroy(signature);
    return ok;
}

bool plan_meter_grid(PlanMeter meter, Rational *beat, Rational *division) {
    MeterMap *map = NULL;
    MeterGrid grid;
    if (!first_measure_grid(meter, &map, &grid)) {
        return false;
    }
    *beat = grid.beat;
    *division = grid.division;
    meter_map_destroy(map);
    return true;
}

bool plan_meter_pickup_on_grid(PlanMeter meter, Rational pickup) {
    if (pickup.den <= 0 || pickup.num < 0) {
        return false;
    }
    MeterMap *map = NULL;
    MeterGrid grid;
    if (!first_measure_grid(meter, &map, &grid)) {
        return false;
    }
    bool holds = pickup.num == 0 || meter_grid_holds(&grid, pickup, rational_ratio(1, PLAN_PICKUP_FINEST));
    meter_map_destroy(map);
    return holds;
}

bool plan_cadence_index(CadenceType type, size_t *index) {
    for (size_t i = 0; i < PLAN_CADENCE_COUNT; i++) {
        if (plan_cadence_types[i] == type) {
            if (index) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

const char *plan_cadence_name(CadenceType type) {
    size_t index;
    return plan_cadence_index(type, &index) ? plan_cadence_names[index] : "";
}

PlanResult plan_result_of_rational(RationalResult result) {
    if (result == RATIONAL_OK) {
        return PLAN_OK;
    }
    return result == RATIONAL_ERR_NULL ? PLAN_ERR_NULL : PLAN_ERR_BAD_TIME;
}

bool plan_spans_overlap(PlanSpan a, PlanSpan b) {
    Rational a_end;
    Rational b_end;
    if (plan_span_end(a, &a_end) != PLAN_OK || plan_span_end(b, &b_end) != PLAN_OK) {
        return false;
    }
    return rational_cmp(a.start, b_end) < 0 && rational_cmp(b.start, a_end) < 0;
}

bool plan_is_note_lane(PlanLaneKind lane) {
    return lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY || lane == PLAN_LANE_INNER_VOICE;
}

bool plan_in_subtree(const PiecePlan *plan, NodeId node, NodeId root) {
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (node == root) {
            return true;
        }
    }
    return false;
}

bool plan_melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out) {
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

bool plan_section_function_of(const PiecePlan *plan, NodeId node, SectionFunction *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_SECTION_FUNCTION) {
            *out = constraint->params.section_function;
            return true;
        }
    }
    return false;
}

PieceResult piece_result_of_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

bool plan_add_given_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out) {
    return plan_add_node(plan, parent, kind, span, NULL, out) == PLAN_OK &&
           plan_node_set_state(plan, *out, PLAN_STATE_GIVEN) == PLAN_OK;
}

const char *const plan_lane_names[PLAN_LANE_NAME_COUNT] = {"meter", "key", "harmony", "bass", "melody", "inner_voice"};

const char *plan_lane_name(PlanLaneKind lane) {
    return (int)lane >= 0 && (int)lane < PLAN_LANE_NAME_COUNT ? plan_lane_names[lane] : "";
}

const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT] = {"any", "leading", "trailing", "middle"};

const char *plan_fragment_position_name(long position) {
    return position >= 0 && position < PLAN_FRAGMENT_POSITION_NAME_COUNT ? plan_fragment_position_names[position] : "any";
}
