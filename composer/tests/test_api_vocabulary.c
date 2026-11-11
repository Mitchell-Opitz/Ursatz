#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_random.h>
#include <ursatz_composer/preferences.h>
#include <ursatz_composer/variant_registry.h>

#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The vocabulary and validation entries (docs/api.md): every list is not empty and each value is accepted by the reader
 * that reads it (a request built from the value validates), a piece request without cadences gets the defaults of
 * pieces-model.js defaultCadence, a motive's harmony comes back parsed, and a bad request is refused with its path. The
 * responses are written beside the test, for the schema test.
 */

static cJSON *vocabulary;

static void write_response(const char *name, const ComposerResponse *response) {
    char path[128];
    snprintf(path, sizeof path, "api_%s_response_vocabulary.json", name);
    test_write_file(path, response->json);
}

static char *print(cJSON *value) {
    char *text = cJSON_PrintUnformatted(value);
    assert(text);
    return text;
}

static const cJSON *list(const char *name) {
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(vocabulary, name);
    assert(cJSON_IsArray(items));
    assert(cJSON_GetArraySize(items) > 0);
    return items;
}

static const char *value_of(const cJSON *entry) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(entry, "value");
    assert(cJSON_IsString(value));
    return value->valuestring;
}

/* The response of validate: its "request" object. The caller frees the response. */
static cJSON *validated(const char *kind, const char *request, ComposerStatus expected, ComposerResponse *response) {
    ComposerStatus status = composer_api_validate(kind, request, response);
    if (status != expected) {
        fprintf(stderr, "%s: status %d (%s) for %s\n", kind, status, response->error.message, request);
    }
    assert(status == expected);
    if (status != COMPOSER_OK) {
        assert(!response->json);
        return NULL;
    }
    cJSON *document = cJSON_Parse(response->json);
    assert(document);
    return document;
}

static void accepted(const char *kind, cJSON *request) {
    char *text = print(request);
    ComposerResponse response;
    cJSON *document = validated(kind, text, COMPOSER_OK, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(text);
}

static void refused(const char *kind, const char *request, const char *path) {
    ComposerResponse response;
    ComposerStatus status = composer_api_validate(kind, request, &response);
    assert(status == COMPOSER_ERR_REQUEST || status == COMPOSER_ERR_UNSATISFIABLE);
    assert(strcmp(response.error.path, path) == 0);
    assert(response.error.message[0]);
    composer_response_destroy(&response);
}

static cJSON *random_request(const char *mode, const char *meter) {
    cJSON *request = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", meter));
    assert(cJSON_AddNumberToObject(request, "seed", 3));
    return request;
}

static cJSON *read_json(const char *path) {
    char *text = test_read_file(path, NULL);
    cJSON *value = cJSON_Parse(text);
    assert(value);
    free(text);
    return value;
}

/* A piece request of sections [{id, part, beats}] with no cadences, in D major, 4/4. */
static cJSON *piece_request(const char *const *parts, size_t count) {
    cJSON *request = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(request, "schema", "ursatz-piece-request") &&
           cJSON_AddNumberToObject(request, "schema_version", 1) && cJSON_AddStringToObject(request, "key", "D") &&
           cJSON_AddStringToObject(request, "mode", "major") && cJSON_AddStringToObject(request, "meter", "4/4"));
    cJSON *sections = cJSON_AddArrayToObject(request, "sections");
    assert(sections);
    for (size_t i = 0; i < count; i++) {
        char id[32];
        snprintf(id, sizeof id, "s%u", (unsigned)i);
        cJSON *section = cJSON_CreateObject();
        assert(section && cJSON_AddStringToObject(section, "id", id) && cJSON_AddStringToObject(section, "part", parts[i]) &&
               cJSON_AddNumberToObject(section, "beats", 16) && cJSON_AddItemToArray(sections, section));
    }
    return request;
}

/* --- the lists ---------------------------------------------------------------------------------------- */

