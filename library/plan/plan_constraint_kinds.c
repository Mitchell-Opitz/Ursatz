#include "plan/plan_constraint_kinds.h"

#include <stdio.h>
#include <string.h>

#include <common/json.h>
#include <theory/vocabulary/texture_rules.h>
#include <theory/galant/galant_prototypes.h>
#include "plan/plan_lanes.h"

#include "plan/plan_internal.h"
#include "plan/plan_common.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define TIME_LIMIT 1000000000L
#define STEP_TEXT_MAX 160

static const char *const SHAPE_KIND_NAMES[] = {"prolongation", "phrase_model", "cadential"};
static const char *const ANCHOR_NAMES[] = {"any", "tonic", "dominant", "pivot"};

/* The meter the plan starts in, from its first meter lane span; 4/4 while it has none (a pickup on 1/32 is on every
   meter's grid, so only a triplet pickup depends on which meter is chosen). */
static PlanMeter first_meter(const Plan *plan) {
    LaneSpanId id;
    PlanSpan whole = plan_node_span(plan, plan_root(plan));
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, whole, &id, 1) == 0) {
        return (PlanMeter){4, 4, 0};
    }
    return plan_lane_span_payload(plan, id)->meter;
}

/* ------------------------------------------------------------------ reading helpers of the kinds */

/*
 * Plan files saved before relation steps write a motive ref as "transform" and the members it uses. This is the only
 * mapping of those names: it is read-only, and nothing writes them. The table (docs/modules/motive-relations.md, Composer
 * fields to relation parameters):
 *   none             no step
 *   restated         exact
 *   transposed       transpose: tonal, by = steps when tonal, else semitones
 *   inverted         invert: axis (-1 without one), tonal
 *   retrograde       retrograde
 *   augmented        augment: ratio_num:ratio_den
 *   diminished       diminish: ratio_num:ratio_den
 *   fragmented       fragment: position, length
 *   interval_changed interval-change: interval, amount, keep_contour
 *   liquidated       liquidate: drop, keep_rhythm
 */
enum {
    LEGACY_NONE,
    LEGACY_TRANSPOSED,
    LEGACY_INVERTED,
    LEGACY_RETROGRADE,
    LEGACY_AUGMENTED,
    LEGACY_DIMINISHED,
    LEGACY_FRAGMENTED,
    LEGACY_RESTATED,
    LEGACY_INTERVAL_CHANGED,
    LEGACY_LIQUIDATED
};
static const char *const LEGACY_TRANSFORM_NAMES[] = {"none",       "transposed",       "inverted",   "retrograde",
                                                     "augmented",  "diminished",       "fragmented", "restated",
                                                     "interval_changed", "liquidated"};
static const char *const LEGACY_POSITION_NAMES[] = {"any", "leading", "trailing", "middle"};

/* An optional integer member; absent is 0. */
static bool json_read_optional_int(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                              long max, int *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    *out = 0;
    if (!item) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    long value;
    json_child_path(at, path, key);
    if (!json_read_long(reader, item, at, min, max, &value)) {
        return false;
    }
    *out = (int)value;
    return true;
}


static bool plan_motive_ref_from_transform(JsonReader *reader, const cJSON *item, const char *path, const char *name,
                                           PlanMotiveRef *ref) {
    int transform = -1;
    for (size_t i = 0; i < COUNT_OF(LEGACY_TRANSFORM_NAMES); i++) {
        if (strcmp(name, LEGACY_TRANSFORM_NAMES[i]) == 0) {
            transform = (int)i;
        }
    }
    if (transform < 0) {
        char message[PLAN_JSON_PATH_MAX + 64];
        snprintf(message, sizeof(message), "unknown motive transform \"%.64s\"", name);
        return json_bad_format(reader, path, message);
    }
    int semitones, steps, axis, interval, amount, length, drop, ratio_num, ratio_den;
    bool tonal, keep_contour, keep_rhythm;
    int position = 0;
    const cJSON *position_item = cJSON_GetObjectItemCaseSensitive(item, "position");
    if (position_item) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "position");
        if (!json_read_enum(reader, position_item, at, LEGACY_POSITION_NAMES, COUNT_OF(LEGACY_POSITION_NAMES), &position)) {
            return false;
        }
    }
    bool has_axis = cJSON_GetObjectItemCaseSensitive(item, "axis") != NULL;
    if (!json_read_optional_int(reader, item, path, "semitones", -127, 127, &semitones) ||
        !json_read_optional_int(reader, item, path, "axis", 0, 127, &axis) ||
        !json_read_optional_bool(reader, item, path, "tonal", &tonal) ||
        !json_read_optional_bool(reader, item, path, "keep_contour", &keep_contour) ||
        !json_read_optional_bool(reader, item, path, "keep_rhythm", &keep_rhythm) ||
        !json_read_optional_int(reader, item, path, "steps", -127, 127, &steps) ||
        !json_read_optional_int(reader, item, path, "interval", 0, 127, &interval) ||
        !json_read_optional_int(reader, item, path, "amount", -127, 127, &amount) ||
        !json_read_optional_int(reader, item, path, "length", 0, 127, &length) ||
        !json_read_optional_int(reader, item, path, "drop", 0, 127, &drop) ||
        !json_read_optional_int(reader, item, path, "ratio_num", 0, 64, &ratio_num) ||
        !json_read_optional_int(reader, item, path, "ratio_den", 0, 64, &ratio_den)) {
        return false;
    }
    if ((ratio_num == 0) != (ratio_den == 0)) {
        return json_bad_format(reader, path, "ratio_num and ratio_den go together");
    }
    MotiveRelationStep *step = &ref->step;
    memset(step, 0, sizeof(*step));
    ref->has_step = transform != LEGACY_NONE;
    switch (transform) {
    case LEGACY_RESTATED:
        step->kind = MOTIVE_RELATION_EXACT;
        break;
    case LEGACY_TRANSPOSED:
        step->kind = MOTIVE_RELATION_TRANSPOSE;
        step->value[0] = tonal ? 1 : 0;
        step->value[1] = tonal ? steps : semitones;
        break;
    case LEGACY_INVERTED:
        step->kind = MOTIVE_RELATION_INVERT;
        step->value[0] = has_axis ? axis : -1;
        step->value[1] = tonal ? 1 : 0;
        break;
    case LEGACY_RETROGRADE:
        step->kind = MOTIVE_RELATION_RETROGRADE;
        break;
    case LEGACY_AUGMENTED:
    case LEGACY_DIMINISHED:
        step->kind = transform == LEGACY_AUGMENTED ? MOTIVE_RELATION_AUGMENT : MOTIVE_RELATION_DIMINISH;
        step->value[0] = ratio_num;
        step->den[0] = ratio_den;
        break;
    case LEGACY_FRAGMENTED:
        step->kind = MOTIVE_RELATION_FRAGMENT;
        step->value[0] = position;
        step->value[1] = length;
        break;
    case LEGACY_INTERVAL_CHANGED:
        step->kind = MOTIVE_RELATION_INTERVAL_CHANGE;
        step->value[0] = interval;
        step->value[1] = amount;
        step->value[2] = keep_contour ? 1 : 0;
        break;
    case LEGACY_LIQUIDATED:
        step->kind = MOTIVE_RELATION_LIQUIDATE;
        step->value[0] = drop;
        step->value[1] = keep_rhythm ? 1 : 0;
        break;
    default:
        break;
    }
    return true;
}

