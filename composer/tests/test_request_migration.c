#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif
#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name

/*
 * What migrates on read (R-597): a request names its material in "material" (a K1 id, name, key word or alias) or in the old
 * "mode" (major, minor, style, random), and the two give the same reply; the two together are refused; a motive document's key
 * reads either; plans of formats 1, 2 and 3 read to the same plan; and a v3 plan whose key span is named only by a relation
 * composes the same notes as the plan that names the key. The plan readers and the relation refusals have their own cases in
 * test_plan_json.
 */

typedef ComposerStatus (*Entry)(const ComposerContext *, const char *, const ComposerJob *, ComposerResponse *);

/* --- helpers --------------------------------------------------------------------------------------- */

static cJSON *load_json(const char *path) {
    char *text = test_must_read_file(path, NULL);
    cJSON *json = cJSON_Parse(text);
    assert(json);
    free(text);
    return json;
}

static cJSON *motive_request(const char *fixture) {
    char *text = test_must_read_file(fixture, NULL);
    for (char *c = text; *c; c++) {
        if (*c == '\n') {
            *c = ' ';
        }
    }
    cJSON *request = cJSON_CreateObject();
    cJSON *input = request ? cJSON_AddObjectToObject(request, "input") : NULL;
    assert(input && cJSON_AddStringToObject(input, "text", text));
    free(text);
    return request;
}

/* Renames the member `from` of the object to `to`, giving it `value` when value is not NULL. */
static void rename_member(cJSON *object, const char *from, const char *to, const char *value) {
    cJSON *item = cJSON_DetachItemFromObjectCaseSensitive(object, from);
    assert(item);
    if (value) {
        cJSON_Delete(item);
        item = cJSON_CreateString(value);
        assert(item);
    }
    assert(cJSON_AddItemToObject(object, to, item));
}

static char *print(const cJSON *request) {
    char *printed = cJSON_PrintUnformatted(request);
    assert(printed);
    return printed;
}

static ComposerStatus run(Entry entry, const cJSON *request, ComposerResponse *response) {
    char *printed = print(request);
    ComposerStatus status = entry(test_context(), printed, NULL, response);
    free(printed);
    return status;
}

static bool same_text(const char *a, const char *b) {
    return (a == NULL && b == NULL) || (a && b && strcmp(a, b) == 0);
}

static bool same_reply(const ComposerResponse *a, const ComposerResponse *b) {
    return same_text(a->json, b->json) && same_text(a->plan_json, b->plan_json) && same_text(a->musicxml, b->musicxml) &&
           a->midi_size == b->midi_size && (a->midi_size == 0 || memcmp(a->midi, b->midi, a->midi_size) == 0);
}

/*
 * The request as written with "mode" (the member `member` of `object`, whose value is `old`), and again with "material" carrying each
 * of `words`: all the replies are the same. `object` is the object that holds the member (the request, or its "request" member).
 */
static void expect_same_replies(Entry entry, cJSON *request, cJSON *object, const char *old, const char *const *words, size_t count) {
    ComposerResponse expected;
    assert(run(entry, request, &expected) == COMPOSER_OK);
    assert(expected.json);
    for (size_t i = 0; i < count; i++) {
        cJSON *copy = cJSON_Duplicate(request, true);
        assert(copy);
        cJSON *holder = copy;
        if (object != request) {
            holder = cJSON_GetObjectItemCaseSensitive(copy, "request");
            assert(holder);
        }
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(holder, "mode")->valuestring, old) == 0);
        rename_member(holder, "mode", "material", words[i]);
        ComposerResponse got;
        ComposerStatus status = run(entry, copy, &got);
        if (status != COMPOSER_OK) {
            fprintf(stderr, "material '%s' for mode '%s': %s\n", words[i], old, got.error.message);
        }
        bool same = status == COMPOSER_OK && same_reply(&expected, &got);
        if (status == COMPOSER_OK && !same) {
            fprintf(stderr, "material '%s' for mode '%s': the reply differs\n%s\n%s\n", words[i], old, expected.json, got.json);
        }
        assert(same);
        composer_response_destroy(&got);
        cJSON_Delete(copy);
    }
    composer_response_destroy(&expected);
}