static void test_lists(void) {
    ComposerResponse response;
    assert(composer_api_vocabulary(&response) == COMPOSER_OK);
    assert(response.json && !response.plan_json && !response.midi && !response.musicxml);
    write_response("vocabulary", &response);
    vocabulary = cJSON_Parse(response.json);
    assert(vocabulary);
    composer_response_destroy(&response);
    composer_response_destroy(&response);

    static const char *const NAMES[] = {"meters",         "modes",          "piece_modes",   "keys",
                                        "pitch_class_names", "cadences",    "theme_cadences", "motive_forms",
                                        "random_motive_strategies",
                                        "roles",          "functions",      "distances",     "theme_slots",
                                        "theme_ideas",    "theme_motive_categories", "theme_pools", "accompaniment_patterns",
                                        "piece_parts",    "piece_intos",    "fill_kinds",    "tone_labels"};
    for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++) {
        list(NAMES[i]);
    }
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(vocabulary, "pickups")));
    assert(cJSON_GetObjectItemCaseSensitive(vocabulary, "variant_operations"));

    /* The limits the command line shows are the library's. */
    const cJSON *limits = cJSON_GetObjectItemCaseSensitive(vocabulary, "limits");
    assert(cJSON_IsObject(limits));
    assert(cJSON_GetObjectItemCaseSensitive(limits, "motive_notes")->valuedouble == MOTIVE_MAX_NOTES);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "motive_chords")->valuedouble == MOTIVE_MAX_CHORDS);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "motive_features")->valuedouble == MOTIVE_MAX_FEATURES);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "variant_chain")->valuedouble == VARIANT_CHAIN_MAX);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "variant_functions")->valuedouble == VARIANT_FUNCTIONS);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "random_motive_count")->valuedouble == MOTIVE_RANDOM_MAX_COUNT);

    /* The preferences (R-411): the member is the defaults document, a request made of it validates, and the limits are the reader's. */
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    cJSON *written = composer_preferences_write(&defaults);
    assert(written);
    const cJSON *preferences = cJSON_GetObjectItemCaseSensitive(vocabulary, "preferences");
    assert(cJSON_IsObject(preferences) && cJSON_Compare(preferences, written, true));
    cJSON *round_trip = cJSON_CreateObject();
    assert(round_trip && cJSON_AddStringToObject(round_trip, "mode", "major") && cJSON_AddStringToObject(round_trip, "meter", "4/4") &&
           cJSON_AddItemToObject(round_trip, "preferences", written));
    char *round_trip_text = print(round_trip);
    ComposerResponse round_trip_response;
    cJSON *round_trip_document = validated("random_motive", round_trip_text, COMPOSER_OK, &round_trip_response);
    cJSON_Delete(round_trip_document);
    composer_response_destroy(&round_trip_response);
    free(round_trip_text);
    cJSON_Delete(round_trip);
    const cJSON *preference_limits = cJSON_GetObjectItemCaseSensitive(limits, "preferences");
    assert(cJSON_IsObject(preference_limits));
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "weight_max")->valuedouble == 20);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "keep_max")->valuedouble == MELODY_KEEP_MAX);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "attempts_max")->valuedouble == 256);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "pickup_one_in_max")->valuedouble == 64);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "accented_one_in_max")->valuedouble == 64);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "long_nct_one_in_max")->valuedouble == 64);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "leaps_per_motif_max")->valuedouble == 11);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "syncopation_one_in_max")->valuedouble == 64);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "skips_per_motif_max")->valuedouble == 11);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "plan_chords_max")->valuedouble == 4);
    assert(cJSON_GetObjectItemCaseSensitive(preference_limits, "plan_beat_max")->valuedouble == 12);

    /* The orders and the notes of the GUI tables it replaces. */
    const cJSON *meters = list("meters");
    assert(cJSON_GetArraySize(meters) == 4);
    assert(strcmp(value_of(cJSON_GetArrayItem(meters, 0)), "2/4") == 0);
    assert(strcmp(value_of(cJSON_GetArrayItem(meters, 3)), "6/8") == 0);
    for (int i = 0; i < 4; i++) {
        const cJSON *meter = cJSON_GetArrayItem(meters, i);
        const cJSON *beats = cJSON_GetObjectItemCaseSensitive(meter, "beats");
        const cJSON *second = cJSON_GetObjectItemCaseSensitive(meter, "second_chord_beat");
        const char *value = value_of(meter);
        assert(cJSON_IsNumber(beats) && beats->valueint == (value[0] == '6' ? 6 : value[0] - '0'));
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(meter, "chord_onset_step")->valuestring, "1/2") == 0);
        if (strcmp(value, "4/4") == 0) {
            assert(second && second->valueint == 2);
        } else if (strcmp(value, "6/8") == 0) {
            assert(second && second->valueint == 3);
        } else {
            assert(!second);
        }
    }
    assert(cJSON_GetArraySize(list("keys")) == 24);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("keys"), 1)), "Db major") == 0);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("keys"), 13)), "C# minor") == 0);
    assert(cJSON_GetArraySize(list("pitch_class_names")) == 12);
    assert(cJSON_GetArraySize(list("cadences")) == 5);
    assert(cJSON_GetArraySize(list("theme_cadences")) == 3);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("cadences"), 0)), "PAC") == 0);
    assert(cJSON_GetArraySize(list("piece_modes")) == 3 && cJSON_GetArraySize(list("modes")) == 2);
    const cJSON *pickups = cJSON_GetObjectItemCaseSensitive(vocabulary, "pickups");
    for (int i = 0; i < 4; i++) {
        const cJSON *offered = cJSON_GetObjectItemCaseSensitive(pickups, value_of(cJSON_GetArrayItem(meters, i)));
        assert(cJSON_IsArray(offered) && cJSON_GetArraySize(offered) > 0);
        assert(strcmp(cJSON_GetArrayItem(offered, 0)->valuestring, "0") == 0);
    }
}

