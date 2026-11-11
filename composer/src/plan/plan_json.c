#include <ursatz_composer/plan_json.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <pitch/pitch_arithmetic.h>
#include <standard-harmony/harmonic_function.h>
#include <standard-tonality/tonal_key.h>
#include <ursatz_analyzer/version.h>
#include <ursatz_composer/composer.h>
#include <galant-schemas/galant_prototypes.h>
#include <standard-texture/texture_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/theme_shape.h>

#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "util/plan_util.h"

#define COMPOSER_FRAMEWORK "ursatz-composer"
#define PLAN_FORMAT_NAME "ursatz-composer-plan"
#define COMPOSER_KEY_METHOD "composer_plan"

static const char *const NODE_KINDS[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};
static const char *const STATE_NAMES[] = {"constrained", "given", "realized"};
static const char *const MODE_NAMES[] = {"major", "minor"};
/* A version 1 file wrote a chord with no function as "unknown". */
#define LEGACY_FUNCTION_NONE "unknown"
static const char *const CONSTRAINT_KINDS[] = {"length_bars", "cadence", "motive_ref", "key", "archetype", "embellish", "schema",
                                                  "material", "harmony_shape", "theme_pairing", "accompaniment", "voicing", "counterpoint",
                                                  "return", "layers", "section_letter", "invent_motive", "style", "source", "pickup", "piece_part", "free_phrase", "hold", "seam", "motive_tones", "pedal", "construction"};