static void expect_refused(Entry entry, const cJSON *request, const char *path_suffix, const char *message) {
    ComposerResponse response;
    ComposerStatus status = run(entry, request, &response);
    size_t length = strlen(response.error.path);
    size_t suffix = strlen(path_suffix);
    if (status != COMPOSER_ERR_REQUEST || length < suffix || strcmp(response.error.path + length - suffix, path_suffix) != 0 ||
        !strstr(response.error.message, message)) {
        fprintf(stderr, "wanted a request refusal at '%s' with '%s', got %d at '%s': %s\n", path_suffix, message, (int)status,
                response.error.path, response.error.message);
    }
    assert(status == COMPOSER_ERR_REQUEST && length >= suffix && strcmp(response.error.path + length - suffix, path_suffix) == 0);
    assert(strstr(response.error.message, message));
    assert(!response.json && !response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
}

static const char *const MAJOR_WORDS[] = {"major", "Major", "omt.material.major", "Ionian"};
static const char *const MINOR_WORDS[] = {"minor", "MINOR", "omt.material.natural-minor", "natural minor"};

/* --- requests ---------------------------------------------------------------------------------------- */

static cJSON *random_request(const char *mode) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 4) && cJSON_AddNumberToObject(request, "count", 2));
    return request;
}

static cJSON *plan_request(const char *mode) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "form", "binary") && cJSON_AddStringToObject(request, "key", "D") &&
           cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 2));
    return request;
}

static cJSON *counterpoint_request(const char *mode) {
    cJSON *request = motive_request(FIXTURE("cantus_d_dorian.txt"));
    assert(cJSON_AddNumberToObject(request, "species", 1) && cJSON_AddStringToObject(request, "key", "D") &&
           cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 1));
    return request;
}

static cJSON *grow_request(const char *mode) {
    cJSON *request = motive_request(FIXTURE("motive_d_minor.txt"));
    assert(cJSON_AddStringToObject(request, "into", "sentence") && cJSON_AddStringToObject(request, "key", "D") &&
           cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 7));
    return request;
}

static cJSON *theme_request(const char *mode) {
    cJSON *request = load_json(FIXTURE("theme/one-motive.json"));
    rename_member(request, "mode", "mode", mode);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "seed");
    assert(cJSON_AddNumberToObject(request, "seed", 3));
    return request;
}

static cJSON *piece_request(const char *mode) {
    cJSON *wrapper = cJSON_CreateObject();
    cJSON *piece = load_json(FIXTURE("piece/structure-only.json"));
    rename_member(piece, "mode", "mode", mode);
    assert(wrapper && cJSON_AddItemToObject(wrapper, "request", piece) && cJSON_AddNumberToObject(wrapper, "seed", 1));
    return wrapper;
}

/* A request with "mode" and "material" together, in the object that holds them. */
static void add_both(cJSON *object, const char *material) {
    assert(cJSON_AddStringToObject(object, "material", material));
}

/* --- cases ------------------------------------------------------------------------------------------- */

static void test_requests_with_mode_and_material(void) {
    /* Every request kind: the old "mode" and the "material" member (an id, a name, a key word, an alias) give the same reply. */
    struct {
        const char *name;
        Entry entry;
        cJSON *(*make)(const char *mode);
        bool nested; /* the member is in the request's "request" object */
    } kinds[] = {{"random_motive", composer_api_random_motive, random_request, false},
                 {"plan", composer_api_plan, plan_request, false},
                 {"counterpoint", composer_api_counterpoint, counterpoint_request, false},
                 {"grow", composer_api_grow, grow_request, false},
                 {"theme", composer_api_theme, theme_request, false},
                 {"piece", composer_api_piece, piece_request, true}};
    for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        for (int minor = 0; minor < 2; minor++) {
            cJSON *request = kinds[k].make(minor ? "minor" : "major");
            cJSON *holder = kinds[k].nested ? cJSON_GetObjectItemCaseSensitive(request, "request") : request;
            if (k >= 2 && !minor) {
                /* the replies are the work of whole compositions: the slower kinds are compared in one mode each */
                cJSON_Delete(request);
                continue;
            }
            expect_same_replies(kinds[k].entry, request, holder, minor ? "minor" : "major", minor ? MINOR_WORDS : MAJOR_WORDS,
                                minor ? sizeof(MINOR_WORDS) / sizeof(MINOR_WORDS[0]) : sizeof(MAJOR_WORDS) / sizeof(MAJOR_WORDS[0]));
            cJSON_Delete(request);
        }
    }

    /* "random" is kept for the random motive: a seed draws the same material, from either member. */
    cJSON *random = random_request("random");
    const char *const random_words[] = {"random", "RANDOM"};
    expect_same_replies(composer_api_random_motive, random, random, "random", random_words, 2);
    cJSON_Delete(random);

    /* "style" where the request offers it: the grow brief without a style refuses it under either member, naming the member. */
    cJSON *request = grow_request("style");
    expect_refused(composer_api_grow, request, "/mode", "style needs style");
    rename_member(request, "mode", "material", NULL);
    expect_refused(composer_api_grow, request, "/material", "style needs style");
    cJSON_Delete(request);
}