/* A motive ref: "step" through motive_relation_step_parse, else a "transform" of an older file, else no step. */
static bool read_motive_ref_members(JsonReader *reader, const cJSON *item, const char *path, PlanMotiveRef *ref) {
    long number_value;
    if (!json_read_int_member(reader, item, path, "motive", 1, TIME_LIMIT, &number_value)) {
        return false;
    }
    ref->motive = (NodeId)number_value;
    /* The segment and the liquidation steps the motive is made from, both optional (ch. VIII). */
    const cJSON *segment = cJSON_GetObjectItemCaseSensitive(item, "segment");
    /* An older file's "transform" has a "steps" of its own (plan_motive_ref_from_transform), which this is not. */
    const cJSON *steps = cJSON_GetObjectItemCaseSensitive(item, "transform")
                             ? NULL
                             : cJSON_GetObjectItemCaseSensitive(item, "steps");
    char at[PLAN_JSON_PATH_MAX];
    if (segment) {
        json_child_path(at, path, "segment");
        if (!cJSON_IsString(segment) || segment->valuestring[0] == '\0' ||
            strlen(segment->valuestring) > PLAN_SEGMENT_LABEL_MAX) {
            return json_bad_format(reader, at, "expected a segment label of 1 to 8 characters");
        }
        strcpy(ref->segment, segment->valuestring);
    }
    if (steps) {
        long count;
        json_child_path(at, path, "steps");
        if (!json_read_long(reader, steps, at, 1, 8, &count)) {
            return false;
        }
        ref->steps = (size_t)count;
    }
    const cJSON *step = cJSON_GetObjectItemCaseSensitive(item, "step");
    const cJSON *transform = cJSON_GetObjectItemCaseSensitive(item, "transform");
    if (step) {
        json_child_path(at, path, "step");
        if (!cJSON_IsString(step)) {
            return json_bad_format(reader, at, "expected a relation step like \"transpose tonal=1 by=2\"");
        }
        if (!motive_relation_step_parse(step->valuestring, &ref->step)) {
            char message[PLAN_JSON_PATH_MAX + 64];
            snprintf(message, sizeof(message), "not a relation step \"%.64s\"", step->valuestring);
            return json_bad_format(reader, at, message);
        }
        ref->has_step = true;
        return true;
    }
    if (transform) {
        json_child_path(at, path, "transform");
        if (!cJSON_IsString(transform)) {
            return json_bad_format(reader, at, "expected a string");
        }
        return plan_motive_ref_from_transform(reader, item, path, transform->valuestring, ref);
    }
    return true;
}

/* A time written "n/d" (the slash is required: a bare "n" is refused) with n from `least` to TIME_LIMIT and d from 1 to TIME_LIMIT. */
static bool read_fraction_text(const cJSON *text, long least, Rational *out) {
    long long numerator;
    long long denominator;
    return cJSON_IsString(text) && text->valuestring && strchr(text->valuestring, '/') != NULL &&
           common_json_rational_read(text, COMMON_RATIONAL_TEXT, &numerator, &denominator) && numerator >= least &&
           numerator <= TIME_LIMIT && denominator >= 1 && denominator <= TIME_LIMIT &&
           rational_make(numerator, denominator, out) == RATIONAL_OK;
}

/* A time like "1/4" in whole notes: the numerator not below `least`. */
static bool read_tones_time(JsonReader *reader, const cJSON *item, const char *path, const char *name, long least,
                            Rational *out) {
    const cJSON *text = plan_json_member(reader, item, path, name, true);
    if (!text) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, name);
    if (!read_fraction_text(text, least, out)) {
        return json_bad_format(reader, at, "expected a time in whole notes like \"1/4\"");
    }
    return true;
}


static bool read_motive_tones_members(JsonReader *reader, const cJSON *item, const char *path, PlanMotiveTones *out) {
    const cJSON *harmony = plan_json_member(reader, item, path, "harmony", true);
    const cJSON *tones = harmony ? plan_json_member(reader, item, path, "tones", true) : NULL;
    char at[PLAN_JSON_PATH_MAX];
    if (!tones) {
        return false;
    }
    json_child_path(at, path, "harmony");
    if (!cJSON_IsArray(harmony) || cJSON_GetArraySize(harmony) < 1 || cJSON_GetArraySize(harmony) > PLAN_TONES_MAX_CHORDS) {
        return json_bad_format(reader, at, "expected one to eight chords");
    }
    const cJSON *chord;
    cJSON_ArrayForEach(chord, harmony) {
        char chord_path[PLAN_JSON_PATH_MAX];
        json_index_path(chord_path, at, out->chord_count);
        const cJSON *numeral = plan_json_member(reader, chord, chord_path, "numeral", true);
        PlanToneChord *slot = &out->chords[out->chord_count];
        if (!numeral) {
            return false;
        }
        if (!cJSON_IsString(numeral) || numeral->valuestring[0] == '\0' ||
            strlen(numeral->valuestring) >= PLAN_ROMAN_NUMERAL_MAX) {
            return json_bad_format(reader, chord_path, "expected a Roman numeral of fewer than sixteen characters");
        }
        snprintf(slot->numeral, sizeof(slot->numeral), "%s", numeral->valuestring);
        if (!read_tones_time(reader, chord, chord_path, "onset", 0, &slot->onset) ||
            !read_tones_time(reader, chord, chord_path, "length", 1, &slot->length)) {
            return false;
        }
        out->chord_count++;
    }
    json_child_path(at, path, "tones");
    if (!cJSON_IsArray(tones) || cJSON_GetArraySize(tones) < 1 || cJSON_GetArraySize(tones) > PLAN_TONES_MAX_NOTES) {
        return json_bad_format(reader, at, "expected one to sixty-four labels");
    }
    const cJSON *label;
    cJSON_ArrayForEach(label, tones) {
        if (!cJSON_IsString(label) || label->valuestring[0] == '\0' || strlen(label->valuestring) >= PLAN_TONE_LABEL_MAX) {
            char label_path[PLAN_JSON_PATH_MAX];
            json_index_path(label_path, at, out->label_count);
            return json_bad_format(reader, label_path, "expected a tone label like \"chord\" or \"passing\"");
        }
        snprintf(out->labels[out->label_count++], PLAN_TONE_LABEL_MAX, "%s", label->valuestring);
    }
    return true;
}