/* --- each value is read -------------------------------------------------------------------------------- */

static void test_round_trip(void) {
    const cJSON *entry = NULL;

    /* Meters, modes, keys, forms and roles: a random motive request. */
    cJSON_ArrayForEach(entry, list("meters")) {
        cJSON *request = random_request("major", value_of(entry));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("modes")) {
        cJSON *request = random_request(value_of(entry), "4/4");
        accepted("random_motive", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("keys")) {
        char key[32];
        snprintf(key, sizeof key, "%s", value_of(entry));
        char *space = strchr(key, ' ');
        assert(space);
        *space = '\0';
        cJSON *request = random_request(space + 1, "4/4");
        assert(cJSON_AddStringToObject(request, "key", value_of(entry)));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("motive_forms")) {
        cJSON *request = random_request("major", "4/4");
        assert(cJSON_AddStringToObject(request, "form", value_of(entry)));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }
    /* The strategies the vocabulary serves are the ones the request takes: segments (the default) and drawn. */
    assert(cJSON_GetArraySize(list("random_motive_strategies")) == 2);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("random_motive_strategies"), 0)), "segments") == 0);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("random_motive_strategies"), 1)), "drawn") == 0);
    cJSON_ArrayForEach(entry, list("random_motive_strategies")) {
        cJSON *request = random_request("major", "4/4");
        assert(cJSON_AddStringToObject(request, "strategy", value_of(entry)));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("roles")) {
        cJSON *request = random_request("minor", "3/4");
        assert(cJSON_AddStringToObject(request, "role", value_of(entry)));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }

    /* Style categories a theme can read without (a style is needed to read them without). */
    cJSON_ArrayForEach(entry, list("theme_motive_categories")) {
        cJSON *request = random_request("major", "4/4");
        cJSON *without = cJSON_AddArrayToObject(request, "style_without");
        assert(without && cJSON_AddItemToArray(without, cJSON_CreateString(value_of(entry))));
        assert(cJSON_AddStringToObject(request, "style", COMPOSER_FIXTURE_DIR "/styles/chorales.style.json"));
        accepted("random_motive", request);
        cJSON_Delete(request);
    }

    /* Parts, fills, cadences, intos and pickups: a piece request. */
    cJSON_ArrayForEach(entry, list("piece_parts")) {
        const char *parts[] = {"body", value_of(entry)};
        size_t count = strcmp(value_of(entry), "body") == 0 ? 1 : 2;
        if (strcmp(value_of(entry), "intro") == 0) {
            parts[0] = "intro";
            parts[1] = "body";
        }
        cJSON *request = piece_request(parts, count);
        accepted("piece", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("cadences")) {
        const char *parts[] = {"body"};
        cJSON *request = piece_request(parts, 1);
        cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "sections"), 0);
        assert(cJSON_AddStringToObject(section, "cadence", value_of(entry)));
        accepted("piece", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("piece_modes")) {
        if (strcmp(value_of(entry), "style") == 0) {
            continue; /* the mode "style" reads the key and mode from a style model file */
        }
        const char *parts[] = {"body"};
        cJSON *request = piece_request(parts, 1);
        cJSON_ReplaceItemInObjectCaseSensitive(request, "mode", cJSON_CreateString(value_of(entry)));
        accepted("piece", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("fill_kinds")) {
        if (strcmp(value_of(entry), "empty") != 0 && strcmp(value_of(entry), "motif") != 0) {
            continue; /* a theme fill carries a plan and a progression fill its numerals: the piece tests have both */
        }
        const char *parts[] = {"body"};
        cJSON *request = piece_request(parts, 1);
        cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "sections"), 0);
        cJSON *fill = cJSON_AddObjectToObject(section, "fill");
        assert(fill && cJSON_AddStringToObject(fill, "kind", value_of(entry)));
        accepted("piece", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("piece_intos")) {
        const char *parts[] = {"body"};
        cJSON *request = piece_request(parts, 1);
        cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "sections"), 0);
        cJSON_ReplaceItemInObjectCaseSensitive(section, "beats", cJSON_CreateNumber(32));
        cJSON *fill = cJSON_AddObjectToObject(section, "fill");
        assert(fill && cJSON_AddStringToObject(fill, "kind", "motif") && cJSON_AddStringToObject(fill, "into", value_of(entry)));
        if (strcmp(value_of(entry), "hybrid") == 0) {
            assert(cJSON_AddStringToObject(fill, "hybrid", "antecedent+continuation"));
        }
        accepted("piece", request);
        cJSON_Delete(request);
    }
    const cJSON *pickups = cJSON_GetObjectItemCaseSensitive(vocabulary, "pickups");
    cJSON_ArrayForEach(entry, list("meters")) {
        const cJSON *offered = cJSON_GetObjectItemCaseSensitive(pickups, value_of(entry));
        const cJSON *pickup = NULL;
        cJSON_ArrayForEach(pickup, offered) {
            const char *parts[] = {"body"};
            cJSON *request = piece_request(parts, 1);
            cJSON_ReplaceItemInObjectCaseSensitive(request, "meter", cJSON_CreateString(value_of(entry)));
            cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "sections"), 0);
            assert(cJSON_AddStringToObject(section, "pickup", pickup->valuestring));
            accepted("piece", request);
            cJSON_Delete(request);
        }
    }

    /* Slots, ideas, theme cadences and accompaniment patterns: a theme request. */
    cJSON_ArrayForEach(entry, list("theme_cadences")) {
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
        cJSON_ReplaceItemInObjectCaseSensitive(request, "cadence", cJSON_CreateString(value_of(entry)));
        accepted("theme", request);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("theme_slots")) {
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
        cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
        cJSON_ReplaceItemInObjectCaseSensitive(slots, value_of(entry), cJSON_CreateString("A"));
        /* The reader knows the slot: it is not refused as an unknown one (a form that lacks the slot refuses it as a plan). */
        char *text = print(request);
        ComposerResponse response;
        ComposerStatus status = composer_api_validate("theme", text, &response);
        assert(status == COMPOSER_OK || strstr(response.error.message, "unknown slot") == NULL);
        composer_response_destroy(&response);
        free(text);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("theme_ideas")) {
        /* The reader knows the idea: it is not refused as an unknown one (a form that lacks the idea is another matter). */
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/idea-repetition.json");
        cJSON *ideas = cJSON_CreateObject();
        assert(ideas && cJSON_AddStringToObject(ideas, value_of(entry), "R"));
        cJSON_ReplaceItemInObjectCaseSensitive(request, "ideas", ideas);
        char *text = print(request);
        ComposerResponse response;
        ComposerStatus status = composer_api_validate("theme", text, &response);
        assert(status == COMPOSER_OK || strstr(response.error.message, "unknown idea") == NULL);
        composer_response_destroy(&response);
        free(text);
        cJSON_Delete(request);
    }
    cJSON_ArrayForEach(entry, list("accompaniment_patterns")) {
        if (strcmp(value_of(entry), "motive") == 0) {
            continue; /* a pattern from a motive carries the motive */
        }
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
        cJSON *accompaniment = cJSON_CreateObject();
        assert(accompaniment && cJSON_AddStringToObject(accompaniment, "pattern", value_of(entry)));
        cJSON_ReplaceItemInObjectCaseSensitive(request, "accompaniment", accompaniment);
        accepted("theme", request);
        cJSON_Delete(request);
    }
}