static const char *const EXTENSION_KIND_NAMES[] = {"repeat", "stretch", "addition", "evaded"};
static const char *const CONSTRUCTION_REASON_NAMES[] = {"given", "motive", "length"};
static const char *const ARCHETYPE_NAMES[] = {"sentence", "period", "hybrid"};
/* The parts of a game loop, in PiecePart order (piece.h). */
static const char *const PIECE_PART_NAMES[] = {"intro", "body", "turnaround", "ending"};
static const char *const SHAPE_KIND_NAMES[] = {"prolongation", "phrase_model", "cadential"};
static const char *const ANCHOR_NAMES[] = {"any", "tonic", "dominant", "pivot"};
static const char *const ACCOMPANIMENT_NAMES[] = {"any", "block", "arpeggiated", "alberti", "oom-pah", "motive"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* ------------------------------------------------------------------ writing */

bool plan_json_add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool plan_json_append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

cJSON *json_number(long value) {
    return cJSON_CreateNumber((double)value);
}

cJSON *rational_json(long long numerator, long long denominator) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(plan_json_add(object, "num", json_number((long)numerator)) &&
                    plan_json_add(object, "den", json_number((long)denominator)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *span_json(PlanSpan span) {
    Rational end;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return NULL;
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(plan_json_add(object, "start", rational_json(span.start.num, span.start.den)) && plan_json_add(object, "end", rational_json(end.num, end.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A motive ref's relation step as its text (motive_relation_step_text); a ref with no step has no member. */
#define STEP_TEXT_MAX 160

/* A key's "tonic", spelled as the library spells it (F#, Bb), and its "mode". */
static bool key_members_json(cJSON *object, const AnalysisKey *key) {
    char tonic[8];
    return pitch_spelling_name(key->tonic_letter, key->tonic_accidental, tonic, sizeof(tonic)) &&
           plan_json_add(object, "tonic", cJSON_CreateString(tonic)) &&
           plan_json_add(object, "mode", cJSON_CreateString(MODE_NAMES[key->mode]));
}

static cJSON *constraint_json(const PiecePlan *plan, const PlanConstraint *constraint) {
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        return NULL;
    }
    bool ok = plan_json_add(object, "kind", cJSON_CreateString(CONSTRAINT_KINDS[constraint->kind]));
    switch (constraint->kind) {
    case PLAN_CONSTRAINT_LENGTH_BARS:
        ok = ok && plan_json_add(object, "bars", json_number(constraint->params.bars));
        break;
    case PLAN_CONSTRAINT_CADENCE: {
        size_t cadence_index = 0;
        ok = ok && plan_cadence_index(constraint->params.cadence, &cadence_index) &&
             plan_json_add(object, "type", cJSON_CreateString(plan_cadence_file_names[cadence_index]));
        break;
    }
    case PLAN_CONSTRAINT_MOTIVE_REF:
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
        break;
    case PLAN_CONSTRAINT_KEY:
        ok = ok && key_members_json(object, &constraint->params.key);
        break;
    case PLAN_CONSTRAINT_ARCHETYPE:
        ok = ok && plan_json_add(object, "type", cJSON_CreateString(ARCHETYPE_NAMES[constraint->params.archetype]));
        break;
    case PLAN_CONSTRAINT_EMBELLISH:
        break;
    case PLAN_CONSTRAINT_SCHEMA:
        ok = ok && plan_json_add(object, "schema", cJSON_CreateString(galant_schema_name(
                                                    (GalantSchemaId)constraint->params.schema.schema))) &&
             plan_json_add(object, "stage", json_number(constraint->params.schema.stage));
        break;
    case PLAN_CONSTRAINT_MATERIAL:
        break;
    case PLAN_CONSTRAINT_HARMONY_SHAPE: {
        const PlanHarmonyShape *shape = &constraint->params.harmony_shape;
        ok = ok && plan_json_add(object, "shape", cJSON_CreateString(SHAPE_KIND_NAMES[shape->kind])) &&
             plan_json_add(object, "starts_on", cJSON_CreateString(ANCHOR_NAMES[shape->starts_on])) &&
             plan_json_add(object, "ends_on", cJSON_CreateString(ANCHOR_NAMES[shape->ends_on])) &&
             plan_json_add(object, "max_cells_per_chord", json_number(shape->max_cells_per_chord)) &&
             plan_json_add(object, "split_at_bars", cJSON_CreateBool(shape->split_at_bars));
        if (ok && shape->pedal) {
            ok = plan_json_add(object, "pedal", cJSON_CreateBool(true));
        }
        break;
    }
    case PLAN_CONSTRAINT_THEME_PAIRING:
        ok = ok && plan_json_add(object, "beginning",
                            cJSON_CreateString(theme_shape_beginning(constraint->params.theme_pairing.beginning)->plan_name)) &&
             plan_json_add(object, "ending",
                           cJSON_CreateString(theme_shape_ending(constraint->params.theme_pairing.ending)->plan_name));
        break;
    case PLAN_CONSTRAINT_ACCOMPANIMENT: {
        const PlanAccompaniment *accompaniment = &constraint->params.accompaniment;
        ok = ok && plan_json_add(object, "pattern", cJSON_CreateString(ACCOMPANIMENT_NAMES[accompaniment->pattern]));
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
        break;
    }
    case PLAN_CONSTRAINT_VOICING:
        ok = ok && plan_json_add(object, "write_soprano", cJSON_CreateBool(constraint->params.voicing.write_soprano));
        break;
    case PLAN_CONSTRAINT_COUNTERPOINT:
        ok = ok && plan_json_add(object, "species", json_number(constraint->params.counterpoint.species)) &&
             plan_json_add(object, "above", cJSON_CreateBool(constraint->params.counterpoint.above));
        break;
    case PLAN_CONSTRAINT_RETURN:
        ok = ok && plan_json_add(object, "section", json_number((long)plan_node_rank(plan, constraint->params.section_return.section))) &&
             plan_json_add(object, "variation",
                      cJSON_CreateString(plan_return_variation_name(constraint->params.section_return.variation)));
        break;
    case PLAN_CONSTRAINT_LAYERS: {
        cJSON *lanes = cJSON_CreateArray();
        ok = ok && plan_json_add(object, "lanes", lanes);
        for (size_t lane = 0; ok && lane < PLAN_LANE_NAME_COUNT; lane++) {
            if (constraint->params.layers.lanes & PLAN_LAYER_BIT(lane)) {
                ok = plan_json_append(lanes, cJSON_CreateString(plan_lane_names[lane]));
            }
        }
        break;
    }
    case PLAN_CONSTRAINT_SECTION_LETTER: {
        char letter[PLAN_SECTION_LETTER_MAX];
        snprintf(letter, sizeof(letter), "%s", constraint->params.letter);
        ok = ok && plan_json_add(object, "letter", cJSON_CreateString(letter));
        break;
    }
    case PLAN_CONSTRAINT_INVENT_MOTIVE:
        break;
    case PLAN_CONSTRAINT_STYLE: {
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
        break;
    }
    case PLAN_CONSTRAINT_SOURCE: {
        const PlanSource *source = &constraint->params.source;
        char score[64];
        snprintf(score, sizeof(score), "%lu/%lu", source->score_numerator, source->score_denominator);
        ok = ok && plan_json_add(object, "piece", cJSON_CreateString(source->piece)) &&
             plan_json_add(object, "start_measure", cJSON_CreateNumber(source->start_measure)) &&
             plan_json_add(object, "start_beat", cJSON_CreateNumber(source->start_beat)) &&
             plan_json_add(object, "end_measure", cJSON_CreateNumber(source->end_measure)) &&
             plan_json_add(object, "end_beat", cJSON_CreateNumber(source->end_beat)) &&
             plan_json_add(object, "score", cJSON_CreateString(score));
        break;
    }
    case PLAN_CONSTRAINT_PICKUP: {
        char length[64];
        snprintf(length, sizeof(length), "%lld/%lld", constraint->params.pickup.num, constraint->params.pickup.den);
        ok = ok && plan_json_add(object, "length", cJSON_CreateString(length));
        break;
    }
    case PLAN_CONSTRAINT_PEDAL:
        ok = ok && plan_json_add(object, "degree", json_number(constraint->params.pedal.degree));
        break;
    case PLAN_CONSTRAINT_CONSTRUCTION: {
        const PlanConstruction *construction = &constraint->params.construction;
        cJSON *ideas = cJSON_CreateObject();
        cJSON *extensions = cJSON_CreateArray();
        ok = ok && plan_json_add(object, "ideas", ideas) && plan_json_add(object, "extensions", extensions);
        for (int idea = 0; ok && idea < PLAN_CONSTRUCTION_IDEAS; idea++) {
            if (construction->idea_bars[idea] != 0) {
                ok = plan_json_add(ideas, theme_shape_idea_name((GrowIdea)idea),
                                   json_number(construction->idea_bars[idea]));
            }
        }
        for (size_t i = 0; ok && i < construction->extension_count; i++) {
            const PlanExtension *extension = &construction->extensions[i];
            cJSON *item = cJSON_CreateObject();
            ok = plan_json_append(extensions, item) &&
                 plan_json_add(item, "kind", cJSON_CreateString(EXTENSION_KIND_NAMES[extension->kind])) &&
                 plan_json_add(item, "idea", cJSON_CreateString(theme_shape_idea_name((GrowIdea)extension->idea))) &&
                 plan_json_add(item, "bars", json_number(extension->bars)) &&
                 plan_json_add(item, "varied", cJSON_CreateBool(extension->varied));
        }
        ok = ok && plan_json_add(object, "reason", cJSON_CreateString(CONSTRUCTION_REASON_NAMES[construction->reason]));
        break;
    }
    case PLAN_CONSTRAINT_PIECE_PART:
        ok = ok && plan_json_add(object, "part", cJSON_CreateString(PIECE_PART_NAMES[constraint->params.piece_part.part])) &&
             plan_json_add(object, "id", cJSON_CreateString(constraint->params.piece_part.id));
        break;
    case PLAN_CONSTRAINT_MOTIVE_TONES: {
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
        break;
    }
    case PLAN_CONSTRAINT_FREE_PHRASE:
    case PLAN_CONSTRAINT_HOLD:
    case PLAN_CONSTRAINT_SEAM:
        break;
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *node_constraints_json(const PiecePlan *plan, NodeId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_node_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_node_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *span_constraints_json(const PiecePlan *plan, LaneSpanId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_lane_span_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_lane_span_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *node_json(const PiecePlan *plan, NodeId id) {
    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    cJSON *children = cJSON_CreateArray();
    const char *name = plan_node_name(plan, id);
    bool ok = object && composer && children;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_node_rank(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(STATE_NAMES[plan_node_state(plan, id)])) &&
         plan_json_add(composer, "constraints", node_constraints_json(plan, id));
    ok = ok && plan_json_add(object, "kind", cJSON_CreateString(NODE_KINDS[plan_node_kind(plan, id)])) &&
         plan_json_add(object, "label", name ? cJSON_CreateString(name) : cJSON_CreateNull()) &&
         plan_json_add(object, "span", span_json(plan_node_span(plan, id)));
    for (size_t i = 0; ok && i < plan_node_child_count(plan, id); i++) {
        ok = plan_json_append(children, node_json(plan, plan_node_child_at(plan, id, i)));
    }
    ok = ok && plan_json_add(object, "children", children);
    if (ok) {
        children = NULL;
        ok = plan_json_add(object, "composer", composer);
        composer = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(composer);
        cJSON_Delete(children);
        return NULL;
    }
    return object;
}

static void plan_key_label(const AnalysisKey *key, char *out, size_t size) {
    char tonic[8] = "?";
    pitch_spelling_name(key->tonic_letter, key->tonic_accidental, tonic, sizeof(tonic));
    snprintf(out, size, "%s %s", tonic, MODE_NAMES[key->mode]);
}

static bool lane_label(const PlanLanePayload *payload, PlanLaneKind lane, char *out, size_t size) {
    switch (lane) {
    case PLAN_LANE_METER:
        snprintf(out, size, "%d/%d", payload->meter.beats_per_bar, payload->meter.beat_unit);
        return true;
    case PLAN_LANE_KEY:
        plan_key_label(&payload->key, out, size);
        return true;
    case PLAN_LANE_HARMONY:
        snprintf(out, size, "%s", payload->harmony.roman_numeral);
        return true;
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        snprintf(out, size, "%s", plan_lane_names[lane]);
        return true;
    }
    return false;
}

static cJSON *notes_json(const PlanNotes *notes) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < notes->note_count; i++) {
        const PlanNote *note = &notes->notes[i];
        cJSON *object = cJSON_CreateObject();
        char spelling[8];
        bool spelt = !note->spelled || pitch_spelling_name(note->letter, note->accidental, spelling, sizeof(spelling));
        if (!(object && spelt && plan_json_add(object, "midi_pitch", json_number(note->midi)) &&
              (!note->spelled || plan_json_add(object, "spelling", cJSON_CreateString(spelling))) &&
              plan_json_add(object, "offset", rational_json(note->offset.num, note->offset.den)) &&
              plan_json_add(object, "duration", rational_json(note->duration.num, note->duration.den)) && plan_json_append(array, object))) {
            if (object && !cJSON_GetObjectItem(object, "duration")) {
                cJSON_Delete(object);
            }
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *payload_json(PlanLaneKind lane, const PlanLanePayload *payload) {
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        return NULL;
    }
    bool ok = true;
    switch (lane) {
    case PLAN_LANE_METER:
        ok = plan_json_add(object, "beats_per_bar", json_number(payload->meter.beats_per_bar)) &&
             plan_json_add(object, "beat_unit", json_number(payload->meter.beat_unit)) &&
             plan_json_add(object, "bars_per_hyperbar", json_number(payload->meter.bars_per_hyperbar));
        break;
    case PLAN_LANE_KEY:
        ok = key_members_json(object, &payload->key);
        break;
    case PLAN_LANE_HARMONY:
        ok = plan_json_add(object, "roman_numeral", cJSON_CreateString(payload->harmony.roman_numeral));
        if (ok && payload->harmony.has_function) {
            const char *name = harmonic_function_name(payload->harmony.function);
            ok = name && plan_json_add(object, "function", cJSON_CreateString(name));
        }
        break;
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        ok = plan_json_add(object, "notes", notes_json(&payload->notes));
        break;
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The label of the key span that covers the start of span, or "no match". */
static void enclosing_plan_key_label(const PiecePlan *plan, PlanSpan span, char *out, size_t size) {
    LaneSpanId found = PLAN_LANE_SPAN_NONE;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, &found, 1) > 0) {
        plan_key_label(&plan_lane_span_payload(plan, found)->key, out, size);
    } else {
        snprintf(out, size, "no match");
    }
}

static cJSON *lane_entry_json(const PiecePlan *plan, LaneSpanId id) {
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    PlanSpan span = plan_lane_span_span(plan, id);
    const PlanLanePayload *payload = plan_lane_span_payload(plan, id);
    char label[64];
    char context_key[64];
    lane_label(payload, lane, label, sizeof(label));

    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    bool ok = object && composer;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_lane_span_rank(plan, id))) &&
         plan_json_add(composer, "owner", json_number((long)plan_node_rank(plan, plan_lane_span_owner(plan, id)))) &&
         plan_json_add(composer, "lane", cJSON_CreateString(plan_lane_names[lane])) &&
         plan_json_add(composer, "voice", json_number(plan_lane_span_voice(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(STATE_NAMES[plan_lane_span_state(plan, id)])) &&
         plan_json_add(composer, "constraints", span_constraints_json(plan, id)) &&
         plan_json_add(composer, "payload", payload_json(lane, payload));
    ok = ok && plan_json_add(object, "framework", cJSON_CreateString(COMPOSER_FRAMEWORK)) &&
         plan_json_add(object, "span", span_json(span)) && plan_json_add(object, "note_ids", cJSON_CreateArray()) &&
         plan_json_add(object, "label", cJSON_CreateString(label)) && plan_json_add(object, "uncertainty", cJSON_CreateNull());
    if (ok && lane == PLAN_LANE_KEY) {
        ok = plan_json_add(object, "method", cJSON_CreateString(COMPOSER_KEY_METHOD));
    }
    if (ok && lane == PLAN_LANE_HARMONY) {
        enclosing_plan_key_label(plan, span, context_key, sizeof(context_key));
        ok = plan_json_add(object, "key_label", cJSON_CreateString(context_key));
    }
    if (ok) {
        ok = plan_json_add(object, "composer", composer);
        composer = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(composer);
        return NULL;
    }
    return object;
}

/* The claims for every lane span whose lane is in mask, in id order. */
static cJSON *lane_array_json(const PiecePlan *plan, unsigned mask) {
    cJSON *array = cJSON_CreateArray();
    for (LaneSpanId id = 1; array && id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && (mask & (1u << plan_lane_span_lane(plan, id))) && !plan_json_append(array, lane_entry_json(plan, id))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *meter_json(const PiecePlan *plan) {
    cJSON *spans = lane_array_json(plan, 1u << PLAN_LANE_METER);
    if (!spans) {
        return NULL;
    }
    if (cJSON_GetArraySize(spans) == 0) {
        cJSON_Delete(spans);
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !plan_json_add(object, "spans", spans)) {
        if (!object) {
            cJSON_Delete(spans);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The Composer that wrote the file, the Ursatz commit it was built on and, for a plan loaded from an analysis, the Analyzer. */
static cJSON *metadata_json(const PiecePlan *plan) {
    cJSON *object = cJSON_CreateObject();
    const char *analyzer = plan_analyzer_version(plan);
    if (object && !(plan_json_add(object, "composer_version", cJSON_CreateString(composer_version())) &&
                    plan_json_add(object, "ursatz_commit", cJSON_CreateString(ursatz_analyzer_ursatz_commit())) &&
                    (analyzer[0] == '\0' || plan_json_add(object, "analyzer_version", cJSON_CreateString(analyzer))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *document_json(const PiecePlan *plan) {
    cJSON *document = cJSON_CreateObject();
    if (!document) {
        return NULL;
    }
    bool ok = plan_json_add(document, "format", cJSON_CreateString(PLAN_FORMAT_NAME)) &&
              plan_json_add(document, "format_version", json_number(URSATZ_COMPOSER_PLAN_FORMAT_VERSION)) &&
              plan_json_add(document, "metadata", metadata_json(plan)) && plan_json_add(document, "meter", meter_json(plan)) &&
              plan_json_add(document, "keys", lane_array_json(plan, 1u << PLAN_LANE_KEY)) &&
              plan_json_add(document, "chords", lane_array_json(plan, 1u << PLAN_LANE_HARMONY)) &&
              plan_json_add(document, "form", plan_node_count(plan) > 0 ? node_json(plan, plan_root(plan)) : cJSON_CreateNull()) &&
              plan_json_add(document, "melody", lane_array_json(plan, 1u << PLAN_LANE_MELODY)) &&
              plan_json_add(document, "texture", lane_array_json(plan, (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)));
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

void json_set_error(PlanJsonError *error, long offset, const char *path, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    snprintf(error->path, sizeof(error->path), "%s", path);
    error->offset = offset;
}

/* cJSON prints with tabs; the project's JSON uses a 2-space indent and "key": value. */
char *json_indent_with_spaces(const char *printed) {
    size_t tabs = 0;
    size_t length = strlen(printed);
    for (size_t i = 0; i < length; i++) {
        tabs += printed[i] == '\t';
    }
    char *out = malloc(length + tabs + 2);
    if (!out) {
        return NULL;
    }
    char *cursor = out;
    for (size_t i = 0; i < length; i++) {
        if (printed[i] == '\t' && i > 0 && printed[i - 1] == ':') {
            *cursor++ = ' ';
        } else if (printed[i] == '\t') {
            *cursor++ = ' ';
            *cursor++ = ' ';
        } else {
            *cursor++ = printed[i];
        }
    }
    *cursor++ = '\n';
    *cursor = '\0';
    return out;
}

char *plan_to_json(const PiecePlan *plan, PlanJsonError *error) {
    if (!plan) {
        json_set_error(error, -1, "", "plan is NULL");
        return NULL;
    }
    cJSON *document = document_json(plan);
    char *printed = document ? cJSON_Print(document) : NULL;
    cJSON_Delete(document);
    char *text = printed ? json_indent_with_spaces(printed) : NULL;
    cJSON_free(printed);
    if (!text) {
        json_set_error(error, -1, "", "out of memory");
    }
    return text;
}

PlanResult plan_write_json_file(const PiecePlan *plan, const char *path, PlanJsonError *error) {
    if (!path) {
        json_set_error(error, -1, "", "path is NULL");
        return PLAN_ERR_NULL;
    }
    char *text = plan_to_json(plan, error);
    if (!text) {
        return plan ? PLAN_ERR_ALLOC : PLAN_ERR_NULL;
    }
    FILE *file = fopen(path, "wb");
    size_t length = strlen(text);
    bool wrote = file && fwrite(text, 1, length, file) == length;
    bool closed = file && fclose(file) == 0;
    free(text);
    if (!wrote || !closed) {
        json_set_error(error, -1, "", "could not write '%s'", path);
        return PLAN_ERR_IO;
    }
    return PLAN_OK;
}

/* ------------------------------------------------------------------ reading */

bool json_fail(JsonReader *reader, PlanResult result, const char *path, const char *format, ...) {
    if (reader->result == PLAN_OK) {
        reader->result = result;
        if (reader->error) {
            va_list args;
            va_start(args, format);
            vsnprintf(reader->error->message, sizeof(reader->error->message), format, args);
            va_end(args);
            snprintf(reader->error->path, sizeof(reader->error->path), "%s", path);
            reader->error->offset = -1;
        }
    }
    return false;
}

bool json_bad_format(JsonReader *reader, const char *path, const char *message) {
    return json_fail(reader, PLAN_ERR_FORMAT, path, "%s", message);
}

void json_child_path(char *out, const char *parent, const char *key) {
    if (parent[0] == '\0') {
        snprintf(out, PLAN_JSON_PATH_MAX, "%s", key);
    } else {
        snprintf(out, PLAN_JSON_PATH_MAX, "%s.%s", parent, key);
    }
}

void json_index_path(char *out, const char *parent, size_t index) {
    snprintf(out, PLAN_JSON_PATH_MAX, "%s[%zu]", parent, index);
}

/* The plan_json_member of object named key, or NULL after reporting it missing (when required) or of the wrong type. */
const cJSON *plan_json_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item && required) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, key);
        json_bad_format(reader, at, "missing required field");
    }
    return item;
}

bool json_read_long(JsonReader *reader, const cJSON *item, const char *path, long min, long max, long *out) {
    if (!cJSON_IsNumber(item)) {
        return json_bad_format(reader, path, "expected a number");
    }
    double value = item->valuedouble;
    if (value < (double)min || value > (double)max || (double)(long)value != value) {
        return json_fail(reader, PLAN_ERR_FORMAT, path, "expected an integer from %ld to %ld", min, max);
    }
    *out = (long)value;
    return true;
}

bool json_read_int_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                            long max, long *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return json_read_long(reader, item, at, min, max, out);
}

static bool read_enum(JsonReader *reader, const cJSON *item, const char *path, const char *const *names, size_t count,
                      int *out) {
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, path, "expected a string");
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(item->valuestring, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return json_fail(reader, PLAN_ERR_FORMAT, path, "unknown value '%.40s'", item->valuestring);
}

static bool read_enum_member(JsonReader *reader, const cJSON *object, const char *path, const char *key,
                             const char *const *names, size_t count, int *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return read_enum(reader, item, at, names, count, out);
}

/* A return constraint's variation, by the name the one table holds. */
static bool read_return_variation(JsonReader *reader, const cJSON *object, const char *path,
                                  PlanReturnVariation *out) {
    const cJSON *item = plan_json_member(reader, object, path, "variation", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "variation");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!plan_return_variation_from_name(item->valuestring, out)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    return true;
}

/* A theme pairing's beginning or ending by its plan name (the theme shape table's). */
static bool read_theme_half(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool beginning,
                            int *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    const ThemeShapeHalf *half = theme_shape_half_find(beginning, item->valuestring);
    if (!half) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    *out = half->value;
    return true;
}

/* An idea by the name the theme shape table gives it; `at` is its path. */
static bool read_idea_name(JsonReader *reader, const cJSON *item, const char *at, int *out) {
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected an idea name");
    }
    const ThemeShapeIdea *row = theme_shape_idea_find(item->valuestring);
    if (!row) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown idea '%.40s'", item->valuestring);
    }
    *out = (int)row->idea;
    return true;
}

/* A construction constraint's ideas, extensions and reason. */
static bool read_construction(JsonReader *reader, const cJSON *item, const char *path, PlanConstruction *out) {
    char at[PLAN_JSON_PATH_MAX];
    int value;
    long number;
    const cJSON *ideas = plan_json_member(reader, item, path, "ideas", true);
    if (!ideas) {
        return false;
    }
    json_child_path(at, path, "ideas");
    if (!cJSON_IsObject(ideas)) {
        return json_bad_format(reader, at, "expected an object of idea names and bars");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        char member_at[PLAN_JSON_PATH_MAX];
        const ThemeShapeIdea *row = theme_shape_idea_find(member->string);
        json_child_path(member_at, at, member->string);
        if (!row) {
            return json_fail(reader, PLAN_ERR_FORMAT, member_at, "unknown idea '%.40s'", member->string);
        }
        if (!json_read_long(reader, member, member_at, 0, 16, &number)) {
            return false;
        }
        out->idea_bars[row->idea] = (unsigned char)number;
    }
    const cJSON *extensions = plan_json_member(reader, item, path, "extensions", true);
    if (!extensions) {
        return false;
    }
    json_child_path(at, path, "extensions");
    if (!cJSON_IsArray(extensions)) {
        return json_bad_format(reader, at, "expected an array");
    }
    if ((size_t)cJSON_GetArraySize(extensions) > PLAN_CONSTRUCTION_EXTENSIONS) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "at most %d extensions", PLAN_CONSTRUCTION_EXTENSIONS);
    }
    size_t index = 0;
    for (const cJSON *extension = extensions->child; extension; extension = extension->next, index++) {
        char extension_at[PLAN_JSON_PATH_MAX];
        json_index_path(extension_at, at, index);
        if (!cJSON_IsObject(extension)) {
            return json_bad_format(reader, extension_at, "expected an object");
        }
        PlanExtension *target = &out->extensions[index];
        const cJSON *idea;
        const cJSON *varied;
        if (!read_enum_member(reader, extension, extension_at, "kind", EXTENSION_KIND_NAMES, COUNT_OF(EXTENSION_KIND_NAMES),
                              &value)) {
            return false;
        }
        target->kind = (PlanExtensionKind)value;
        if (!(idea = plan_json_member(reader, extension, extension_at, "idea", true))) {
            return false;
        }
        char idea_at[PLAN_JSON_PATH_MAX];
        json_child_path(idea_at, extension_at, "idea");
        if (!read_idea_name(reader, idea, idea_at, &value)) {
            return false;
        }
        target->idea = (unsigned char)value;
        if (!json_read_int_member(reader, extension, extension_at, "bars", 0, 4, &number)) {
            return false;
        }
        target->bars = (unsigned char)number;
        if (!(varied = plan_json_member(reader, extension, extension_at, "varied", true))) {
            return false;
        }
        if (!cJSON_IsBool(varied)) {
            char varied_at[PLAN_JSON_PATH_MAX];
            json_child_path(varied_at, extension_at, "varied");
            return json_bad_format(reader, varied_at, "expected true or false");
        }
        target->varied = cJSON_IsTrue(varied);
        out->extension_count = index + 1;
    }
    if (!read_enum_member(reader, item, path, "reason", CONSTRUCTION_REASON_NAMES, COUNT_OF(CONSTRUCTION_REASON_NAMES),
                          &value)) {
        return false;
    }
    out->reason = (PlanConstructionReason)value;
    return true;
}

#define TIME_LIMIT 1000000000L

static bool json_read_rational(JsonReader *reader, const cJSON *item, const char *path, Rational *out) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object with num and den");
    }
    long num;
    long den;
    if (!json_read_int_member(reader, item, path, "num", -TIME_LIMIT, TIME_LIMIT, &num) ||
        !json_read_int_member(reader, item, path, "den", 1, TIME_LIMIT, &den)) {
        return false;
    }
    return rational_make(num, (long long)den, out) == RATIONAL_OK;
}

static bool read_rational_member(JsonReader *reader, const cJSON *object, const char *path, const char *key,
                                 Rational *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return json_read_rational(reader, item, at, out);
}

/* Reads {"start", "end"} into a start and a length. */
bool json_read_span(JsonReader *reader, const cJSON *object, const char *path, PlanSpan *out) {
    const cJSON *item = plan_json_member(reader, object, path, "span", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "span");
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, at, "expected an object with start and end");
    }
    Rational end;
    if (!read_rational_member(reader, item, at, "start", &out->start) ||
        !read_rational_member(reader, item, at, "end", &end)) {
        return false;
    }
    Rational negative_start = {-out->start.num, out->start.den};
    if (rational_add(end, negative_start, &out->length) != RATIONAL_OK || out->length.num <= 0) {
        return json_bad_format(reader, at, "end must be after start");
    }
    return true;
}

/* An optional boolean member; absent is false. */
static bool read_optional_bool(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    *out = false;
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, key);
        return json_bad_format(reader, at, "expected true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

/* An optional integer member; absent is 0. */
static bool read_optional_int(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
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
        if (!read_enum(reader, position_item, at, LEGACY_POSITION_NAMES, COUNT_OF(LEGACY_POSITION_NAMES), &position)) {
            return false;
        }
    }
    bool has_axis = cJSON_GetObjectItemCaseSensitive(item, "axis") != NULL;
    if (!read_optional_int(reader, item, path, "semitones", -127, 127, &semitones) ||
        !read_optional_int(reader, item, path, "axis", 0, 127, &axis) ||
        !read_optional_bool(reader, item, path, "tonal", &tonal) ||
        !read_optional_bool(reader, item, path, "keep_contour", &keep_contour) ||
        !read_optional_bool(reader, item, path, "keep_rhythm", &keep_rhythm) ||
        !read_optional_int(reader, item, path, "steps", -127, 127, &steps) ||
        !read_optional_int(reader, item, path, "interval", 0, 127, &interval) ||
        !read_optional_int(reader, item, path, "amount", -127, 127, &amount) ||
        !read_optional_int(reader, item, path, "length", 0, 127, &length) ||
        !read_optional_int(reader, item, path, "drop", 0, 127, &drop) ||
        !read_optional_int(reader, item, path, "ratio_num", 0, 64, &ratio_num) ||
        !read_optional_int(reader, item, path, "ratio_den", 0, 64, &ratio_den)) {
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
static bool read_motive_ref(JsonReader *reader, const cJSON *item, const char *path, PlanMotiveRef *ref) {
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

/* A time like "1/4" in whole notes: the numerator not below `least`. */
static bool read_tones_time(JsonReader *reader, const cJSON *item, const char *path, const char *name, long least,
                            Rational *out) {
    const cJSON *text = plan_json_member(reader, item, path, name, true);
    if (!text) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, name);
    long numerator;
    unsigned long denominator;
    char rest;
    if (!cJSON_IsString(text) || sscanf(text->valuestring, "%ld/%lu%c", &numerator, &denominator, &rest) != 2 ||
        denominator == 0 || numerator < least || numerator > TIME_LIMIT || denominator > (unsigned long)TIME_LIMIT ||
        rational_make(numerator, denominator, out) != RATIONAL_OK) {
        return json_bad_format(reader, at, "expected a time in whole notes like \"1/4\"");
    }
    return true;
}

static bool read_motive_tones(JsonReader *reader, const cJSON *item, const char *path, PlanMotiveTones *out) {
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

/*
 * Version 1 files (R-260): a key is "tonic_pitch_class" and "mode", with no spelling. The tonic is spelled as the library
 * spells the key on that pitch class (standard_tonality_key_tonic_of_pitch_class: the fewest accidentals in its
 * signature, sharps on a tie), which is how every plan of version 1 was written.
 */
static bool read_key_v1(JsonReader *reader, const cJSON *object, const char *path, KeyStructureMode mode,
                        AnalysisKey *out) {
    long pitch_class;
    PitchLetter letter;
    int accidental;
    if (!json_read_int_member(reader, object, path, "tonic_pitch_class", 0, 11, &pitch_class)) {
        return false;
    }
    if (!standard_tonality_key_tonic_of_pitch_class((int)pitch_class, mode == KEY_STRUCTURE_MODE_MINOR, &letter,
                                                    &accidental)) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "tonic_pitch_class");
        return json_bad_format(reader, at, "no key on this pitch class");
    }
    AnalysisKey key = {NULL, letter, accidental, mode};
    *out = key;
    return true;
}

/* A key's members: "tonic" (a spelled name) in version 2; see read_key_v1 for version 1. */
static bool read_key_members(JsonReader *reader, const cJSON *object, const char *path, AnalysisKey *out) {
    int mode;
    if (!read_enum_member(reader, object, path, "mode", MODE_NAMES, COUNT_OF(MODE_NAMES), &mode)) {
        return false;
    }
    if (reader->version == 1) {
        return read_key_v1(reader, object, path, (KeyStructureMode)mode, out);
    }
    const cJSON *tonic = plan_json_member(reader, object, path, "tonic", true);
    if (!tonic) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "tonic");
    PitchLetter letter;
    int accidental;
    if (!cJSON_IsString(tonic) || !pitch_spelling_parse_name(tonic->valuestring, &letter, &accidental, NULL)) {
        return json_bad_format(reader, at, "expected a tonic like \"C\", \"F#\" or \"Bb\"");
    }
    AnalysisKey key = {NULL, letter, accidental, (KeyStructureMode)mode};
    *out = key;
    return true;
}

/* A chord's "function": a harmonic_function_name, absent for none. Version 1 always wrote it, "unknown" for none. */
static bool read_function(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony) {
    harmony->has_function = false;
    harmony->function = HARMONIC_FUNCTION_TONIC;
    const cJSON *item = plan_json_member(reader, object, path, "function", reader->version == 1);
    if (!item) {
        return reader->result == PLAN_OK;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "function");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (reader->version == 1 && strcmp(item->valuestring, LEGACY_FUNCTION_NONE) == 0) {
        return true;
    }
    for (int function = 0; function < (int)HARMONIC_FUNCTION_COUNT; function++) {
        if (strcmp(item->valuestring, harmonic_function_name((HarmonicFunction)function)) == 0) {
            harmony->has_function = true;
            harmony->function = (HarmonicFunction)function;
            return true;
        }
    }
    return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
}

static bool read_constraint(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object");
    }
    int kind;
    int value;
    long number_value;
    memset(out, 0, sizeof(*out));
    if (!read_enum_member(reader, item, path, "kind", CONSTRAINT_KINDS, COUNT_OF(CONSTRAINT_KINDS), &kind)) {
        return false;
    }
    out->kind = (PlanConstraintKind)kind;
    switch (out->kind) {
    case PLAN_CONSTRAINT_LENGTH_BARS:
        if (!json_read_int_member(reader, item, path, "bars", 1, TIME_LIMIT, &number_value)) {
            return false;
        }
        out->params.bars = (int)number_value;
        return true;
    case PLAN_CONSTRAINT_CADENCE:
        if (!read_enum_member(reader, item, path, "type", plan_cadence_file_names, PLAN_CADENCE_COUNT, &value)) {
            return false;
        }
        out->params.cadence = plan_cadence_types[value];
        return true;
    case PLAN_CONSTRAINT_MOTIVE_REF:
        return read_motive_ref(reader, item, path, &out->params.motive);
    case PLAN_CONSTRAINT_KEY:
        return read_key_members(reader, item, path, &out->params.key);
    case PLAN_CONSTRAINT_ARCHETYPE:
        if (!read_enum_member(reader, item, path, "type", ARCHETYPE_NAMES, COUNT_OF(ARCHETYPE_NAMES), &value)) {
            return false;
        }
        out->params.archetype = (PlanArchetype)value;
        return true;
    case PLAN_CONSTRAINT_EMBELLISH:
        return true;
    case PLAN_CONSTRAINT_SCHEMA: {
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
    case PLAN_CONSTRAINT_MATERIAL:
        return true;
    case PLAN_CONSTRAINT_HARMONY_SHAPE: {
        PlanHarmonyShape *shape = &out->params.harmony_shape;
        int anchor;
        if (!read_enum_member(reader, item, path, "shape", SHAPE_KIND_NAMES, COUNT_OF(SHAPE_KIND_NAMES), &value)) {
            return false;
        }
        shape->kind = (PlanHarmonyShapeKind)value;
        const cJSON *starts = cJSON_GetObjectItemCaseSensitive(item, "starts_on");
        const cJSON *ends = cJSON_GetObjectItemCaseSensitive(item, "ends_on");
        char at[PLAN_JSON_PATH_MAX];
        if (starts) {
            json_child_path(at, path, "starts_on");
            if (!read_enum(reader, starts, at, ANCHOR_NAMES, COUNT_OF(ANCHOR_NAMES), &anchor)) {
                return false;
            }
            shape->starts_on = (PlanHarmonyAnchor)anchor;
        }
        if (ends) {
            json_child_path(at, path, "ends_on");
            if (!read_enum(reader, ends, at, ANCHOR_NAMES, COUNT_OF(ANCHOR_NAMES), &anchor)) {
                return false;
            }
            shape->ends_on = (PlanHarmonyAnchor)anchor;
        }
        return read_optional_int(reader, item, path, "max_cells_per_chord", 0, 64, &shape->max_cells_per_chord) &&
               read_optional_bool(reader, item, path, "split_at_bars", &shape->split_at_bars) &&
               read_optional_bool(reader, item, path, "pedal", &shape->pedal);
    }
    case PLAN_CONSTRAINT_THEME_PAIRING: {
        int ending;
        if (!read_theme_half(reader, item, path, "beginning", true, &value) ||
            !read_theme_half(reader, item, path, "ending", false, &ending)) {
            return false;
        }
        out->params.theme_pairing.beginning = (PlanThemeBeginning)value;
        out->params.theme_pairing.ending = (PlanThemeEnding)ending;
        return true;
    }
    case PLAN_CONSTRAINT_ACCOMPANIMENT: {
        PlanAccompaniment *accompaniment = &out->params.accompaniment;
        const cJSON *texture = cJSON_GetObjectItemCaseSensitive(item, "texture");
        accompaniment->texture = -1;
        if (!read_enum_member(reader, item, path, "pattern", ACCOMPANIMENT_NAMES, COUNT_OF(ACCOMPANIMENT_NAMES), &value)) {
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
        return read_optional_int(reader, item, path, "low", 0, 127, &accompaniment->low) &&
               read_optional_int(reader, item, path, "high", 0, 127, &accompaniment->high);
    }
    case PLAN_CONSTRAINT_VOICING:
        return read_optional_bool(reader, item, path, "write_soprano", &out->params.voicing.write_soprano);
    case PLAN_CONSTRAINT_COUNTERPOINT:
        if (!cJSON_GetObjectItemCaseSensitive(item, "species")) {
            char at[PLAN_JSON_PATH_MAX];
            json_child_path(at, path, "species");
            return json_bad_format(reader, at, "missing species");
        }
        return read_optional_int(reader, item, path, "species", 1, 4, &out->params.counterpoint.species) &&
               read_optional_bool(reader, item, path, "above", &out->params.counterpoint.above);
    case PLAN_CONSTRAINT_RETURN:
        if (!json_read_int_member(reader, item, path, "section", 1, TIME_LIMIT, &number_value) ||
            !read_return_variation(reader, item, path, &out->params.section_return.variation)) {
            return false;
        }
        out->params.section_return.section = (NodeId)number_value;
        return true;
    case PLAN_CONSTRAINT_LAYERS: {
        const cJSON *lanes = plan_json_member(reader, item, path, "lanes", true);
        if (!lanes) {
            return false;
        }
        char lanes_path[PLAN_JSON_PATH_MAX];
        json_child_path(lanes_path, path, "lanes");
        if (!cJSON_IsArray(lanes)) {
            return json_bad_format(reader, lanes_path, "expected an array of lane names");
        }
        size_t index = 0;
        const cJSON *lane;
        cJSON_ArrayForEach(lane, lanes) {
            char at[PLAN_JSON_PATH_MAX];
            json_index_path(at, lanes_path, index++);
            if (!read_enum(reader, lane, at, plan_lane_names, PLAN_LANE_NAME_COUNT, &value)) {
                return false;
            }
            out->params.layers.lanes |= PLAN_LAYER_BIT(value);
        }
        if (out->params.layers.lanes == 0) {
            return json_bad_format(reader, lanes_path, "a layer sounds at least one lane");
        }
        return true;
    }
    case PLAN_CONSTRAINT_SECTION_LETTER: {
        const cJSON *letter = plan_json_member(reader, item, path, "letter", true);
        if (!letter) {
            return false;
        }
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "letter");
        if (!cJSON_IsString(letter) || letter->valuestring[0] == '\0' ||
            strlen(letter->valuestring) >= PLAN_SECTION_LETTER_MAX) {
            return json_bad_format(reader, at, "expected one to three characters");
        }
        snprintf(out->params.letter, PLAN_SECTION_LETTER_MAX, "%s", letter->valuestring);
        return true;
    }
    case PLAN_CONSTRAINT_INVENT_MOTIVE:
        return true;
    case PLAN_CONSTRAINT_STYLE: {
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
    case PLAN_CONSTRAINT_SOURCE: {
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
    case PLAN_CONSTRAINT_PICKUP: {
        const cJSON *length = plan_json_member(reader, item, path, "length", true);
        long numerator;
        unsigned long denominator;
        char rest;
        if (!length) {
            return false;
        }
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "length");
        if (!cJSON_IsString(length) || sscanf(length->valuestring, "%ld/%lu%c", &numerator, &denominator, &rest) != 2 ||
            denominator == 0 || numerator <= 0 || numerator > TIME_LIMIT || denominator > (unsigned long)TIME_LIMIT ||
            rational_make(numerator, denominator, &out->params.pickup) != RATIONAL_OK) {
            return json_bad_format(reader, at, "expected a length in whole notes like \"1/4\"");
        }
        return true;
    }
    case PLAN_CONSTRAINT_PIECE_PART: {
        const cJSON *id = NULL;
        char at[PLAN_JSON_PATH_MAX];
        if (!read_enum_member(reader, item, path, "part", PIECE_PART_NAMES, COUNT_OF(PIECE_PART_NAMES), &value) ||
            !(id = plan_json_member(reader, item, path, "id", true))) {
            return false;
        }
        json_child_path(at, path, "id");
        if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) >= PLAN_PIECE_ID_MAX) {
            return json_bad_format(reader, at, "expected one to fifteen characters");
        }
        out->params.piece_part.part = value;
        snprintf(out->params.piece_part.id, PLAN_PIECE_ID_MAX, "%s", id->valuestring);
        return true;
    }
    case PLAN_CONSTRAINT_MOTIVE_TONES:
        return read_motive_tones(reader, item, path, &out->params.motive_tones);
    case PLAN_CONSTRAINT_PEDAL:
        if (!json_read_int_member(reader, item, path, "degree", 1, 7, &number_value)) {
            return false;
        }
        out->params.pedal.degree = (int)number_value;
        return true;
    case PLAN_CONSTRAINT_CONSTRUCTION:
        return read_construction(reader, item, path, &out->params.construction);
    case PLAN_CONSTRAINT_FREE_PHRASE:
    case PLAN_CONSTRAINT_HOLD:
    case PLAN_CONSTRAINT_SEAM:
        return true;
    }
    return false;
}

static const char *result_name(PlanResult result) {
    switch (result) {
    case PLAN_ERR_BAD_TIME:
        return "invalid time";
    case PLAN_ERR_BAD_SPAN:
        return "span is empty or not inside its parent";
    case PLAN_ERR_BAD_KIND:
        return "this kind cannot go here";
    case PLAN_ERR_ROOT_EXISTS:
        return "a plan has one root";
    case PLAN_ERR_NOT_FOUND:
        return "refers to a missing node";
    case PLAN_ERR_LANE_OVERLAP:
        return "overlaps another span in the same lane";
    case PLAN_ERR_BAD_PAYLOAD:
        return "value is not valid here";
    case PLAN_ERR_GIVEN_LOCKED:
        return "the item is Given";
    default:
        return "could not be added";
    }
}

/* Reads composer.constraints and adds each one through add. */
typedef PlanResult (*AddConstraintFn)(PiecePlan *plan, size_t id, const PlanConstraint *constraint);

static PlanResult add_node_constraint(PiecePlan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_node_add_constraint(plan, (NodeId)id, constraint);
}

static PlanResult add_span_constraint(PiecePlan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_lane_span_add_constraint(plan, (LaneSpanId)id, constraint);
}

static bool read_constraints(JsonReader *reader, PiecePlan *plan, const cJSON *composer, const char *composer_path,
                             size_t id, AddConstraintFn add_constraint) {
    const cJSON *list = plan_json_member(reader, composer, composer_path, "constraints", true);
    if (!list) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, composer_path, "constraints");
    if (!cJSON_IsArray(list)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        PlanConstraint constraint;
        if (!read_constraint(reader, item, at, &constraint)) {
            return false;
        }
        if (add_constraint == add_node_constraint && constraint.kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            /* The labels are one to a note of the node's melody span. */
            LaneSpanId spans[16];
            size_t found = plan_node_lane_spans(plan, (NodeId)id, spans, 16);
            for (size_t k = 0; k < found && k < 16; k++) {
                size_t notes = plan_lane_span_payload(plan, spans[k])->notes.note_count;
                if (plan_lane_span_lane(plan, spans[k]) == PLAN_LANE_MELODY && notes != constraint.params.motive_tones.label_count) {
                    return json_fail(reader, PLAN_ERR_FORMAT, at, "motive_tones on %zu: %zu labels for %zu notes", id,
                                     constraint.params.motive_tones.label_count, notes);
                }
            }
        }
        PlanResult result = add_constraint(plan, id, &constraint);
        if (result == PLAN_ERR_ALLOC) {
            return json_fail(reader, PLAN_ERR_ALLOC, at, "out of memory");
        }
        if (result != PLAN_OK) {
            return json_fail(reader, PLAN_ERR_FORMAT, at, "constraint %s", result_name(result));
        }
    }
    return true;
}

typedef struct {
    const cJSON *json;
    NodeId parent;
    char path[PLAN_JSON_PATH_MAX];
} ParsedNode;

static bool count_nodes(JsonReader *reader, const cJSON *node, const char *path, size_t *count) {
    if (!cJSON_IsObject(node)) {
        return json_bad_format(reader, path, "expected an object");
    }
    (*count)++;
    const cJSON *children = plan_json_member(reader, node, path, "children", true);
    if (!children) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    if (!cJSON_IsArray(children)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!count_nodes(reader, child, at, count)) {
            return false;
        }
    }
    return true;
}

/* Files each node under its composer.id so nodes can be added in id order. */
static bool collect_nodes(JsonReader *reader, const cJSON *node, const char *path, NodeId parent, ParsedNode *table,
                          size_t count) {
    const cJSON *composer = plan_json_member(reader, node, path, "composer", true);
    if (!composer) {
        return false;
    }
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, path, "composer");
    if (!cJSON_IsObject(composer)) {
        return json_bad_format(reader, composer_path, "expected an object");
    }
    long id;
    if (!json_read_int_member(reader, composer, composer_path, "id", 1, (long)count, &id)) {
        return false;
    }
    if (table[id - 1].json) {
        char id_path[PLAN_JSON_PATH_MAX];
        json_child_path(id_path, composer_path, "id");
        return json_bad_format(reader, id_path, "id is used twice");
    }
    table[id - 1].json = node;
    table[id - 1].parent = parent;
    snprintf(table[id - 1].path, PLAN_JSON_PATH_MAX, "%s", path);

    const cJSON *children = cJSON_GetObjectItemCaseSensitive(node, "children");
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!collect_nodes(reader, child, at, (NodeId)id, table, count)) {
            return false;
        }
    }
    return true;
}

static bool build_nodes(JsonReader *reader, PiecePlan *plan, const cJSON *form) {
    size_t count = 0;
    if (!count_nodes(reader, form, "form", &count)) {
        return false;
    }
    ParsedNode *table = calloc(count, sizeof(ParsedNode));
    if (!table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    bool ok = collect_nodes(reader, form, "form", PLAN_NODE_NONE, table, count);
    for (size_t i = 0; ok && i < count; i++) {
        const ParsedNode *parsed = &table[i];
        int kind;
        PlanSpan span;
        char at[PLAN_JSON_PATH_MAX];
        ok = read_enum_member(reader, parsed->json, parsed->path, "kind", NODE_KINDS, COUNT_OF(NODE_KINDS), &kind) &&
             json_read_span(reader, parsed->json, parsed->path, &span);
        const cJSON *label = ok ? plan_json_member(reader, parsed->json, parsed->path, "label", false) : NULL;
        if (ok && label && !cJSON_IsNull(label) && !cJSON_IsString(label)) {
            json_child_path(at, parsed->path, "label");
            ok = json_bad_format(reader, at, "expected a string or null");
        }
        if (ok) {
            NodeId id;
            const char *name = cJSON_IsString(label) ? label->valuestring : NULL;
            PlanResult result = plan_add_node(plan, parsed->parent, (PlanNodeKind)kind, span, name, &id);
            if (result == PLAN_ERR_ALLOC) {
                ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
            } else if (result != PLAN_OK) {
                ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "node %s", result_name(result));
            }
        }
    }
    free(table);
    return ok;
}

typedef struct {
    const cJSON *json;
    unsigned lanes;
    char path[PLAN_JSON_PATH_MAX];
} ParsedLane;

static bool count_section(JsonReader *reader, const cJSON *document, const char *name, size_t *count) {
    const cJSON *section = plan_json_member(reader, document, "", name, false);
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    if (strcmp(name, "meter") == 0) {
        section = plan_json_member(reader, section, "meter", "spans", true);
        if (!section) {
            return false;
        }
    }
    if (!cJSON_IsArray(section)) {
        return json_bad_format(reader, name, "expected an array");
    }
    *count += (size_t)cJSON_GetArraySize(section);
    return true;
}

static bool collect_section(JsonReader *reader, const cJSON *document, const char *name, unsigned lanes,
                            ParsedLane *table, size_t count) {
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(document, name);
    char section_path[PLAN_JSON_PATH_MAX];
    snprintf(section_path, sizeof(section_path), "%s", name);
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    if (strcmp(name, "meter") == 0) {
        section = cJSON_GetObjectItemCaseSensitive(section, "spans");
        json_child_path(section_path, name, "spans");
    }
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, section) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, section_path, index++);
        if (!cJSON_IsObject(entry)) {
            return json_bad_format(reader, at, "expected an object");
        }
        const cJSON *composer = plan_json_member(reader, entry, at, "composer", true);
        if (!composer) {
            return false;
        }
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, at, "composer");
        if (!cJSON_IsObject(composer)) {
            return json_bad_format(reader, composer_path, "expected an object");
        }
        long id;
        if (!json_read_int_member(reader, composer, composer_path, "id", 1, (long)count, &id)) {
            return false;
        }
        if (table[id - 1].json) {
            char id_path[PLAN_JSON_PATH_MAX];
            json_child_path(id_path, composer_path, "id");
            return json_bad_format(reader, id_path, "id is used twice");
        }
        table[id - 1].json = entry;
        table[id - 1].lanes = lanes;
        snprintf(table[id - 1].path, PLAN_JSON_PATH_MAX, "%s", at);
    }
    return true;
}