static void test_both_members_refused(void) {
    struct {
        Entry entry;
        cJSON *(*make)(const char *mode);
        bool nested;
    } kinds[] = {{composer_api_random_motive, random_request, false}, {composer_api_plan, plan_request, false},
                 {composer_api_counterpoint, counterpoint_request, false}, {composer_api_grow, grow_request, false},
                 {composer_api_theme, theme_request, false}, {composer_api_piece, piece_request, true}};
    for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        cJSON *request = kinds[k].make("major");
        add_both(kinds[k].nested ? cJSON_GetObjectItemCaseSensitive(request, "request") : request, "major");
        expect_refused(kinds[k].entry, request, "/mode", "not both");
        cJSON_Delete(request);
    }
}

static void test_material_words(void) {
    /* A material word the Composer does not compose in is refused under "material"; under "mode" only major and minor are read. */
    cJSON *request = random_request("major");
    rename_member(request, "mode", "material", "dorian");
    expect_refused(composer_api_random_motive, request, "/material", "the Composer writes major and minor keys");
    cJSON_Delete(request);
    request = random_request("major");
    rename_member(request, "mode", "material", "omt.material.no-such-material");
    expect_refused(composer_api_random_motive, request, "/material", "material must be a material id, name, key word or alias");
    cJSON_Delete(request);
    request = random_request("dorian");
    expect_refused(composer_api_random_motive, request, "/mode", "mode must be major or minor");
    cJSON_Delete(request);
    request = plan_request("minor");
    rename_member(request, "key", "key", "D minor");
    expect_refused(composer_api_plan, request, "/mode", "names a mode already");
    cJSON_Delete(request);
    request = plan_request("minor");
    rename_member(request, "key", "key", "D minor");
    rename_member(request, "mode", "material", NULL);
    expect_refused(composer_api_plan, request, "/material", "names a mode already");
    cJSON_Delete(request);
}