/* ------------------------------------------------------------------ the kinds */

static bool valid_length_bars(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return constraint->params.bars > 0;
}

static bool write_length_bars(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    ok = ok && plan_json_add(object, "bars", json_number(constraint->params.bars));
    return ok;
}

static bool read_length_bars(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    long number_value;
    if (!json_read_int_member(reader, item, path, "bars", 1, TIME_LIMIT, &number_value)) {
        return false;
    }
    out->params.bars = (int)number_value;
    return true;
}

static bool valid_cadence(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return plan_cadence_index(constraint->params.cadence, NULL);
}

static bool write_cadence(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    size_t cadence_index = 0;
    ok = ok && plan_cadence_index(constraint->params.cadence, &cadence_index) &&
         plan_json_add(object, "type", cJSON_CreateString(plan_cadence_file_names[cadence_index]));
    return ok;
}

static bool read_cadence(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    int value;
    if (!json_read_enum_member(reader, item, path, "type", plan_cadence_file_names, PLAN_CADENCE_COUNT, &value)) {
        return false;
    }
    out->params.cadence = plan_cadence_types[value];
    return true;
}

static bool valid_motive_ref(const Plan *plan, const PlanConstraint *constraint) {
    const PlanMotiveRef *ref = &constraint->params.motive;
    return plan_has_node(plan, ref->motive) && plan_node_kind(plan, ref->motive) == PLAN_NODE_MOTIVE &&
           (!ref->has_step || (ref->step.kind >= 0 && ref->step.kind < MOTIVE_RELATION_KIND_COUNT));
}

static bool write_motive_ref(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    ok = ok && plan_json_add(object, "motive", json_number((long)plan_node_rank(plan, constraint->params.motive.motive)));
    if (ok && constraint->params.motive.has_step) {
        char text[STEP_TEXT_MAX];
        ok = motive_relation_step_text(&constraint->params.motive.step, text, sizeof(text)) &&
             plan_json_add(object, "step", cJSON_CreateString(text));
    }
    if (ok && constraint->params.motive.segment[0] != '\0') {
        ok = plan_json_add(object, "segment", cJSON_CreateString(constraint->params.motive.segment));
    }
    if (ok && constraint->params.motive.steps > 0) {
        ok = plan_json_add(object, "steps", json_number((long)constraint->params.motive.steps));
    }
    return ok;
}

static bool read_motive_ref(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    return read_motive_ref_members(reader, item, path, &out->params.motive);
}

static bool valid_key(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return spelled_key_tonic_pitch_class(&constraint->params.region) >= 0 && spelled_key_tonic_pitch_class(&constraint->params.region) <= 11 &&
           plan_key_valid(&constraint->params.region);
}

static bool write_key(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    ok = ok && json_key_members(object, &constraint->params.region);
    return ok;
}

static bool read_key(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    return json_read_key_members(reader, item, path, &out->params.region);
}

static bool valid_embellish(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    (void)constraint;
    return true;
}

static bool write_embellish(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    (void)constraint;
    (void)object;
    return ok;
}

static bool read_embellish(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    (void)reader;
    (void)item;
    (void)path;
    (void)out;
    return true;
}

static bool valid_schema(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const GalantPrototype *prototype = galant_prototype((GalantSchemaId)constraint->params.schema.schema);
    return prototype && constraint->params.schema.stage >= 0 &&
           (size_t)constraint->params.schema.stage < prototype->stage_count;
}

static bool write_schema(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    ok = ok && plan_json_add(object, "schema", cJSON_CreateString(galant_schema_name(
                                                (GalantSchemaId)constraint->params.schema.schema))) &&
         plan_json_add(object, "stage", json_number(constraint->params.schema.stage));
    return ok;
}

static bool read_schema(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    long number_value;
    const cJSON *name = plan_json_member(reader, item, path, "schema", true);
    if (!name) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "schema");
    if (!cJSON_IsString(name)) {
        return json_bad_format(reader, at, "expected a schema name");
    }
    int schema = -1;
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        if (strcmp(galant_schema_name((GalantSchemaId)id), name->valuestring) == 0) {
            schema = id;
        }
    }
    if (schema < 0) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown schema '%.40s'", name->valuestring);
    }
    const GalantPrototype *prototype = galant_prototype((GalantSchemaId)schema);
    if (!json_read_int_member(reader, item, path, "stage", 0, (long)prototype->stage_count - 1,
                              &number_value)) {
        return false;
    }
    out->params.schema.schema = schema;
    out->params.schema.stage = (int)number_value;
    return true;
}

static bool valid_harmony_shape(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanHarmonyShape *shape = &constraint->params.harmony_shape;
    return shape->kind >= PLAN_HARMONY_SHAPE_PROLONGATION && shape->kind <= PLAN_HARMONY_SHAPE_CADENTIAL &&
           shape->starts_on >= PLAN_HARMONY_ANCHOR_ANY && shape->starts_on <= PLAN_HARMONY_ANCHOR_PIVOT &&
           shape->ends_on >= PLAN_HARMONY_ANCHOR_ANY && shape->ends_on <= PLAN_HARMONY_ANCHOR_DOMINANT &&
           shape->max_cells_per_chord >= 0 &&
           (!shape->pedal || (shape->kind == PLAN_HARMONY_SHAPE_PROLONGATION &&
                              shape->starts_on == PLAN_HARMONY_ANCHOR_DOMINANT &&
                              shape->ends_on == PLAN_HARMONY_ANCHOR_DOMINANT));
}

static bool write_harmony_shape(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    const PlanHarmonyShape *shape = &constraint->params.harmony_shape;
    ok = ok && plan_json_add(object, "shape", cJSON_CreateString(SHAPE_KIND_NAMES[shape->kind])) &&
         plan_json_add(object, "starts_on", cJSON_CreateString(ANCHOR_NAMES[shape->starts_on])) &&
         plan_json_add(object, "ends_on", cJSON_CreateString(ANCHOR_NAMES[shape->ends_on])) &&
         plan_json_add(object, "max_cells_per_chord", json_number(shape->max_cells_per_chord)) &&
         plan_json_add(object, "split_at_bars", cJSON_CreateBool(shape->split_at_bars));
    if (ok && shape->pedal) {
        ok = plan_json_add(object, "pedal", cJSON_CreateBool(true));
    }
    if (ok && shape->free_motive) {
        ok = plan_json_add(object, "free_motive", cJSON_CreateBool(true));
    }
    return ok;
}