/* The theme form, style weight and seed vocabularies (API 1.1): each offered value is read, and the limits are the readers'. */
/* theme_slots say which slot every theme needs, piece_intos which beginning and ending a one-pairing archetype is (R-430). */
static void test_slot_rules(void) {
    const char *const slots[] = {"basic", "contrasting", "continuation", "cadential"};
    const cJSON *list_slots = list("theme_slots");
    assert(cJSON_GetArraySize(list_slots) == 4);
    for (int i = 0; i < 4; i++) {
        const cJSON *entry = cJSON_GetArrayItem(list_slots, i);
        const cJSON *required = cJSON_GetObjectItemCaseSensitive(entry, "required");
        assert(strcmp(value_of(entry), slots[i]) == 0);
        assert(cJSON_IsBool(required));
        assert(cJSON_IsTrue(required) == (i == 0));
    }
    const cJSON *intos = list("piece_intos");
    assert(cJSON_GetArraySize(intos) == 3);
    const char *const beginnings[] = {"presentation", "antecedent", NULL};
    const char *const endings[] = {"continuation", "consequent", NULL};
    for (int i = 0; i < 3; i++) {
        const cJSON *entry = cJSON_GetArrayItem(intos, i);
        const cJSON *beginning = cJSON_GetObjectItemCaseSensitive(entry, "beginning");
        const cJSON *ending = cJSON_GetObjectItemCaseSensitive(entry, "ending");
        if (!beginnings[i]) {
            assert(strcmp(value_of(entry), "hybrid") == 0 && !beginning && !ending);
            continue;
        }
        assert(cJSON_IsString(beginning) && strcmp(beginning->valuestring, beginnings[i]) == 0);
        assert(cJSON_IsString(ending) && strcmp(ending->valuestring, endings[i]) == 0);
        /* The words are the vocabulary's own. */
        bool beginning_listed = false;
        bool ending_listed = false;
        const cJSON *part = NULL;
        cJSON_ArrayForEach(part, list("theme_beginnings")) {
            beginning_listed = beginning_listed || strcmp(value_of(part), beginning->valuestring) == 0;
        }
        cJSON_ArrayForEach(part, list("theme_endings")) {
            ending_listed = ending_listed || strcmp(value_of(part), ending->valuestring) == 0;
        }
        assert(beginning_listed && ending_listed);
    }
}