/* A motive document's key reads "material" (an id, name, key word or alias) or the old "mode", and the two together are refused. */
static void test_motive_document_key(void) {
    cJSON *document = load_json(FIXTURE("motive_d_minor.json"));
    char *old_text = print(document);
    cJSON *input = cJSON_CreateObject();
    cJSON *request = cJSON_CreateObject();
    assert(input && request && cJSON_AddStringToObject(input, "format", "json") && cJSON_AddStringToObject(input, "text", old_text) &&
           cJSON_AddItemToObject(request, "input", input));
    ComposerResponse expected;
    assert(run(composer_api_motive, request, &expected) == COMPOSER_OK && expected.json);
    cJSON_Delete(request);
    free(old_text);

    static const char *const WORDS[] = {"minor", "omt.material.natural-minor", "natural minor"};
    for (size_t i = 0; i < sizeof(WORDS) / sizeof(WORDS[0]); i++) {
        cJSON *copy = cJSON_Duplicate(document, true);
        rename_member(cJSON_GetObjectItemCaseSensitive(copy, "key"), "mode", "material", WORDS[i]);
        char *text = print(copy);
        cJSON_Delete(copy);
        input = cJSON_CreateObject();
        request = cJSON_CreateObject();
        assert(input && request && cJSON_AddStringToObject(input, "format", "json") && cJSON_AddStringToObject(input, "text", text) &&
               cJSON_AddItemToObject(request, "input", input));
        ComposerResponse got;
        assert(run(composer_api_motive, request, &got) == COMPOSER_OK && same_reply(&expected, &got));
        composer_response_destroy(&got);
        cJSON_Delete(request);
        free(text);
    }

    /* both members, and a material the Composer does not compose in, are refused at the key */
    static const char *const BAD[][2] = {{"material", "minor"}, {"material", "dorian"}};
    for (size_t i = 0; i < 2; i++) {
        cJSON *copy = cJSON_Duplicate(document, true);
        cJSON *key = cJSON_GetObjectItemCaseSensitive(copy, "key");
        if (i == 0) {
            assert(cJSON_AddStringToObject(key, BAD[i][0], BAD[i][1]));
        } else {
            rename_member(key, "mode", "material", BAD[i][1]);
        }
        char *text = print(copy);
        cJSON_Delete(copy);
        input = cJSON_CreateObject();
        request = cJSON_CreateObject();
        assert(input && request && cJSON_AddStringToObject(input, "format", "json") && cJSON_AddStringToObject(input, "text", text) &&
               cJSON_AddItemToObject(request, "input", input));
        ComposerResponse response;
        ComposerStatus status = run(composer_api_motive, request, &response);
        assert(status != COMPOSER_OK && !response.json);
        assert(strstr(response.error.message, "key") != NULL || strstr(response.error.path, "key") != NULL);
        composer_response_destroy(&response);
        cJSON_Delete(request);
        free(text);
    }
    composer_response_destroy(&expected);
    cJSON_Delete(document);
}

/* --- plans ------------------------------------------------------------------------------------------- */

static char *replace_all(const char *text, const char *from, const char *to) {
    size_t from_length = strlen(from);
    size_t count = 0;
    for (const char *at = text; (at = strstr(at, from)) != NULL; at += from_length) {
        count++;
    }
    assert(count > 0);
    char *out = malloc(strlen(text) + count * (strlen(to) + 1) + 1);
    assert(out);
    char *write = out;
    const char *at = text;
    for (const char *found; (found = strstr(at, from)) != NULL; at = found + from_length) {
        memcpy(write, at, (size_t)(found - at));
        write += found - at;
        memcpy(write, to, strlen(to));
        write += strlen(to);
    }
    strcpy(write, at);
    return out;
}

/* The export of the plan text, as the reply the plan gives: its MIDI file and MusicXML. */
static void export_plan(const char *plan_text, ComposerResponse *response) {
    cJSON *request = cJSON_CreateObject();
    cJSON *plan = cJSON_Parse(plan_text);
    assert(request && plan && cJSON_AddItemToObject(request, "plan", plan) && cJSON_AddTrueToObject(request, "midi") &&
           cJSON_AddTrueToObject(request, "musicxml"));
    ComposerStatus status = run(composer_api_export, request, response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "export: %s (%s)\n", response->error.message, response->error.path);
    }
    assert(status == COMPOSER_OK);
    cJSON_Delete(request);
}