static bool read_harmony_shape(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    int value;
    PlanHarmonyShape *shape = &out->params.harmony_shape;
    int anchor;
    if (!json_read_enum_member(reader, item, path, "shape", SHAPE_KIND_NAMES, COUNT_OF(SHAPE_KIND_NAMES), &value)) {
        return false;
    }
    shape->kind = (PlanHarmonyShapeKind)value;
    const cJSON *starts = cJSON_GetObjectItemCaseSensitive(item, "starts_on");
    const cJSON *ends = cJSON_GetObjectItemCaseSensitive(item, "ends_on");
    char at[PLAN_JSON_PATH_MAX];
    if (starts) {
        json_child_path(at, path, "starts_on");
        if (!json_read_enum(reader, starts, at, ANCHOR_NAMES, COUNT_OF(ANCHOR_NAMES), &anchor)) {
            return false;
        }
        shape->starts_on = (PlanHarmonyAnchor)anchor;
    }
    if (ends) {
        json_child_path(at, path, "ends_on");
        if (!json_read_enum(reader, ends, at, ANCHOR_NAMES, COUNT_OF(ANCHOR_NAMES), &anchor)) {
            return false;
        }
        shape->ends_on = (PlanHarmonyAnchor)anchor;
    }
    return json_read_optional_int(reader, item, path, "max_cells_per_chord", 0, 64, &shape->max_cells_per_chord) &&
           json_read_optional_bool(reader, item, path, "split_at_bars", &shape->split_at_bars) &&
           json_read_optional_bool(reader, item, path, "pedal", &shape->pedal) &&
           json_read_optional_bool(reader, item, path, "free_motive", &shape->free_motive);
}

static bool valid_accompaniment(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanAccompaniment *accompaniment = &constraint->params.accompaniment;
    int low = accompaniment->low ? accompaniment->low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
    int high = accompaniment->high ? accompaniment->high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;
    return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_ENTRY &&
           (accompaniment->pattern == PLAN_ACCOMPANIMENT_MOTIVE) == (accompaniment->motive_node[0] != '\0') &&
           (accompaniment->pattern == PLAN_ACCOMPANIMENT_ENTRY) == (accompaniment->pattern_id[0] != '\0') &&
           memchr(accompaniment->motive_node, '\0', sizeof(accompaniment->motive_node)) &&
           memchr(accompaniment->pattern_id, '\0', sizeof(accompaniment->pattern_id)) &&
           accompaniment->texture >= -1 && accompaniment->texture < (int)STANDARD_TEXTURE_COUNT &&
           accompaniment->low >= 0 && accompaniment->low <= 127 && accompaniment->high >= 0 &&
           accompaniment->high <= 127 && low < high;
}

static bool write_accompaniment(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    const PlanAccompaniment *accompaniment = &constraint->params.accompaniment;
    ok = ok && plan_json_add(object, "pattern", cJSON_CreateString(JSON_ACCOMPANIMENT_NAMES[accompaniment->pattern]));
    if (accompaniment->texture >= 0) {
        ok = ok && plan_json_add(object, "texture",
                            cJSON_CreateString(standard_texture_name((StandardTexture)accompaniment->texture)));
    }
    if (accompaniment->low != 0) {
        ok = ok && plan_json_add(object, "low", json_number(accompaniment->low));
    }
    if (accompaniment->high != 0) {
        ok = ok && plan_json_add(object, "high", json_number(accompaniment->high));
    }
    if (accompaniment->motive_node[0] != '\0') {
        ok = ok && plan_json_add(object, "motive_node", cJSON_CreateString(accompaniment->motive_node));
    }
    if (accompaniment->pattern_id[0] != '\0') {
        ok = ok && plan_json_add(object, "pattern_id", cJSON_CreateString(accompaniment->pattern_id));
    }
    return ok;
}

static bool read_accompaniment(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    int value;
    PlanAccompaniment *accompaniment = &out->params.accompaniment;
    const cJSON *texture = cJSON_GetObjectItemCaseSensitive(item, "texture");
    accompaniment->texture = -1;
    if (!json_read_enum_member(reader, item, path, "pattern", JSON_ACCOMPANIMENT_NAMES, COUNT_OF(JSON_ACCOMPANIMENT_NAMES), &value)) {
        return false;
    }
    accompaniment->pattern = (PlanAccompanimentPattern)value;
    if (texture) {
        char at[PLAN_JSON_PATH_MAX];
        StandardTexture named;
        json_child_path(at, path, "texture");
        if (!cJSON_IsString(texture)) {
            return json_bad_format(reader, at, "expected a texture name");
        }
        if (!standard_texture_from_name(texture->valuestring, &named)) {
            return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown texture '%.40s'", texture->valuestring);
        }
        accompaniment->texture = (int)named;
    }
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(item, "motive_node");
    if (node) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "motive_node");
        if (!cJSON_IsString(node) || node->valuestring[0] == '\0' ||
            strlen(node->valuestring) >= sizeof(accompaniment->motive_node)) {
            return json_bad_format(reader, at, "expected a node name");
        }
        memcpy(accompaniment->motive_node, node->valuestring, strlen(node->valuestring) + 1);
    }
    const cJSON *entry = cJSON_GetObjectItemCaseSensitive(item, "pattern_id");
    if (entry) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "pattern_id");
        if (!cJSON_IsString(entry) || entry->valuestring[0] == '\0' || strlen(entry->valuestring) >= sizeof(accompaniment->pattern_id)) {
            return json_bad_format(reader, at, "expected a pattern entry id");
        }
        memcpy(accompaniment->pattern_id, entry->valuestring, strlen(entry->valuestring) + 1);
    }
    return json_read_optional_int(reader, item, path, "low", 0, 127, &accompaniment->low) &&
           json_read_optional_int(reader, item, path, "high", 0, 127, &accompaniment->high);
}

static bool valid_voicing(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    (void)constraint;
    return true;
}

static bool write_voicing(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    ok = ok && plan_json_add(object, "write_soprano", cJSON_CreateBool(constraint->params.voicing.write_soprano));
    return ok;
}

static bool read_voicing(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    return json_read_optional_bool(reader, item, path, "write_soprano", &out->params.voicing.write_soprano);
}

static bool valid_counterpoint(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return constraint->params.counterpoint.species >= 1 && constraint->params.counterpoint.species <= 4;
}

static bool write_counterpoint(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    ok = ok && plan_json_add(object, "species", json_number(constraint->params.counterpoint.species)) &&
         plan_json_add(object, "above", cJSON_CreateBool(constraint->params.counterpoint.above));
    return ok;
}

static bool read_counterpoint(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    if (!cJSON_GetObjectItemCaseSensitive(item, "species")) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "species");
        return json_bad_format(reader, at, "missing species");
    }
    return json_read_optional_int(reader, item, path, "species", 1, 4, &out->params.counterpoint.species) &&
           json_read_optional_bool(reader, item, path, "above", &out->params.counterpoint.above);
}