static void test_theme_form_lists(void) {
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list("theme_bars")) {
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
        cJSON_ReplaceItemInObjectCaseSensitive(request, "bars", cJSON_CreateNumber(atoi(value_of(entry))));
        accepted("theme", request);
        cJSON_Delete(request);
    }
    assert(cJSON_GetArraySize(list("theme_bars")) == 2);
    assert(cJSON_GetArraySize(list("theme_first_cadences")) == 2);
    assert(strcmp(value_of(cJSON_GetArrayItem(list("theme_first_cadences"), 0)), "IAC") == 0);
    cJSON_ArrayForEach(entry, list("theme_first_cadences")) {
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/period-contrast.json");
        cJSON_ReplaceItemInObjectCaseSensitive(request, "first_cadence", cJSON_CreateString(value_of(entry)));
        accepted("theme", request);
        cJSON_Delete(request);
    }

    /* Every beginning with every ending is a hybrid the reader takes, and the ideas listed are the ones the form has. */
    const cJSON *beginning = NULL;
    const cJSON *ending = NULL;
    assert(cJSON_GetArraySize(list("theme_beginnings")) == 3 && cJSON_GetArraySize(list("theme_endings")) == 3);
    cJSON_ArrayForEach(beginning, list("theme_beginnings")) {
        cJSON_ArrayForEach(ending, list("theme_endings")) {
            char hybrid[64];
            snprintf(hybrid, sizeof hybrid, "%s+%s", value_of(beginning), value_of(ending));
            cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
            cJSON_ReplaceItemInObjectCaseSensitive(request, "into", cJSON_CreateString("hybrid"));
            cJSON_ReplaceItemInObjectCaseSensitive(request, "hybrid", cJSON_CreateString(hybrid));
            char *text = print(request);
            ComposerResponse response;
            ComposerStatus status = composer_api_validate("theme", text, &response);
            /* A pairing a sentence or a period already names is refused as such, never as an unknown word. */
            assert(status == COMPOSER_OK || strstr(response.error.message, "hybrid must be a beginning") == NULL);
            composer_response_destroy(&response);
            free(text);
            cJSON_Delete(request);
        }
    }
    const cJSON *first = cJSON_GetArrayItem(list("theme_beginnings"), 0);
    assert(strcmp(value_of(first), "presentation") == 0);
    const cJSON *idea = NULL;
    const cJSON *known = NULL;
    cJSON_ArrayForEach(beginning, list("theme_beginnings")) {
        cJSON_ArrayForEach(idea, cJSON_GetObjectItemCaseSensitive(beginning, "ideas")) {
            bool found = false;
            cJSON_ArrayForEach(known, list("theme_ideas")) {
                found = found || strcmp(value_of(known), idea->valuestring) == 0;
            }
            assert(found);
        }
    }
    assert(strcmp(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(first, "ideas"), 1)->valuestring, "repetition") == 0);

    cJSON_ArrayForEach(entry, list("style_weights")) {
        cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
        assert(cJSON_AddStringToObject(request, "style", COMPOSER_FIXTURE_DIR "/styles/chorales.style.json"));
        cJSON_ReplaceItemInObjectCaseSensitive(request, "style_weight", cJSON_CreateString(value_of(entry)));
        accepted("theme", request);
        cJSON_Delete(request);
    }

    const cJSON *limits = cJSON_GetObjectItemCaseSensitive(vocabulary, "limits");
    assert(cJSON_GetObjectItemCaseSensitive(limits, "tempo_min")->valuedouble == 20);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "tempo_max")->valuedouble == 400);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "piece_sections")->valuedouble == 32);
    assert(cJSON_GetObjectItemCaseSensitive(limits, "piece_beats")->valuedouble == 4096);
    const cJSON *seed_max = cJSON_GetObjectItemCaseSensitive(limits, "seed_max");
    assert(cJSON_GetObjectItemCaseSensitive(seed_max, "variants")->valuedouble == 4294967231.0);
    assert(cJSON_GetObjectItemCaseSensitive(seed_max, "random_motive")->valuedouble == 4294967295.0);
    assert(cJSON_GetObjectItemCaseSensitive(seed_max, "theme")->valuedouble == 4294967295.0);
    assert(cJSON_GetObjectItemCaseSensitive(seed_max, "piece")->valuedouble == 4294967295.0);

    /* The largest seed is read, the next one is refused with the path. */
    cJSON *request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
    assert(cJSON_AddNumberToObject(request, "seed", 4294967295.0));
    accepted("theme", request);
    cJSON_Delete(request);
    request = read_json(COMPOSER_FIXTURE_DIR "/theme/one-motive.json");
    assert(cJSON_AddNumberToObject(request, "seed", 4294967296.0));
    char *text = print(request);
    refused("theme", text, "/seed");
    free(text);
    cJSON_Delete(request);
}

