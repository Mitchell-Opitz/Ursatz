#include "plan/plan_common.h"

#include <string.h>

#include <model/identity/entity_id.h>
#include <model/time/meter_arithmetic.h>
#include <model/time/meter_map.h>
#include <model/time/note_value.h>
#include <theory/tonality/common_practice_materials.h>

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

const char *const plan_lane_names[PLAN_LANE_NAME_COUNT] = {"meter", "key", "harmony", "bass", "melody", "inner_voice"};

const char *plan_lane_name(PlanLaneKind lane) {
    return (int)lane >= 0 && (int)lane < PLAN_LANE_NAME_COUNT ? plan_lane_names[lane] : "";
}

PlanResult plan_meter_bar_length(const PlanMeter *meter, Rational *out) {
    if (!meter || !out) {
        return PLAN_ERR_NULL;
    }
    RationalResult made = rational_make(meter->beats_per_bar, (long long)meter->beat_unit, out);
    if (made != RATIONAL_OK) {
        out->num = 0;
        out->den = 1;
    }
    return plan_result_of_rational(made);
}

bool plan_key_valid(const SpelledKey *key) {
    CommonPracticeKeyMode mode;
    return key && (key->material == NULL || common_practice_key_mode_of(key->material, &mode));
}

#define MAJOR_MATERIAL_ID "omt.material.major"

const char *plan_key_material_id(const SpelledKey *key) {
    return key && key->material ? key->material->id : MAJOR_MATERIAL_ID;
}

const char *plan_key_word(const SpelledKey *key) {
    return key && key->material ? key->material->key_word : "major";
}