static bool valid_invent_motive(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    (void)constraint;
    return true;
}

static bool write_invent_motive(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    (void)constraint;
    (void)object;
    return ok;
}

static bool read_invent_motive(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    (void)reader;
    (void)item;
    (void)path;
    (void)out;
    return true;
}

static bool valid_style(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanStyle *style = &constraint->params.style;
    size_t length = strnlen(style->path, PLAN_STYLE_PATH_MAX);
    /* A weight of 0/0 is the default; any other has a denominator. */
    size_t blend_length = strnlen(style->blend_path, PLAN_STYLE_PATH_MAX);
    if (length == 0 || length >= PLAN_STYLE_PATH_MAX || blend_length >= PLAN_STYLE_PATH_MAX ||
        (style->weight_denominator == 0 && style->weight_numerator != 0)) {
        return false;
    }
    for (int d = 0; blend_length > 0 && d < 4; d++) {
        if (style->blend_denominator[d] == 0 || style->blend_numerator[d] > style->blend_denominator[d]) {
            return false;
        }
    }
    if (style->without_count > PLAN_STYLE_WITHOUT_MAX) {
        return false;
    }
    for (size_t i = 0; i < style->without_count; i++) {
        size_t name_length = strnlen(style->without[i], PLAN_STYLE_WITHOUT_NAME_MAX);
        if (name_length == 0 || name_length >= PLAN_STYLE_WITHOUT_NAME_MAX) {
            return false;
        }
    }
    return true;
}

static bool write_style(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    const PlanStyle *style = &constraint->params.style;
    char weight[64];
    ok = ok && plan_json_add(object, "path", cJSON_CreateString(style->path));
    snprintf(weight, sizeof(weight), "%lu/%lu", style->weight_numerator, style->weight_denominator);
    ok = ok && plan_json_add(object, "weight", cJSON_CreateString(weight));
    if (ok && style->without_count > 0) {
        cJSON *without = cJSON_CreateArray();
        ok = without != NULL;
        for (size_t i = 0; ok && i < style->without_count; i++) {
            ok = plan_json_append(without, cJSON_CreateString(style->without[i]));
        }
        if (ok) {
            ok = plan_json_add(object, "without", without);
        } else {
            cJSON_Delete(without);
        }
    }
    if (ok && style->blend_path[0]) {
        static const char *const DIMENSIONS[4] = {"harmony", "rhythm", "melody", "form"};
        cJSON *blend = cJSON_CreateObject();
        cJSON *weights = cJSON_CreateObject();
        bool built = blend && weights && plan_json_add(blend, "path", cJSON_CreateString(style->blend_path));
        for (int d = 0; built && d < 4; d++) {
            snprintf(weight, sizeof(weight), "%lu/%lu", style->blend_numerator[d], style->blend_denominator[d]);
            built = plan_json_add(weights, DIMENSIONS[d], cJSON_CreateString(weight));
        }
        if (built) {
            built = plan_json_add(blend, "weights", weights);
            weights = NULL;
        }
        cJSON_Delete(weights);
        if (built) {
            built = plan_json_add(object, "blend", blend);
        } else {
            cJSON_Delete(blend);
        }
        ok = built;
    }
    return ok;
}

static bool read_style(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    const cJSON *file = plan_json_member(reader, item, path, "path", true);
    const cJSON *weight = file ? plan_json_member(reader, item, path, "weight", true) : NULL;
    if (!file || !weight) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "path");
    if (!cJSON_IsString(file) || file->valuestring[0] == '\0' || strlen(file->valuestring) >= PLAN_STYLE_PATH_MAX) {
        return json_bad_format(reader, at, "expected a file path");
    }
    json_child_path(at, path, "weight");
    unsigned long numerator, denominator;
    char rest;
    if (!cJSON_IsString(weight) || sscanf(weight->valuestring, "%lu/%lu%c", &numerator, &denominator, &rest) != 2 ||
        (denominator == 0 && numerator != 0)) {
        return json_bad_format(reader, at, "expected a fraction like \"1/1\"");
    }
    snprintf(out->params.style.path, PLAN_STYLE_PATH_MAX, "%s", file->valuestring);
    out->params.style.weight_numerator = numerator;
    out->params.style.weight_denominator = denominator;
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(item, "without");
    if (without) {
        json_child_path(at, path, "without");
        int listed = cJSON_IsArray(without) ? cJSON_GetArraySize(without) : -1;
        if (listed < 0 || listed > PLAN_STYLE_WITHOUT_MAX) {
            return json_bad_format(reader, at, "expected a list of at most 8 category names");
        }
        const cJSON *name;
        cJSON_ArrayForEach(name, without) {
            if (!cJSON_IsString(name) || name->valuestring[0] == '\0' ||
                strlen(name->valuestring) >= PLAN_STYLE_WITHOUT_NAME_MAX) {
                return json_bad_format(reader, at, "expected a list of at most 8 category names");
            }
            snprintf(out->params.style.without[out->params.style.without_count++], PLAN_STYLE_WITHOUT_NAME_MAX, "%s",
                     name->valuestring);
        }
    }
    const cJSON *blend = cJSON_GetObjectItemCaseSensitive(item, "blend");
    if (!blend) {
        return true;
    }
    char blend_at[PLAN_JSON_PATH_MAX];
    json_child_path(blend_at, path, "blend");
    if (!cJSON_IsObject(blend)) {
        return json_bad_format(reader, blend_at, "expected an object with a path and weights");
    }
    const cJSON *blend_file = plan_json_member(reader, blend, blend_at, "path", true);
    const cJSON *blend_weights = blend_file ? plan_json_member(reader, blend, blend_at, "weights", true) : NULL;
    if (!blend_file || !blend_weights) {
        return false;
    }
    if (!cJSON_IsObject(blend_weights)) {
        json_child_path(at, blend_at, "weights");
        return json_bad_format(reader, at, "expected an object with a weight for each dimension");
    }
    json_child_path(at, blend_at, "path");
    if (!cJSON_IsString(blend_file) || blend_file->valuestring[0] == '\0' ||
        strlen(blend_file->valuestring) >= PLAN_STYLE_PATH_MAX) {
        return json_bad_format(reader, at, "expected a file path");
    }
    static const char *const DIMENSIONS[4] = {"harmony", "rhythm", "melody", "form"};
    for (int d = 0; d < 4; d++) {
        const cJSON *w = plan_json_member(reader, blend_weights, blend_at, DIMENSIONS[d], true);
        if (!w) {
            return false;
        }
        char w_at[PLAN_JSON_PATH_MAX];
        json_child_path(w_at, blend_at, DIMENSIONS[d]);
        if (!cJSON_IsString(w) || sscanf(w->valuestring, "%lu/%lu%c", &numerator, &denominator, &rest) != 2 ||
            denominator == 0 || numerator > denominator) {
            return json_bad_format(reader, w_at, "expected a fraction from 0 to 1 like \"1/2\"");
        }
        out->params.style.blend_numerator[d] = numerator;
        out->params.style.blend_denominator[d] = denominator;
    }
    snprintf(out->params.style.blend_path, PLAN_STYLE_PATH_MAX, "%s", blend_file->valuestring);
    return true;
}