/* The variants entry reads functions and distance before it makes anything. */
static void test_variants_words(void) {
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list("distances")) {
        char request[512];
        snprintf(request, sizeof request,
                 "{\"input\":{\"format\":\"notes\",\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D minor\",\"meter\":\"4/4\","
                 "\"distance\":\"%s\",\"count\":1,\"seed\":1}",
                 value_of(entry));
        ComposerResponse response;
        ComposerStatus status = composer_api_variants(request, NULL, &response);
        assert(status != COMPOSER_ERR_REQUEST);
        composer_response_destroy(&response);
    }
    cJSON_ArrayForEach(entry, list("functions")) {
        char request[512];
        snprintf(request, sizeof request,
                 "{\"input\":{\"format\":\"notes\",\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D minor\",\"meter\":\"4/4\","
                 "\"functions\":[\"%s\"],\"count\":1,\"seed\":1}",
                 value_of(entry));
        ComposerResponse response;
        ComposerStatus status = composer_api_variants(request, NULL, &response);
        assert(status != COMPOSER_ERR_REQUEST);
        composer_response_destroy(&response);
    }
    /* Every operation of the registry is the one composer_api_variant_operations lists. */
    ComposerResponse operations;
    assert(composer_api_variant_operations(&operations) == COMPOSER_OK);
    cJSON *registry = cJSON_Parse(operations.json);
    assert(registry && cJSON_Compare(registry, cJSON_GetObjectItemCaseSensitive(vocabulary, "variant_operations"), true));
    cJSON_Delete(registry);
    composer_response_destroy(&operations);
}

