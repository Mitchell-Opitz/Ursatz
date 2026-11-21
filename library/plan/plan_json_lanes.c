#include "plan/plan_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/json.h>
#include <model/pitch/pitch_arithmetic.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/tonal_key.h>
#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "plan/section_json.h"
#include "plan/plan_constraint_kinds.h"
#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "plan/plan_common.h"

/* The lanes of a plan file: writing the lane spans and their payloads, reading them and their notes. */

static void plan_key_label(const SpelledKey *key, char *out, size_t size) {
    char tonic[8] = "?";
    pitch_spelling_name(key->tonic_letter, key->tonic_accidental, tonic, sizeof(tonic));
    snprintf(out, size, "%s %s", tonic, plan_key_word(key));
}

static bool lane_label(const PlanLanePayload *payload, PlanLaneKind lane, char *out, size_t size) {
    switch (lane) {
    case PLAN_LANE_METER:
        snprintf(out, size, "%d/%d", payload->meter.beats_per_bar, payload->meter.beat_unit);
        return true;
    case PLAN_LANE_REGION:
        plan_key_label(&payload->region, out, size);
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

/* How a note was made from its source, by the name the file gives it (the index is the PlanNoteOperation). */
static const char *const plan_note_operation_names[] = {"", "copy", "motive", "return"};

#define PLAN_NOTE_OPERATION_COUNT (sizeof(plan_note_operation_names) / sizeof(plan_note_operation_names[0]))

static cJSON *notes_json(const PlanNotes *notes) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < notes->note_count; i++) {
        const PlanNote *note = &notes->notes[i];
        cJSON *object = cJSON_CreateObject();
        char spelling[8];
        bool spelt = !note->spelled || pitch_spelling_name(note->letter, note->accidental, spelling, sizeof(spelling));
        if (!(object && spelt && plan_json_add(object, "midi_pitch", json_number(note->midi)) &&
              (!note->spelled || plan_json_add(object, "spelling", cJSON_CreateString(spelling))) &&
              plan_json_add(object, "offset", common_json_rational_object(note->offset.num, note->offset.den)) &&
              plan_json_add(object, "duration", common_json_rational_object(note->duration.num, note->duration.den)) &&
              plan_json_add(object, "id", json_number((double)note->id)) &&
              (note->source == PLAN_NOTE_NONE ||
               (plan_json_add(object, "source", json_number((double)note->source)) &&
                plan_json_add(object, "operation", cJSON_CreateString(plan_note_operation_names[note->operation])))) &&
              plan_json_append(array, object))) {
            if (object && !cJSON_GetObjectItem(object, "duration")) {
                cJSON_Delete(object);
            }
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *payload_json(PlanLaneKind lane, const PlanLanePayload *payload, const PlanKeyRelation *relation) {
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
    case PLAN_LANE_REGION:
        ok = plan_json_key_payload_json(object, &payload->region, relation);
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

/* The label of a key span: the key's, or for a span not yet resolved the relation and its anchor. */
static void key_span_label(const Plan *plan, LaneSpanId id, char *out, size_t size) {
    const PlanKeyRelation *relation = plan_lane_span_key_relation(plan, id);
    if (relation && !relation->resolved) {
        snprintf(out, size, "%s from %s", relation->relation, plan_json_KEY_ANCHOR_NAMES[relation->anchor]);
    } else {
        plan_key_label(&plan_lane_span_payload(plan, id)->region, out, size);
    }
}

static cJSON *lane_entry_json(const Plan *plan, LaneSpanId id) {
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    PlanSpan span = plan_lane_span_span(plan, id);
    const PlanLanePayload *payload = plan_lane_span_payload(plan, id);
    char label[64];
    lane_label(payload, lane, label, sizeof(label));
    if (lane == PLAN_LANE_REGION) {
        key_span_label(plan, id, label, sizeof(label));
    }

    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    bool ok = object && composer;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_lane_span_rank(plan, id))) &&
         plan_json_add(composer, "owner", json_number((long)plan_node_rank(plan, plan_lane_span_owner(plan, id)))) &&
         plan_json_add(composer, "lane", cJSON_CreateString(plan_lane_names[lane])) &&
         plan_json_add(composer, "voice", json_number(plan_lane_span_voice(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(plan_json_STATE_NAMES[plan_lane_span_state(plan, id)])) &&
         plan_json_add(composer, "constraints", plan_json_span_constraints_json(plan, id)) &&
         plan_json_add(composer, "payload", payload_json(lane, payload, plan_lane_span_key_relation(plan, id)));
    ok = ok && plan_json_add(object, "span", plan_json_span_json(span)) && plan_json_add(object, "label", cJSON_CreateString(label));
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
cJSON *plan_json_lane_array_json(const Plan *plan, unsigned mask) {
    cJSON *array = cJSON_CreateArray();
    for (LaneSpanId id = 1; array && id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && (mask & (1u << plan_lane_span_lane(plan, id))) && !plan_json_append(array, lane_entry_json(plan, id))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
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

/* Adds id; false when the set already holds it or memory ran out (*grown_failed then says which). */
static bool note_id_set_add(struct NoteIdSet *set, size_t id, bool *out_of_memory) {
    *out_of_memory = false;
    if (set->count * 2 >= set->capacity) {
        size_t capacity = set->capacity ? set->capacity * 2 : 64;
        size_t *slots = calloc(capacity, sizeof(size_t));
        if (!slots) {
            *out_of_memory = true;
            return false;
        }
        for (size_t i = 0; i < set->capacity; i++) {
            if (set->slots[i]) {
                size_t at = (set->slots[i] * 11400714819323198485ull) & (capacity - 1);
                while (slots[at]) {
                    at = (at + 1) & (capacity - 1);
                }
                slots[at] = set->slots[i];
            }
        }
        free(set->slots);
        set->slots = slots;
        set->capacity = capacity;
    }
    size_t at = (id * 11400714819323198485ull) & (set->capacity - 1);
    while (set->slots[at]) {
        if (set->slots[at] == id) {
            return false;
        }
        at = (at + 1) & (set->capacity - 1);
    }
    set->slots[at] = id;
    set->count++;
    if (id > set->max) {
        set->max = id;
    }
    return true;
}

/* A version 4 note's "id" (required, unique in the file, from 1), "source" and "operation" (optional). */
static bool read_note_identity(JsonReader *reader, const cJSON *item, const char *at, PlanNote *note) {
    long id;
    if (!json_read_int_member(reader, item, at, "id", 1, 0x7fffffffffffffffL, &id)) {
        return false;
    }
    bool out_of_memory = false;
    if (reader->note_ids && !note_id_set_add(reader->note_ids, (size_t)id, &out_of_memory)) {
        char id_path[PLAN_JSON_PATH_MAX];
        json_child_path(id_path, at, "id");
        return out_of_memory ? json_fail(reader, PLAN_ERR_ALLOC, id_path, "out of memory")
                             : json_bad_format(reader, id_path, "this note id is already another note's");
    }
    note->id = (PlanNoteId)id;
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(item, "source");
    if (source) {
        char source_path[PLAN_JSON_PATH_MAX];
        json_child_path(source_path, at, "source");
        long value;
        if (!json_read_long(reader, source, source_path, 1, 0x7fffffffffffffffL, &value)) {
            return false;
        }
        note->source = (PlanNoteId)value;
    }
    const cJSON *operation = cJSON_GetObjectItemCaseSensitive(item, "operation");
    if (operation) {
        char operation_path[PLAN_JSON_PATH_MAX];
        json_child_path(operation_path, at, "operation");
        int value;
        if (!json_read_enum(reader, operation, operation_path, plan_note_operation_names + 1, PLAN_NOTE_OPERATION_COUNT - 1, &value)) {
            return false;
        }
        note->operation = (PlanNoteOperation)(value + 1);
    }
    return true;
}

static bool read_payload(JsonReader *reader, const cJSON *composer, const char *composer_path, PlanLaneKind lane,
                         PlanLanePayload *payload, PlanKeyRelation *relation, PlanNote **notes_storage) {
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
    case PLAN_LANE_REGION:
        return plan_json_read_key_span(reader, object, path, &payload->region, relation);
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
        return plan_json_read_function(reader, object, path, &payload->harmony);
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
                !plan_json_read_rational_member(reader, item, at, "offset", &notes[index].offset) ||
                !plan_json_read_rational_member(reader, item, at, "duration", &notes[index].duration)) {
                return false;
            }
            notes[index].midi = (int)pitch;
            if (reader->version >= 4 && !read_note_identity(reader, item, at, &notes[index])) {
                return false;
            }
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

static bool build_lane_span(JsonReader *reader, Plan *plan, const ParsedLane *parsed, size_t expected_id) {
    const cJSON *composer = cJSON_GetObjectItemCaseSensitive(parsed->json, "composer");
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, parsed->path, "composer");

    int lane;
    long owner;
    long voice;
    PlanSpan span;
    if (!json_read_enum_member(reader, composer, composer_path, "lane", plan_lane_names, PLAN_LANE_NAME_COUNT, &lane) ||
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
    PlanKeyRelation relation;
    PlanNote *notes_storage = NULL;
    memset(&relation, 0, sizeof(relation));
    bool ok = read_payload(reader, composer, composer_path, (PlanLaneKind)lane, &payload, &relation, &notes_storage);
    if (ok) {
        LaneSpanId id;
        PlanResult result =
            plan_add_lane_span(plan, (NodeId)owner, (PlanLaneKind)lane, (int)voice, span, &payload, &id);
        if (result == PLAN_ERR_ALLOC) {
            ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
        } else if (result != PLAN_OK) {
            ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "lane span %s", plan_json_result_name(result));
        } else if (id != expected_id) {
            ok = json_bad_format(reader, parsed->path, "lane span ids must run from 1 without gaps");
        } else if (relation.relation[0] != '\0' &&
                   plan_lane_span_set_key_relation(plan, id, relation.relation, relation.anchor,
                                                   relation.anchor == PLAN_KEY_ANCHOR_SECTION ? relation.anchor_section : NULL,
                                                   relation.resolved) != PLAN_OK) {
            ok = json_bad_format(reader, parsed->path, "a key span names its relation badly");
        }
    }
    free(notes_storage);
    return ok;
}

/* The document member that holds the region lane: "regions" from format 6, "keys" before it. */
const char *plan_json_regions_section_name(const JsonReader *reader) {
    return reader->version >= 6 ? "regions" : "keys";
}

bool plan_json_build_lane_spans(JsonReader *reader, Plan *plan, const cJSON *document) {
    struct {
        const char *name;
        unsigned lanes;
    } sections[] = {
        {"meter", 1u << PLAN_LANE_METER},
        {NULL, 1u << PLAN_LANE_REGION}, /* "regions" from format 6, "keys" before it */
        {"chords", 1u << PLAN_LANE_HARMONY},
        {"melody", 1u << PLAN_LANE_MELODY},
        {"texture", (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)},
    };
    for (size_t i = 0; i < COUNT_OF(sections); i++) {
        if (!sections[i].name) {
            sections[i].name = plan_json_regions_section_name(reader);
        }
    }
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
        ok = plan_json_read_constraints(reader, plan, composer, composer_path, i + 1, plan_json_add_span_constraint);
    }
    free(table);
    return ok;
}