static bool read_payload(JsonReader *reader, const cJSON *composer, const char *composer_path, PlanLaneKind lane,
                         PlanLanePayload *payload, PlanNote **notes_storage) {
    const cJSON *object = plan_json_member(reader, composer, composer_path, "payload", true);
    if (!object) {
        return false;
    }
    char path[PLAN_JSON_PATH_MAX];
    json_child_path(path, composer_path, "payload");
    if (!cJSON_IsObject(object)) {
        return json_bad_format(reader, path, "expected an object");
    }
    long first;
    long second;
    long third;
    memset(payload, 0, sizeof(*payload));
    switch (lane) {
    case PLAN_LANE_METER:
        if (!json_read_int_member(reader, object, path, "beats_per_bar", 1, 1000, &first) ||
            !json_read_int_member(reader, object, path, "beat_unit", 1, 1000, &second) ||
            !json_read_int_member(reader, object, path, "bars_per_hyperbar", 0, 1000, &third)) {
            return false;
        }
        payload->meter.beats_per_bar = (int)first;
        payload->meter.beat_unit = (int)second;
        payload->meter.bars_per_hyperbar = (int)third;
        return true;
    case PLAN_LANE_KEY:
        return read_key_members(reader, object, path, &payload->key);
    case PLAN_LANE_HARMONY: {
        const cJSON *numeral = plan_json_member(reader, object, path, "roman_numeral", true);
        if (!numeral) {
            return false;
        }
        char numeral_path[PLAN_JSON_PATH_MAX];
        json_child_path(numeral_path, path, "roman_numeral");
        if (!cJSON_IsString(numeral)) {
            return json_bad_format(reader, numeral_path, "expected a string");
        }
        if (numeral->valuestring[0] == '\0' || strlen(numeral->valuestring) >= PLAN_ROMAN_NUMERAL_MAX) {
            return json_fail(reader, PLAN_ERR_FORMAT, numeral_path, "expected 1 to %d characters",
                        PLAN_ROMAN_NUMERAL_MAX - 1);
        }
        snprintf(payload->harmony.roman_numeral, PLAN_ROMAN_NUMERAL_MAX, "%s", numeral->valuestring);
        return read_function(reader, object, path, &payload->harmony);
    }
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE: {
        const cJSON *list = plan_json_member(reader, object, path, "notes", true);
        if (!list) {
            return false;
        }
        char list_path[PLAN_JSON_PATH_MAX];
        json_child_path(list_path, path, "notes");
        /* An inner voice may be silent over its span: an empty array. */
        if (!cJSON_IsArray(list) || (cJSON_GetArraySize(list) == 0 && lane != PLAN_LANE_INNER_VOICE)) {
            return json_bad_format(reader, list_path, "expected a non-empty array");
        }
        size_t count = (size_t)cJSON_GetArraySize(list);
        PlanNote *notes = count ? calloc(count, sizeof(PlanNote)) : NULL;
        if (count && !notes) {
            return json_fail(reader, PLAN_ERR_ALLOC, list_path, "out of memory");
        }
        *notes_storage = notes;
        size_t index = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, list) {
            char at[PLAN_JSON_PATH_MAX];
            json_index_path(at, list_path, index);
            if (!cJSON_IsObject(item)) {
                return json_bad_format(reader, at, "expected an object");
            }
            long pitch;
            if (!json_read_int_member(reader, item, at, "midi_pitch", 0, 127, &pitch) ||
                !read_rational_member(reader, item, at, "offset", &notes[index].offset) ||
                !read_rational_member(reader, item, at, "duration", &notes[index].duration)) {
                return false;
            }
            notes[index].midi = (int)pitch;
            notes[index].spelled = false;
            const cJSON *spelling = cJSON_GetObjectItemCaseSensitive(item, "spelling");
            if (spelling) {
                char spelling_path[PLAN_JSON_PATH_MAX];
                json_child_path(spelling_path, at, "spelling");
                if (!cJSON_IsString(spelling) ||
                    !pitch_spelling_parse_name(spelling->valuestring, &notes[index].letter, &notes[index].accidental,
                                               NULL)) {
                    return json_bad_format(reader, spelling_path, "expected a note name like \"F#\" or \"Bb\"");
                }
                notes[index].spelled = true;
            }
            index++;
        }
        payload->notes.notes = notes;
        payload->notes.note_count = count;
        return true;
    }
    }
    return false;
}