/* The tone labels are the roles a tone override takes. */
static void test_tone_labels(void) {
    const cJSON *entry = NULL;
    assert(strcmp(value_of(cJSON_GetArrayItem(list("tone_labels"), 0)), "chord") == 0);
    assert(cJSON_GetArraySize(list("tone_labels")) == 11);
    cJSON_ArrayForEach(entry, list("tone_labels")) {
        assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry, "abbreviation")));
        char request[512];
        snprintf(request, sizeof request,
                 "{\"input\":{\"format\":\"notes\",\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D minor\",\"meter\":\"4/4\","
                 "\"tones\":[\"2=%s\"]}",
                 value_of(entry));
        ComposerResponse response;
        ComposerStatus status = composer_api_motive(request, NULL, &response);
        assert(status == COMPOSER_OK);
        composer_response_destroy(&response);
    }
}

/* --- the defaults --------------------------------------------------------------------------------------- */

static void expect_cadences(const char *const *parts, size_t count, const char *const *cadences, size_t total) {
    cJSON *request = piece_request(parts, count);
    char *text = print(request);
    ComposerResponse response;
    cJSON *document = validated("piece", text, COMPOSER_OK, &response);
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "request"), "sections");
    assert(cJSON_GetArraySize(sections) == (int)total);
    for (size_t i = 0; i < total; i++) {
        const cJSON *section = cJSON_GetArrayItem(sections, (int)i);
        const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(section, "cadence");
        assert(cJSON_IsString(cadence) && strcmp(cadence->valuestring, cadences[i]) == 0);
    }
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, (int)total - 1), "part")->valuestring,
                  "ending") == 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(text);
    cJSON_Delete(request);
}

static void test_defaults(void) {
    /* A loop body alone: the default ending joins it (PAC), the body leads back (HC). */
    const char *const body[] = {"body"};
    const char *const body_cadences[] = {"HC", "PAC"};
    expect_cadences(body, 1, body_cadences, 2);

    /* Intro, two bodies, a turnaround: the last of the intro and of the loop are HC; the default ending is PAC. */
    const char *const loop[] = {"intro", "intro", "body", "body", "turnaround"};
    const char *const loop_cadences[] = {"IAC", "HC", "IAC", "IAC", "HC", "PAC"};
    expect_cadences(loop, 5, loop_cadences, 6);

    /* Two endings: only the last of them is PAC. */
    const char *const ending[] = {"intro", "body", "turnaround", "ending", "ending"};
    const char *const ending_cadences[] = {"HC", "IAC", "HC", "IAC", "PAC"};
    expect_cadences(ending, 5, ending_cadences, 5);

    /* A cadence the request names is kept, and the sections are given back in order. */
    const char *const kept[] = {"body", "body"};
    cJSON *request = piece_request(kept, 2);
    cJSON *first = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "sections"), 0);
    assert(cJSON_AddStringToObject(first, "cadence", "DC"));
    char *text = print(request);
    ComposerResponse response;
    cJSON *document = validated("piece", text, COMPOSER_OK, &response);
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "request"), "sections");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, 0), "cadence")->valuestring, "DC") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, 1), "cadence")->valuestring, "HC") == 0);
    write_response("validate", &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(text);
    cJSON_Delete(request);

    /* A random motive: the seed and what it drew. */
    request = random_request("random", "random");
    text = print(request);
    document = validated("random_motive", text, COMPOSER_OK, &response);
    const cJSON *filled = cJSON_GetObjectItemCaseSensitive(document, "request");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(filled, "mode");
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(filled, "meter");
    assert(cJSON_IsString(mode) && (strcmp(mode->valuestring, "major") == 0 || strcmp(mode->valuestring, "minor") == 0));
    assert(cJSON_IsString(meter) && strcmp(meter->valuestring, "random") != 0);
    assert(cJSON_GetObjectItemCaseSensitive(filled, "seed")->valueint == 3);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(text);
    cJSON_Delete(request);

    /* The same seed draws what composer_api_random_motive reports as drawn. */
    ComposerResponse drawn;
    assert(composer_api_random_motive("{\"mode\":\"random\",\"meter\":\"random\",\"seed\":3}", NULL, &drawn) == COMPOSER_OK);
    cJSON *made = cJSON_Parse(drawn.json);
    const cJSON *drawn_object = cJSON_GetObjectItemCaseSensitive(made, "drawn");
    assert(drawn_object);
    cJSON_Delete(made);
    composer_response_destroy(&drawn);
}

