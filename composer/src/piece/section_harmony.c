#include <ursatz_composer/piece.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <theory/harmony/harmony_chord.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "util/plan_util.h"
#include "util/request_chords.h"

#define NUMERALS_MAX 32
#define CHORDS_MAX 16

/* The numerals of a progression text, "I vi IV V", split in place; false for none or more than NUMERALS_MAX. */
static bool split_numerals(char *text, const char **out, size_t *count) {
    char *save = NULL;
    *count = 0;
    for (char *word = strtok_r(text, " \t,", &save); word; word = strtok_r(NULL, " \t,", &save)) {
        if (*count == NUMERALS_MAX) {
            return false;
        }
        out[(*count)++] = word;
    }
    return *count > 0;
}

static void refuse(char *message, size_t size, const char *format, ...) {
    va_list args;
    if (message && size > 0) {
        va_start(args, format);
        vsnprintf(message, size, format, args);
        va_end(args);
    }
}

PieceResult piece_give_harmony(PiecePlan *plan, NodeId section, const SectionHarmony *harmony, const PlanMeter *meter,
                               const SpelledKey *key, char *message, size_t message_size) {
    if (!plan || !harmony || !meter || !key) {
        return PIECE_ERR_NULL;
    }
    size_t cells_per_bar = 0;
    GivenResult given = GIVEN_OK;
    switch (harmony->kind) {
    case SECTION_HARMONY_NONE:
        return PIECE_OK;
    case SECTION_HARMONY_PROGRESSION: {
        char text[SECTION_NUMERALS_MAX];
        const char *numerals[NUMERALS_MAX];
        size_t count;
        snprintf(text, sizeof(text), "%s", harmony->numerals);
        if (!split_numerals(text, numerals, &count) || !grow_cells_per_bar(meter, &cells_per_bar)) {
            refuse(message, message_size, "a progression is 1 to %d numerals in a meter the harmony stage writes", NUMERALS_MAX);
            return PIECE_ERR_BAD_REQUEST;
        }
        given = plan_give_progression(plan, section, 0, numerals, count, harmony->cells ? harmony->cells : cells_per_bar);
        break;
    }
    case SECTION_HARMONY_SCHEMA: {
        GalantSchemaId schema;
        char chords[GALANT_MAX_STAGES][PLAN_ROMAN_NUMERAL_MAX];
        const char *numerals[GALANT_MAX_STAGES];
        size_t count = 0;
        if (!harmony_schema_find(harmony->schema, &schema) || !harmony_schema_available(schema, key->material) ||
            !harmony_schema_chords(key, schema, chords, &count) || count == 0) {
            refuse(message, message_size, "the schema '%s' is not one that makes a chord of each stage in %s", harmony->schema,
                   key->material && key->material->name ? key->material->name : "this key");
            return PIECE_ERR_BAD_REQUEST;
        }
        for (size_t i = 0; i < count; i++) {
            numerals[i] = chords[i];
        }
        given = plan_give_progression(plan, section, 0, numerals, count, 1);
        break;
    }
    case SECTION_HARMONY_CHORDS: {
        cJSON *list = cJSON_CreateArray();
        GivenChord chords[CHORDS_MAX];
        char numerals[CHORDS_MAX][PLAN_ROMAN_NUMERAL_MAX];
        size_t count = 0;
        for (size_t i = 0; list && i < harmony->chord_count; i++) {
            cJSON *item = cJSON_CreateObject();
            if (!item || !cJSON_AddNumberToObject(item, "bar", (double)harmony->chords[i].bar) ||
                !cJSON_AddStringToObject(item, "text", harmony->chords[i].text) || !cJSON_AddItemToArray(list, item)) {
                cJSON_Delete(item);
                cJSON_Delete(list);
                list = NULL;
            }
        }
        if (!list) {
            return PIECE_ERR_ALLOC;
        }
        PlanSpan span = plan_node_span(plan, section);
        Rational bar_length;
        plan_meter_bar_length(meter, &bar_length);
        Rational bars;
        long length = rational_div(span.length, bar_length, &bars) == RATIONAL_OK && bars.den == 1 ? (long)bars.num : 0;
        ComposerError error;
        memset(&error, 0, sizeof(error));
        ComposerStatus read = request_read_chords(list, "/chords", "the section", 1, meter, key, length, chords, numerals, CHORDS_MAX, &count,
                                                  &error);
        cJSON_Delete(list);
        if (read != COMPOSER_OK) {
            refuse(message, message_size, "%s", error.message);
            return PIECE_ERR_BAD_REQUEST;
        }
        given = plan_give_chords(plan, section, chords, count);
        break;
    }
    }
    if (given != GIVEN_OK) {
        refuse(message, message_size, "the harmony cannot be given: %s", given_result_name(given));
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

bool piece_check_harmony(const PlanMeter *meter, const SpelledKey *key, Rational length, const SectionHarmony *harmony, char *message,
                         size_t message_size) {
    if (harmony->kind == SECTION_HARMONY_NONE) {
        return true;
    }
    /* The harmony is given to a scratch plan of one section as long as the section, in its key and meter, as the build gives it. */
    PiecePlan *plan = plan_create();
    NodeId form = PLAN_NODE_NONE;
    NodeId section = PLAN_NODE_NONE;
    PlanSpan span = {{0, 1}, length};
    PlanLanePayload payload;
    LaneSpanId lane;
    bool ok = plan && plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, span, "check", &form) == PLAN_OK &&
              plan_add_node(plan, form, PLAN_NODE_SECTION, span, "section", &section) == PLAN_OK;
    SectionDescription description;
    section_description_init(&description);
    description.has_pairing = true;
    description.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
    description.pairing.ending = SECTION_ENDING_CONTINUATION;
    description.has_cadence = true;
    description.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    ok = ok && plan_node_set_section(plan, section, &description) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.meter = *meter;
    ok = ok && plan_add_lane_span(plan, form, PLAN_LANE_METER, 0, span, &payload, &lane) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.key = *key;
    ok = ok && plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, span, &payload, &lane) == PLAN_OK;
    PieceResult result = ok ? piece_give_harmony(plan, section, harmony, meter, key, message, message_size) : PIECE_ERR_ALLOC;
    plan_destroy(plan);
    return result == PIECE_OK;
}