static bool build_lane_span(JsonReader *reader, PiecePlan *plan, const ParsedLane *parsed, size_t expected_id) {
    const cJSON *composer = cJSON_GetObjectItemCaseSensitive(parsed->json, "composer");
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, parsed->path, "composer");

    int lane;
    long owner;
    long voice;
    PlanSpan span;
    if (!read_enum_member(reader, composer, composer_path, "lane", plan_lane_names, PLAN_LANE_NAME_COUNT, &lane) ||
        !json_read_int_member(reader, composer, composer_path, "owner", 1, TIME_LIMIT, &owner) ||
        !json_read_int_member(reader, composer, composer_path, "voice", 0, 1000, &voice) ||
        !json_read_span(reader, parsed->json, parsed->path, &span)) {
        return false;
    }
    if (!(parsed->lanes & (1u << lane))) {
        char lane_path[PLAN_JSON_PATH_MAX];
        json_child_path(lane_path, composer_path, "lane");
        return json_bad_format(reader, lane_path, "lane does not belong in this section");
    }

    PlanLanePayload payload;
    PlanNote *notes_storage = NULL;
    bool ok = read_payload(reader, composer, composer_path, (PlanLaneKind)lane, &payload, &notes_storage);
    if (ok) {
        LaneSpanId id;
        PlanResult result =
            plan_add_lane_span(plan, (NodeId)owner, (PlanLaneKind)lane, (int)voice, span, &payload, &id);
        if (result == PLAN_ERR_ALLOC) {
            ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
        } else if (result != PLAN_OK) {
            ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "lane span %s", result_name(result));
        } else if (id != expected_id) {
            ok = json_bad_format(reader, parsed->path, "lane span ids must run from 1 without gaps");
        }
    }
    free(notes_storage);
    return ok;
}