static void test_harmony(void) {
    const char *request =
        "{\"input\":{\"format\":\"notes\",\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D major\",\"meter\":\"4/4\","
        "\"harmony\":\"I V@3/2 V7@3\"}";
    ComposerResponse response;
    cJSON *document = validated("motive", request, COMPOSER_OK, &response);
    const cJSON *filled = cJSON_GetObjectItemCaseSensitive(document, "request");
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(filled, "harmony");
    assert(cJSON_IsArray(harmony) && cJSON_GetArraySize(harmony) == 3);
    static const char *const NUMERALS[] = {"I", "V", "V7"};
    static const char *const ONSETS[] = {"0", "3/2", "3"};
    for (int i = 0; i < 3; i++) {
        const cJSON *chord = cJSON_GetArrayItem(harmony, i);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(chord, "numeral")->valuestring, NUMERALS[i]) == 0);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(chord, "onset")->valuestring, ONSETS[i]) == 0);
    }
    /* The defaults of a motive request. */
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(filled, "role")->valuestring, "melodic") == 0);
    const cJSON *input = cJSON_GetObjectItemCaseSensitive(filled, "input");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(input, "format")->valuestring, "notes") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(input, "form")->valuestring, "full") == 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);

    /* A request without harmony has none added, and a meter the file declares is filled in. */
    cJSON *file = cJSON_CreateObject();
    cJSON *file_input = cJSON_AddObjectToObject(file, "input");
    char *musicxml = test_read_file(COMPOSER_FIXTURE_DIR "/motive_d_minor.musicxml", NULL);
    assert(file_input && cJSON_AddStringToObject(file_input, "format", "musicxml") && cJSON_AddStringToObject(file_input, "text", musicxml));
    free(musicxml);
    char *text = print(file);
    document = validated("motive", text, COMPOSER_OK, &response);
    filled = cJSON_GetObjectItemCaseSensitive(document, "request");
    assert(!cJSON_GetObjectItemCaseSensitive(filled, "harmony"));
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(filled, "meter")->valuestring, "4/4") == 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(text);
    cJSON_Delete(file);
}

static void test_refusals(void) {
    /* The harmony grammar takes upper case numerals; a bad chord is refused at /harmony. */
    refused("motive",
            "{\"input\":{\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D major\",\"meter\":\"4/4\",\"harmony\":\"i V@3/2\"}",
            "/harmony");
    refused("motive",
            "{\"input\":{\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D major\",\"meter\":\"4/4\",\"harmony\":\"I V7/V@3\"}",
            "/harmony");
    refused("motive", "{\"input\":{\"text\":\"D4:q F4:e A4:e D5:h\"},\"key\":\"D major\",\"meter\":\"5/7\"}", "/meter");
    refused("random_motive", "{\"mode\":\"dorian\",\"meter\":\"4/4\"}", "/mode");
    refused("random_motive", "{\"mode\":\"major\",\"meter\":\"random\",\"key\":\"D minor\"}", "/key");
    refused("piece", "{\"schema\":\"ursatz-piece-request\",\"schema_version\":1}", "/key");
    refused("piece", "{\"schema\":\"other\",\"schema_version\":1,\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\"}", "");
    refused("theme", "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2}", "/motives");
    refused("theme", "[]", "");
    refused("piece", "{\"api_version\":9}", "/api_version");
    refused("variants", "{}", "/kind");
    refused(NULL, "{}", "/kind");

    ComposerResponse response;
    assert(composer_api_validate("motive", NULL, &response) == COMPOSER_ERR_REQUEST);
    composer_response_destroy(&response);
}

int main(void) {
    assert(strcmp(composer_api_version(), "1.17") == 0);
    test_lists();
    test_round_trip();
    test_slot_rules();
    test_theme_form_lists();
    test_variants_words();
    test_tone_labels();
    test_defaults();
    test_harmony();
    test_refusals();
    cJSON_Delete(vocabulary);
    printf("test_api_vocabulary: ok\n");
    return 0;
}