/* The plans of formats 1, 2, 3 and 4 are the same plan: read, written (always in format 4) and exported they are the same. */
static void test_plan_formats(void) {
    static const char *const FIXTURES[] = {"sentence_c_major.json", "sentence_c_to_g.json", "period_a_minor.json"};
    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), COMPOSER_FIXTURE_DIR "/%s", FIXTURES[i]);
        char *v1 = test_must_read_file(path, NULL);
        assert(strstr(v1, "\"schema_version\": 1") != NULL || strstr(v1, "\"schema_version\":1") != NULL);
        PiecePlan *read = NULL;
        PlanJsonError error;
        assert(plan_from_json(test_knowledge(), v1, &read, &error) == PLAN_OK);
        char *v4 = plan_to_json(read, test_plan_producer(), &error);
        assert(v4 && strstr(v4, "\"format_version\": 5") && strstr(v4, "\"mode\"") == NULL && strstr(v4, "omt.material.") != NULL);
        /* format 3 is the same text without the note ids */
        char *v3 = test_plan_text_as_format_three(v4);
        assert(v3 && strstr(v3, "\"format_version\": 3") && strstr(v3, "next_note_id") == NULL);

        /* format 2 names the mode: major or minor, where format 3 names the material */
        char *v2 = replace_all(v3, "\"format_version\": 3", "\"format_version\": 2");
        char *step = v2;
        if (strstr(step, "\"material\": \"omt.material.major\"")) {
            v2 = replace_all(step, "\"material\": \"omt.material.major\"", "\"mode\": \"major\"");
            free(step);
            step = v2;
        }
        if (strstr(step, "\"material\": \"omt.material.natural-minor\"")) {
            v2 = replace_all(step, "\"material\": \"omt.material.natural-minor\"", "\"mode\": \"minor\"");
            free(step);
        }
        assert(strstr(v2, "omt.material.") == NULL && strstr(v2, "\"mode\"") != NULL);

        ComposerResponse from_v1;
        ComposerResponse from_v2;
        ComposerResponse from_v3;
        ComposerResponse from_v4;
        export_plan(v1, &from_v1);
        export_plan(v2, &from_v2);
        export_plan(v3, &from_v3);
        export_plan(v4, &from_v4);
        assert(from_v1.midi_size > 14 && same_reply(&from_v1, &from_v2) && same_reply(&from_v1, &from_v3) && same_reply(&from_v1, &from_v4));
        composer_response_destroy(&from_v1);
        composer_response_destroy(&from_v2);
        composer_response_destroy(&from_v3);
        composer_response_destroy(&from_v4);

        /* every format writes back as the format 4 text (a format 1 to 3 file gets its note ids 1 to n in file order) */
        PiecePlan *again = NULL;
        assert(plan_from_json(test_knowledge(), v2, &again, &error) == PLAN_OK);
        char *written = plan_to_json(again, test_plan_producer(), &error);
        assert(written && strcmp(written, v4) == 0);
        free(written);
        plan_destroy(again);
        plan_destroy(read);
        free(v1);
        free(v2);
        free(v3);
        free(v4);
    }
}

/* The first key span of the plan named by `relation` and resolved: its index in the "keys" member of the plan object, or -1. */
static int resolved_span_of(const cJSON *plan, const char *relation) {
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(plan, "keys");
    int index = 0;
    const cJSON *span;
    cJSON_ArrayForEach(span, keys) {
        const cJSON *payload = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "composer"), "payload");
        const cJSON *named = cJSON_GetObjectItemCaseSensitive(payload, "relation");
        if (cJSON_IsString(named) && strcmp(named->valuestring, relation) == 0 && cJSON_GetObjectItemCaseSensitive(payload, "tonic")) {
            return index;
        }
        index++;
    }
    return -1;
}

/* The plan of a piece that changes key into the dominant, as text: the span in G major carries the relation beside its key. */
static char *dominant_plan(unsigned long *seed_out, int *span_out) {
    static const char *const FORMS[] = {"binary", "rounded-binary", "ternary"};
    for (size_t f = 0; f < sizeof(FORMS) / sizeof(FORMS[0]); f++) {
        for (unsigned long seed = 1; seed <= 10; seed++) {
            PieceRequest request;
            memset(&request, 0, sizeof(request));
            request.form = FORMS[f];
            request.key = test_key(0, "major");
            request.knowledge = test_knowledge();
            request.meter.beats_per_bar = 4;
            request.meter.beat_unit = 4;
            request.modulate = true;
            request.seed = seed;
            PiecePlan *plan = NULL;
            if (piece_build_plan(&request, &plan) != PIECE_OK) {
                continue;
            }
            PlanJsonError error;
            char *text = plan_to_json(plan, test_plan_producer(), &error);
            plan_destroy(plan);
            assert(text);
            cJSON *json = cJSON_Parse(text);
            assert(json);
            int span = resolved_span_of(json, "omt.relation.dominant");
            cJSON_Delete(json);
            if (span >= 0) {
                *seed_out = seed;
                *span_out = span;
                return text;
            }
            free(text);
        }
    }
    return NULL;
}