static bool build_lane_spans(JsonReader *reader, PiecePlan *plan, const cJSON *document) {
    static const struct {
        const char *name;
        unsigned lanes;
    } sections[] = {
        {"meter", 1u << PLAN_LANE_METER},
        {"keys", 1u << PLAN_LANE_KEY},
        {"chords", 1u << PLAN_LANE_HARMONY},
        {"melody", 1u << PLAN_LANE_MELODY},
        {"texture", (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)},
    };
    size_t count = 0;
    for (size_t i = 0; i < COUNT_OF(sections); i++) {
        if (!count_section(reader, document, sections[i].name, &count)) {
            return false;
        }
    }
    if (count == 0) {
        return true;
    }
    ParsedLane *table = calloc(count, sizeof(ParsedLane));
    if (!table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "", "out of memory");
    }
    bool ok = true;
    for (size_t i = 0; ok && i < COUNT_OF(sections); i++) {
        ok = collect_section(reader, document, sections[i].name, sections[i].lanes, table, count);
    }
    for (size_t i = 0; ok && i < count; i++) {
        ok = build_lane_span(reader, plan, &table[i], i + 1);
    }

    /* Constraints and states go on last so a Given item never refuses its own constraints. */
    for (size_t i = 0; ok && i < count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        ok = read_constraints(reader, plan, composer, composer_path, i + 1, add_span_constraint);
    }
    free(table);
    return ok;
}