static bool valid_source(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanSource *source = &constraint->params.source;
    size_t length = strnlen(source->piece, PLAN_SOURCE_PIECE_MAX);
    return length > 0 && length < PLAN_SOURCE_PIECE_MAX && source->score_denominator > 0 &&
           source->score_numerator <= source->score_denominator;
}

static bool write_source(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    const PlanSource *source = &constraint->params.source;
    char score[64];
    snprintf(score, sizeof(score), "%lu/%lu", source->score_numerator, source->score_denominator);
    ok = ok && plan_json_add(object, "piece", cJSON_CreateString(source->piece)) &&
         plan_json_add(object, "start_measure", cJSON_CreateNumber(source->start_measure)) &&
         plan_json_add(object, "start_beat", cJSON_CreateNumber(source->start_beat)) &&
         plan_json_add(object, "end_measure", cJSON_CreateNumber(source->end_measure)) &&
         plan_json_add(object, "end_beat", cJSON_CreateNumber(source->end_beat)) &&
         plan_json_add(object, "score", cJSON_CreateString(score));
    return ok;
}

static bool read_source(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    const cJSON *piece = plan_json_member(reader, item, path, "piece", true);
    const cJSON *start_measure = piece ? plan_json_member(reader, item, path, "start_measure", true) : NULL;
    const cJSON *start_beat = start_measure ? plan_json_member(reader, item, path, "start_beat", true) : NULL;
    const cJSON *end_measure = start_beat ? plan_json_member(reader, item, path, "end_measure", true) : NULL;
    const cJSON *end_beat = end_measure ? plan_json_member(reader, item, path, "end_beat", true) : NULL;
    const cJSON *score = end_beat ? plan_json_member(reader, item, path, "score", true) : NULL;
    if (!score) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "piece");
    if (!cJSON_IsString(piece) || piece->valuestring[0] == '\0' ||
        strlen(piece->valuestring) >= PLAN_SOURCE_PIECE_MAX) {
        return json_bad_format(reader, at, "expected the piece's path in the corpus");
    }
    if (!cJSON_IsNumber(start_measure) || !cJSON_IsNumber(start_beat) || !cJSON_IsNumber(end_measure) ||
        !cJSON_IsNumber(end_beat)) {
        return json_bad_format(reader, path, "expected the measures and beats of the passage as numbers");
    }
    unsigned long numerator, denominator;
    char rest;
    json_child_path(at, path, "score");
    if (!cJSON_IsString(score) || sscanf(score->valuestring, "%lu/%lu%c", &numerator, &denominator, &rest) != 2 ||
        denominator == 0 || numerator > denominator) {
        return json_bad_format(reader, at, "expected a fraction from 0 to 1 like \"3/4\"");
    }
    snprintf(out->params.source.piece, PLAN_SOURCE_PIECE_MAX, "%s", piece->valuestring);
    out->params.source.start_measure = (int)start_measure->valuedouble;
    out->params.source.start_beat = start_beat->valuedouble;
    out->params.source.end_measure = (int)end_measure->valuedouble;
    out->params.source.end_beat = end_beat->valuedouble;
    out->params.source.score_numerator = numerator;
    out->params.source.score_denominator = denominator;
    return true;
}

static bool valid_pickup(const Plan *plan, const PlanConstraint *constraint) {
    /* The bar it must be shorter than is the meter's, which the melody operator reads; no bar is as long as two wholes. */
    Rational pickup = constraint->params.pickup;
    return pickup.den > 0 && pickup.num > 0 && pickup.num < 2 * pickup.den &&
           plan_meter_pickup_on_grid(first_meter(plan), pickup);
}

static bool write_pickup(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    char length[64];
    snprintf(length, sizeof(length), "%lld/%lld", constraint->params.pickup.num, constraint->params.pickup.den);
    ok = ok && plan_json_add(object, "length", cJSON_CreateString(length));
    return ok;
}

static bool read_pickup(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    const cJSON *length = plan_json_member(reader, item, path, "length", true);
    if (!length) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "length");
    if (!read_fraction_text(length, 1, &out->params.pickup)) {
        return json_bad_format(reader, at, "expected a length in whole notes like \"1/4\"");
    }
    return true;
}

static bool valid_motive_tones(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanMotiveTones *tones = &constraint->params.motive_tones;
    if (tones->chord_count == 0 || tones->chord_count > PLAN_TONES_MAX_CHORDS || tones->label_count == 0 ||
        tones->label_count > PLAN_TONES_MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < tones->chord_count; i++) {
        const PlanToneChord *chord = &tones->chords[i];
        size_t length = strnlen(chord->numeral, PLAN_ROMAN_NUMERAL_MAX);
        if (length == 0 || length >= PLAN_ROMAN_NUMERAL_MAX || chord->onset.den == 0 || chord->onset.num < 0 ||
            chord->length.den == 0 || chord->length.num <= 0) {
            return false;
        }
    }
    for (size_t i = 0; i < tones->label_count; i++) {
        size_t length = strnlen(tones->labels[i], PLAN_TONE_LABEL_MAX);
        if (length == 0 || length >= PLAN_TONE_LABEL_MAX) {
            return false;
        }
    }
    return true;
}

static bool write_motive_tones(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    bool ok = true;
    (void)plan;
    const PlanMotiveTones *tones = &constraint->params.motive_tones;
    cJSON *harmony = cJSON_CreateArray();
    cJSON *labels = cJSON_CreateArray();
    ok = ok && plan_json_add(object, "harmony", harmony) && plan_json_add(object, "tones", labels);
    for (size_t i = 0; ok && i < tones->chord_count; i++) {
        const PlanToneChord *chord = &tones->chords[i];
        char onset[64];
        char length[64];
        snprintf(onset, sizeof(onset), "%lld/%lld", chord->onset.num, chord->onset.den);
        snprintf(length, sizeof(length), "%lld/%lld", chord->length.num, chord->length.den);
        cJSON *item = cJSON_CreateObject();
        ok = plan_json_append(harmony, item) && plan_json_add(item, "numeral", cJSON_CreateString(chord->numeral)) &&
             plan_json_add(item, "onset", cJSON_CreateString(onset)) &&
             plan_json_add(item, "length", cJSON_CreateString(length));
    }
    for (size_t i = 0; ok && i < tones->label_count; i++) {
        ok = plan_json_append(labels, cJSON_CreateString(tones->labels[i]));
    }
    return ok;
}