/* The plan text with the key span `span` named by its key alone, or by its relation and anchor alone. */
static char *plan_with_span_named(const char *text, int span, bool by_relation) {
    cJSON *json = cJSON_Parse(text);
    assert(json);
    cJSON *keys = cJSON_GetObjectItemCaseSensitive(json, "keys");
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(keys, span), "composer"), "payload");
    assert(payload);
    if (by_relation) {
        assert(cJSON_GetObjectItemCaseSensitive(payload, "anchor"));
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "tonic");
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "material");
    } else {
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "relation");
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "anchor");
    }
    char *out = cJSON_Print(json);
    assert(out);
    cJSON_Delete(json);
    return out;
}

/* One fill of the plan text from a seed: its MIDI file and MusicXML. */
static ComposerStatus fill_plan(const char *plan_text, unsigned long seed, ComposerResponse *response) {
    cJSON *request = cJSON_CreateObject();
    cJSON *plan = cJSON_Parse(plan_text);
    assert(request && plan && cJSON_AddItemToObject(request, "plan", plan) && cJSON_AddNumberToObject(request, "seed", (double)seed) &&
           cJSON_AddTrueToObject(request, "midi") && cJSON_AddTrueToObject(request, "musicxml"));
    ComposerStatus status = run(composer_api_fill, request, response);
    cJSON_Delete(request);
    return status;
}

/*
 * A v3 plan whose key span is {relation: omt.relation.dominant, anchor: home} is resolved before it is composed, and composes the
 * same notes (MIDI and MusicXML) as the plan whose span is {tonic: G, material: omt.material.major} (C major home).
 */
static void test_relation_named_span_composes(void) {
    unsigned long seed = 0;
    int span = -1;
    char *planned = dominant_plan(&seed, &span);
    assert(planned);
    cJSON *json = cJSON_Parse(planned);
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(json, "keys"), span), "composer"), "payload");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(payload, "tonic")->valuestring, "G") == 0 &&
           strcmp(cJSON_GetObjectItemCaseSensitive(payload, "material")->valuestring, "omt.material.major") == 0 &&
           strcmp(cJSON_GetObjectItemCaseSensitive(payload, "anchor")->valuestring, "home") == 0);
    cJSON_Delete(json);

    char *by_key = plan_with_span_named(planned, span, false);
    char *by_relation = plan_with_span_named(planned, span, true);
    assert(strstr(by_key, "\"relation\"") == NULL && strstr(by_relation, "\"tonic\": \"G\"") == NULL);

    /* the relation-named plan is read unresolved, and written resolved once it is composed from */
    PiecePlan *read = NULL;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), by_relation, &read, &error) == PLAN_OK);
    plan_destroy(read);
    assert(plan_from_json_resolved(test_knowledge(), by_relation, &read, &error) == PLAN_OK);
    plan_destroy(read);

    ComposerResponse named;
    ComposerStatus status = COMPOSER_ERR_UNSATISFIABLE;
    for (unsigned long tried = seed; tried < seed + 8 && status != COMPOSER_OK; tried++) {
        status = fill_plan(by_key, tried, &named);
        if (status != COMPOSER_OK) {
            assert(status == COMPOSER_ERR_UNSATISFIABLE);
            composer_response_destroy(&named);
        } else {
            seed = tried;
        }
    }
    assert(status == COMPOSER_OK && named.midi_size > 14 && named.musicxml);
    ComposerResponse by_name;
    assert(fill_plan(by_relation, seed, &by_name) == COMPOSER_OK);
    assert(by_name.midi_size == named.midi_size && memcmp(by_name.midi, named.midi, named.midi_size) == 0);
    assert(same_text(by_name.musicxml, named.musicxml));
    composer_response_destroy(&by_name);
    composer_response_destroy(&named);

    /* a relation that cannot resolve is refused by the entry that composes, with the relation's message, and nothing is composed */
    char *unknown = replace_all(by_relation, "omt.relation.dominant", "omt.relation.no-such-relation");
    ComposerResponse refused;
    assert(fill_plan(unknown, seed, &refused) != COMPOSER_OK);
    assert(!refused.midi && !refused.json && refused.error.message[0] != '\0');
    composer_response_destroy(&refused);
    free(unknown);

    free(by_relation);
    free(by_key);
    free(planned);
}

int main(void) {
    test_requests_with_mode_and_material();
    test_both_members_refused();
    test_material_words();
    test_motive_document_key();
    test_plan_formats();
    test_relation_named_span_composes();
    return 0;
}