static bool apply_states(JsonReader *reader, PiecePlan *plan, const cJSON *document) {
    /* Nodes: walk the form tree; the ids were validated while building. */
    size_t node_count = plan_node_count(plan);
    ParsedNode *table = node_count ? calloc(node_count, sizeof(ParsedNode)) : NULL;
    if (node_count && !table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    bool ok = true;
    if (node_count) {
        ok = collect_nodes(reader, cJSON_GetObjectItemCaseSensitive(document, "form"), "form", PLAN_NODE_NONE, table,
                           node_count);
    }
    for (size_t i = 0; ok && i < node_count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        int state;
        ok = read_constraints(reader, plan, composer, composer_path, i + 1, add_node_constraint) &&
             read_enum_member(reader, composer, composer_path, "state", STATE_NAMES, COUNT_OF(STATE_NAMES), &state);
        if (ok) {
            plan_node_set_state(plan, (NodeId)(i + 1), (PlanState)state);
        }
    }
    free(table);
    if (!ok) {
        return false;
    }

    static const char *const SECTIONS[] = {"meter", "keys", "chords", "melody", "texture"};
    for (size_t s = 0; s < COUNT_OF(SECTIONS); s++) {
        const cJSON *section = cJSON_GetObjectItemCaseSensitive(document, SECTIONS[s]);
        if (section && strcmp(SECTIONS[s], "meter") == 0) {
            section = cJSON_GetObjectItemCaseSensitive(section, "spans");
        }
        const cJSON *entry;
        cJSON_ArrayForEach(entry, section) {
            const cJSON *composer = cJSON_GetObjectItemCaseSensitive(entry, "composer");
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(composer, "id");
            const cJSON *state_item = cJSON_GetObjectItemCaseSensitive(composer, "state");
            char at[PLAN_JSON_PATH_MAX];
            snprintf(at, sizeof(at), "%s[%d].composer.state", SECTIONS[s], 0);
            int state;
            if (!read_enum(reader, state_item, at, STATE_NAMES, COUNT_OF(STATE_NAMES), &state)) {
                return false;
            }
            plan_lane_span_set_state(plan, (LaneSpanId)id->valueint, (PlanState)state);
        }
    }
    return true;
}

/*
 * Version 2 records metadata.analyzer_version only for a plan loaded from an Analyzer result. A version 1 file wrote the
 * Composer's own version under that key (B-A-10), so it says nothing about an analysis and is ignored.
 */
static bool read_metadata(JsonReader *reader, const cJSON *document, PiecePlan *plan) {
    const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    if (reader->version == 1 || !metadata) {
        return true;
    }
    if (!cJSON_IsObject(metadata)) {
        return json_bad_format(reader, "metadata", "expected an object");
    }
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(metadata, "analyzer_version");
    if (!analyzer) {
        return true;
    }
    if (!cJSON_IsString(analyzer)) {
        return json_bad_format(reader, "metadata.analyzer_version", "expected a string");
    }
    if (plan_set_analyzer_version(plan, analyzer->valuestring) != PLAN_OK) {
        return json_bad_format(reader, "metadata.analyzer_version", "expected a version of fewer than 32 characters");
    }
    return true;
}

static PlanResult read_document(const cJSON *document, PiecePlan *plan, PlanJsonError *error) {
    JsonReader reader = {error, PLAN_OK, 0};
    if (!cJSON_IsObject(document)) {
        json_bad_format(&reader, "", "expected a JSON object");
        return reader.result;
    }
    long version;
    if (cJSON_GetObjectItemCaseSensitive(document, "format")) {
        const cJSON *format = cJSON_GetObjectItemCaseSensitive(document, "format");
        if (!cJSON_IsString(format) || strcmp(format->valuestring, PLAN_FORMAT_NAME) != 0) {
            json_bad_format(&reader, "format", "expected \"" PLAN_FORMAT_NAME "\"");
            return reader.result;
        }
        if (!json_read_int_member(&reader, document, "", "format_version", 2, URSATZ_COMPOSER_PLAN_FORMAT_VERSION,
                                  &version)) {
            return reader.result;
        }
    } else if (!json_read_int_member(&reader, document, "", "schema_version", 1, PLAN_JSON_SCHEMA_VERSION, &version)) {
        return reader.result;
    }
    reader.version = (int)version;
    if (!read_metadata(&reader, document, plan)) {
        return reader.result;
    }
    const cJSON *form = plan_json_member(&reader, document, "", "form", true);
    if (!form) {
        return reader.result;
    }
    if (!cJSON_IsNull(form) && !build_nodes(&reader, plan, form)) {
        return reader.result;
    }
    if (!build_lane_spans(&reader, plan, document)) {
        return reader.result;
    }
    if (!apply_states(&reader, plan, document)) {
        return reader.result;
    }
    return PLAN_OK;
}

PlanResult plan_from_json(const char *text, PiecePlan **out, PlanJsonError *error) {
    if (!text || !out) {
        json_set_error(error, -1, "", "text or out is NULL");
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        json_set_error(error, end ? (long)(end - text) : -1, "", "not valid JSON");
        return PLAN_ERR_PARSE;
    }
    PiecePlan *plan = plan_create();
    if (!plan) {
        cJSON_Delete(document);
        json_set_error(error, -1, "", "out of memory");
        return PLAN_ERR_ALLOC;
    }
    PlanResult result = read_document(document, plan, error);
    cJSON_Delete(document);
    if (result != PLAN_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PLAN_OK;
}

PlanResult plan_read_json_file(const char *path, PiecePlan **out, PlanJsonError *error) {
    if (!path || !out) {
        json_set_error(error, -1, "", "path or out is NULL");
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        json_set_error(error, -1, "", "could not open '%s'", path);
        return PLAN_ERR_IO;
    }
    char *text = NULL;
    size_t size = 0;
    size_t capacity = 0;
    bool ok = true;
    while (ok && !feof(file)) {
        if (size + 4096 + 1 > capacity) {
            capacity = capacity ? capacity * 2 : 8192;
            char *grown = realloc(text, capacity);
            ok = grown != NULL;
            text = grown ? grown : text;
        }
        if (ok) {
            size += fread(text + size, 1, 4096, file);
            ok = !ferror(file);
        }
    }
    fclose(file);
    if (!ok) {
        free(text);
        json_set_error(error, -1, "", "could not read '%s'", path);
        return PLAN_ERR_IO;
    }
    text[size] = '\0';
    PlanResult result = plan_from_json(text, out, error);
    free(text);
    return result;
}