static bool read_motive_tones(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    return read_motive_tones_members(reader, item, path, &out->params.motive_tones);
}

/* A contrast target (R-710): the idea it contrasts against and, per aspect, the least distance. */
static bool valid_contrast(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanContrast *contrast = &constraint->params.contrast;
    size_t length = strnlen(contrast->against, PLAN_CONTRAST_AGAINST_MAX);
    if (length == 0 || length >= PLAN_CONTRAST_AGAINST_MAX || contrast->aspect_count < 1 ||
        contrast->aspect_count > PLAN_CONTRAST_ASPECTS_MAX) {
        return false;
    }
    for (size_t i = 0; i < contrast->aspect_count; i++) {
        const PlanContrastAspect *aspect = &contrast->aspects[i];
        if (contrast_aspect_name(aspect->aspect) == NULL || aspect->amount.den <= 0 || aspect->amount.num < 0) {
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (contrast->aspects[j].aspect == aspect->aspect) {
                return false;
            }
        }
    }
    return true;
}

static bool write_contrast(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    const PlanContrast *contrast = &constraint->params.contrast;
    cJSON *aspects = cJSON_CreateArray();
    bool ok = plan_json_add(object, "against", cJSON_CreateString(contrast->against)) && plan_json_add(object, "aspects", aspects);
    for (size_t i = 0; ok && i < contrast->aspect_count; i++) {
        char amount[64];
        snprintf(amount, sizeof(amount), "%lld/%lld", contrast->aspects[i].amount.num, contrast->aspects[i].amount.den);
        cJSON *item = cJSON_CreateObject();
        ok = plan_json_append(aspects, item) &&
             plan_json_add(item, "aspect", cJSON_CreateString(contrast_aspect_name(contrast->aspects[i].aspect))) &&
             plan_json_add(item, "amount", cJSON_CreateString(amount));
    }
    return ok;
}

static bool read_contrast(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    PlanContrast *contrast = &out->params.contrast;
    const cJSON *against = plan_json_member(reader, item, path, "against", true);
    const cJSON *aspects = against ? plan_json_member(reader, item, path, "aspects", true) : NULL;
    char at[PLAN_JSON_PATH_MAX];
    if (!aspects) {
        return false;
    }
    json_child_path(at, path, "against");
    if (!cJSON_IsString(against) || against->valuestring[0] == '\0' || strlen(against->valuestring) >= PLAN_CONTRAST_AGAINST_MAX) {
        return json_bad_format(reader, at, "expected the name of an idea of fewer than sixty-four characters");
    }
    snprintf(contrast->against, sizeof(contrast->against), "%s", against->valuestring);
    json_child_path(at, path, "aspects");
    if (!cJSON_IsArray(aspects) || cJSON_GetArraySize(aspects) < 1 || cJSON_GetArraySize(aspects) > PLAN_CONTRAST_ASPECTS_MAX) {
        return json_bad_format(reader, at, "expected one to four aspects");
    }
    const cJSON *aspect;
    cJSON_ArrayForEach(aspect, aspects) {
        char aspect_path[PLAN_JSON_PATH_MAX];
        json_index_path(aspect_path, at, contrast->aspect_count);
        const cJSON *name = plan_json_member(reader, aspect, aspect_path, "aspect", true);
        const cJSON *amount = name ? plan_json_member(reader, aspect, aspect_path, "amount", true) : NULL;
        PlanContrastAspect *slot = &contrast->aspects[contrast->aspect_count];
        char member_path[PLAN_JSON_PATH_MAX];
        if (!amount) {
            return false;
        }
        json_child_path(member_path, aspect_path, "aspect");
        if (!cJSON_IsString(name) || !contrast_aspect_from_name(name->valuestring, &slot->aspect)) {
            return json_bad_format(reader, member_path, "expected rhythm, contour, register or density");
        }
        for (size_t i = 0; i < contrast->aspect_count; i++) {
            if (contrast->aspects[i].aspect == slot->aspect) {
                return json_bad_format(reader, member_path, "an aspect is named twice");
            }
        }
        json_child_path(member_path, aspect_path, "amount");
        if (!read_fraction_text(amount, 0, &slot->amount)) {
            return json_bad_format(reader, member_path, "expected an amount like \"1/2\"");
        }
        contrast->aspect_count++;
    }
    return true;
}

/* ------------------------------------------------------------ the melody constraints (R-746) */

static const char *const EVENT_NAMES[] = {"appoggiatura", "suspension", "passing", "neighbor",
                                          "escape",       "anticipation", "retardation"};
_Static_assert(COUNT_OF(EVENT_NAMES) == PLAN_EVENT_RETARDATION + 1, "a name for every event type");

bool plan_event_type_named(const char *word, PlanEventType *out) {
    for (size_t i = 0; word && i < COUNT_OF(EVENT_NAMES); i++) {
        if (strcmp(word, EVENT_NAMES[i]) == 0) {
            *out = (PlanEventType)i;
            return true;
        }
    }
    return false;
}

static bool valid_offset(Rational offset) {
    return offset.den > 0 && offset.num >= 0 && offset.num <= TIME_LIMIT && offset.den <= TIME_LIMIT;
}

static bool valid_midi(int midi) {
    return midi >= 0 && midi <= 127;
}

static bool write_offset(cJSON *object, Rational offset) {
    char text[64];
    snprintf(text, sizeof(text), "%lld/%lld", offset.num, offset.den);
    return plan_json_add(object, "offset", cJSON_CreateString(text));
}

/* The offset member: a fraction of a whole note "n/d" from 0. */
static bool read_offset(JsonReader *reader, const cJSON *item, const char *path, Rational *out) {
    return read_tones_time(reader, item, path, "offset", 0, out);
}

static bool valid_anchor(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return valid_offset(constraint->params.anchor.offset) && valid_midi(constraint->params.anchor.midi);
}

static bool write_anchor(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return write_offset(object, constraint->params.anchor.offset) &&
           plan_json_add(object, "midi", cJSON_CreateNumber(constraint->params.anchor.midi));
}

static bool read_anchor(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    long midi;
    return read_offset(reader, item, path, &out->params.anchor.offset) &&
           json_read_int_member(reader, item, path, "midi", 0, 127, &midi) && ((out->params.anchor.midi = (int)midi), true);
}

static bool valid_event(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return valid_offset(constraint->params.event.offset) && constraint->params.event.type >= PLAN_EVENT_APPOGGIATURA &&
           constraint->params.event.type <= PLAN_EVENT_RETARDATION;
}

static bool write_event(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return write_offset(object, constraint->params.event.offset) &&
           plan_json_add(object, "type", cJSON_CreateString(EVENT_NAMES[constraint->params.event.type]));
}

static bool read_event(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    int type;
    if (!read_offset(reader, item, path, &out->params.event.offset) ||
        !json_read_enum_member(reader, item, path, "type", EVENT_NAMES, COUNT_OF(EVENT_NAMES), &type)) {
        return false;
    }
    out->params.event.type = (PlanEventType)type;
    return true;
}

static bool valid_climax(const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return valid_offset(constraint->params.climax.offset) && valid_midi(constraint->params.climax.midi);
}

static bool write_climax(cJSON *object, const Plan *plan, const PlanConstraint *constraint) {
    (void)plan;
    return write_offset(object, constraint->params.climax.offset) &&
           plan_json_add(object, "midi", cJSON_CreateNumber(constraint->params.climax.midi));
}

static bool read_climax(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    long midi;
    return read_offset(reader, item, path, &out->params.climax.offset) &&
           json_read_int_member(reader, item, path, "midi", 0, 127, &midi) && ((out->params.climax.midi = (int)midi), true);
}

static bool melody_kind(PlanConstraintKind kind) {
    return kind == PLAN_CONSTRAINT_ANCHOR || kind == PLAN_CONSTRAINT_EVENT || kind == PLAN_CONSTRAINT_CLIMAX;
}

static Rational melody_offset(const PlanConstraint *constraint) {
    switch (constraint->kind) {
    case PLAN_CONSTRAINT_ANCHOR:
        return constraint->params.anchor.offset;
    case PLAN_CONSTRAINT_EVENT:
        return constraint->params.event.offset;
    default:
        return constraint->params.climax.offset;
    }
}

const char *constraint_melody_refusal(const Plan *plan, LaneSpanId span_id, const PlanConstraint *constraint) {
    if (!constraint || !melody_kind(constraint->kind)) {
        return NULL;
    }
    const char *const NOT_MELODY[] = {"anchor: on a melody span only", "event: on a melody span only",
                                      "climax: on a melody span only"};
    const char *const PAST_END[] = {"anchor: the offset is at or past the span's end",
                                    "event: the offset is at or past the span's end",
                                    "climax: the offset is at or past the span's end"};
    const size_t index = (size_t)(constraint->kind - PLAN_CONSTRAINT_ANCHOR);
    if (span_id == PLAN_LANE_SPAN_NONE || plan_lane_span_lane(plan, span_id) != PLAN_LANE_MELODY) {
        return NOT_MELODY[index];
    }
    const Rational offset = melody_offset(constraint);
    if (offset.den <= 0 || rational_cmp(offset, plan_lane_span_span(plan, span_id).length) >= 0) {
        return PAST_END[index];
    }
    /* One anchor and one event to an offset on a span: the constraints the span holds already. */
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, span_id); i++) {
        const PlanConstraint *held = plan_lane_span_constraint_at(plan, span_id, i);
        if (held->kind == constraint->kind && constraint->kind != PLAN_CONSTRAINT_CLIMAX &&
            rational_cmp(melody_offset(held), offset) == 0) {
            return constraint->kind == PLAN_CONSTRAINT_ANCHOR ? "anchor: two anchors at one offset on a span"
                                                              : "event: two events at one offset on a span";
        }
    }
    /* At most one climax in a plan. */
    if (constraint->kind == PLAN_CONSTRAINT_CLIMAX) {
        for (LaneSpanId other = 1; other <= plan_lane_span_count(plan); other++) {
            for (size_t i = 0; i < plan_lane_span_constraint_count(plan, other); i++) {
                if (plan_lane_span_constraint_at(plan, other, i)->kind == PLAN_CONSTRAINT_CLIMAX) {
                    return "climax: at most one in a plan";
                }
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ the table */

static const ConstraintKindRow ROWS[] = {
    {PLAN_CONSTRAINT_LENGTH_BARS, "length_bars", write_length_bars, read_length_bars, valid_length_bars},
    {PLAN_CONSTRAINT_CADENCE, "cadence", write_cadence, read_cadence, valid_cadence},
    {PLAN_CONSTRAINT_MOTIVE_REF, "motive_ref", write_motive_ref, read_motive_ref, valid_motive_ref},
    {PLAN_CONSTRAINT_REGION, "region", write_key, read_key, valid_key},
    {PLAN_CONSTRAINT_EMBELLISH, "embellish", write_embellish, read_embellish, valid_embellish},
    {PLAN_CONSTRAINT_SCHEMA, "schema", write_schema, read_schema, valid_schema},
    {PLAN_CONSTRAINT_HARMONY_SHAPE, "harmony_shape", write_harmony_shape, read_harmony_shape, valid_harmony_shape},
    {PLAN_CONSTRAINT_ACCOMPANIMENT, "accompaniment", write_accompaniment, read_accompaniment, valid_accompaniment},
    {PLAN_CONSTRAINT_VOICING, "voicing", write_voicing, read_voicing, valid_voicing},
    {PLAN_CONSTRAINT_COUNTERPOINT, "counterpoint", write_counterpoint, read_counterpoint, valid_counterpoint},
    {PLAN_CONSTRAINT_INVENT_MOTIVE, "invent_motive", write_invent_motive, read_invent_motive, valid_invent_motive},
    {PLAN_CONSTRAINT_STYLE, "style", write_style, read_style, valid_style},
    {PLAN_CONSTRAINT_SOURCE, "source", write_source, read_source, valid_source},
    {PLAN_CONSTRAINT_PICKUP, "pickup", write_pickup, read_pickup, valid_pickup},
    {PLAN_CONSTRAINT_MOTIVE_TONES, "motive_tones", write_motive_tones, read_motive_tones, valid_motive_tones},
    {PLAN_CONSTRAINT_CONTRAST, "contrast", write_contrast, read_contrast, valid_contrast},
    {PLAN_CONSTRAINT_ANCHOR, "anchor", write_anchor, read_anchor, valid_anchor},
    {PLAN_CONSTRAINT_EVENT, "event", write_event, read_event, valid_event},
    {PLAN_CONSTRAINT_CLIMAX, "climax", write_climax, read_climax, valid_climax},
};

_Static_assert(sizeof(ROWS) / sizeof(ROWS[0]) == PLAN_CONSTRAINT_CLIMAX + 1, "a row for every constraint kind");

const ConstraintKindRow *constraint_kind_row(PlanConstraintKind kind) {
    return (int)kind >= 0 && (size_t)kind < COUNT_OF(ROWS) ? &ROWS[kind] : NULL;
}

const ConstraintKindRow *constraint_kind_named(const char *name) {
    for (size_t i = 0; name && i < COUNT_OF(ROWS); i++) {
        if (strcmp(ROWS[i].name, name) == 0) {
            return &ROWS[i];
        }
    }
    return NULL;
}
