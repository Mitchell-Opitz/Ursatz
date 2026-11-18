#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Per-section controls in a piece request (R-610, R-611): a sized form's overlays give a section a region, a motive of its own or a
 * composed theme; a game loop section gives a region. The R-611 ternary composes with A in C minor and B in E flat major, one theme
 * composed in each mode; the keys it refuses name P69; an overlay that names no section, a section twice, a fill on a section that
 * returns, an anchor that is not before the section are refused at their paths.
 */

#define THEME_SEED 1
#define PIECE_SEED 1 /* the first seed of 1 to 16 that composes each request below; recorded */

static char *printed(cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    assert(text);
    return text;
}

/* The plan of the one-motive theme of the given kind and mode in C (fixtures/theme/one-motive.json), as a parsed JSON object. */
static cJSON *theme_plan(const char *into, const char *mode) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    cJSON_ReplaceItemInObjectCaseSensitive(request, "into", cJSON_CreateString(into));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "key", cJSON_CreateString("C"));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "mode", cJSON_CreateString(mode));
    assert(cJSON_AddNumberToObject(request, "seed", THEME_SEED));
    char *body = printed(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), body, NULL, &response) == COMPOSER_OK);
    free(body);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    return plan;
}

static cJSON *sized_request(const char *form, cJSON *sections) {
    cJSON *piece = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(piece, "schema", "ursatz-piece-request") && cJSON_AddNumberToObject(piece, "schema_version", 2) &&
           cJSON_AddStringToObject(piece, "form", form) && cJSON_AddStringToObject(piece, "key", "C") &&
           cJSON_AddStringToObject(piece, "material", "minor") && cJSON_AddStringToObject(piece, "meter", "4/4"));
    if (sections) {
        assert(cJSON_AddItemToObject(piece, "sections", sections));
    }
    return piece;
}

static cJSON *wrap(cJSON *piece, unsigned seed) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "request", piece) && cJSON_AddNumberToObject(request, "seed", seed));
    return request;
}

static cJSON *region_key(const char *tonic, const char *material) {
    cJSON *region = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(region, "tonic", tonic) && cJSON_AddStringToObject(region, "material", material));
    return region;
}

static cJSON *region_relation(const char *relation, const char *anchor) {
    cJSON *region = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(region, "relation", relation) && cJSON_AddStringToObject(region, "anchor", anchor));
    return region;
}

static cJSON *overlay(const char *name, cJSON *region, cJSON *fill) {
    cJSON *item = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(item, "name", name));
    if (region) {
        assert(cJSON_AddItemToObject(item, "region", region));
    }
    if (fill) {
        assert(cJSON_AddItemToObject(item, "fill", fill));
    }
    return item;
}

static cJSON *theme_fill(cJSON *plan) {
    cJSON *fill = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(fill, "kind", "theme") && cJSON_AddItemToObject(fill, "plan", plan));
    return fill;
}

static cJSON *own_motif(void) {
    cJSON *fill = cJSON_CreateObject();
    cJSON *motive = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(motive, "degrees", "1:e 3:e 5:e 3:e 1:e 3:e 5:e 1':e") && cJSON_AddStringToObject(motive, "pickup", "0") &&
           cJSON_AddNullToObject(motive, "harmony") && cJSON_AddArrayToObject(motive, "features") &&
           cJSON_AddStringToObject(motive, "role", "melodic"));
    assert(cJSON_AddStringToObject(fill, "kind", "motif") && cJSON_AddItemToObject(fill, "motive", motive));
    return fill;
}

static ComposerStatus run(cJSON *request, ComposerResponse *response) {
    char *text = printed(request);
    ComposerStatus status = composer_api_piece(test_context(), text, NULL, response);
    free(text);
    cJSON_Delete(request);
    return status;
}

static void expect_refusal(cJSON *request, const char *path, const char *message) {
    ComposerResponse response;
    ComposerStatus status = run(request, &response);
    if (status != COMPOSER_ERR_REQUEST || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "'%s' at '%s' (status %d), expected '%s' at '%s'\n", response.error.message, response.error.path, (int)status,
                message, path);
    }
    assert(status == COMPOSER_ERR_REQUEST && strcmp(response.error.path, path) == 0 && strstr(response.error.message, message));
    composer_response_destroy(&response);
}

/* --- reading the plan --------------------------------------------------------------------------------------------- */

static double time_of(const cJSON *rational) {
    return cJSON_GetObjectItemCaseSensitive(rational, "num")->valuedouble / cJSON_GetObjectItemCaseSensitive(rational, "den")->valuedouble;
}

static const cJSON *section_named(const cJSON *plan, const char *name) {
    const cJSON *node;
    cJSON_ArrayForEach(node, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(plan, "form"), "children")) {
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(node, "label");
        if (cJSON_IsString(label) && strcmp(label->valuestring, name) == 0) {
            return node;
        }
    }
    return NULL;
}

/* The pitches of the melody lane's notes inside [from, to), in time order, each moved by `shift` semitones; at most `capacity`. */
static size_t melody_pitches(const cJSON *plan, double from, double to, int shift, int *out, size_t capacity) {
    double at[512];
    size_t count = 0;
    const cJSON *span;
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "melody")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
        if (cJSON_GetObjectItemCaseSensitive(composer, "voice")->valueint != 0) {
            continue;
        }
        double start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "span"), "start"));
        const cJSON *note;
        cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "notes")) {
            double when = start + time_of(cJSON_GetObjectItemCaseSensitive(note, "offset"));
            if (when < from - 1e-9 || when >= to - 1e-9 || count >= capacity || count >= 512) {
                continue;
            }
            size_t place = count++;
            while (place > 0 && at[place - 1] > when) {
                at[place] = at[place - 1];
                out[place] = out[place - 1];
                place--;
            }
            at[place] = when;
            out[place] = cJSON_GetObjectItemCaseSensitive(note, "midi_pitch")->valueint + shift;
        }
    }
    return count;
}

/* The key spans of the plan in time order: the key lane entries. */
static const cJSON *key_span(const cJSON *plan, size_t index) {
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(plan, "keys"), (int)index);
}

static const char *key_text(const cJSON *span, const char *member) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "composer"), "payload"), member);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* --- the R-611 ternary -------------------------------------------------------------------------------------------- */

static void test_ternary_regions(void) {
    cJSON *minor = theme_plan("period", "minor");
    cJSON *major = theme_plan("sentence", "major");
    cJSON *minor_copy = cJSON_Duplicate(minor, true);
    cJSON *major_copy = cJSON_Duplicate(major, true);
    cJSON *sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_key("C", "minor"), theme_fill(minor))));
    assert(cJSON_AddItemToArray(sections, overlay("B", region_relation("mediant", "A"), theme_fill(major))));
    ComposerResponse response;
    ComposerStatus status = run(wrap(sized_request("ternary", sections), PIECE_SEED), &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "ternary with overlays: %s\n", response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);

    /* A in C minor, B in E flat major reached by the mediant of A, A return in C minor again. */
    assert(strcmp(key_text(key_span(plan, 0), "tonic"), "C") == 0 && strstr(key_text(key_span(plan, 0), "material"), "natural-minor"));
    const cJSON *b_key = key_span(plan, 1);
    assert(strcmp(key_text(b_key, "tonic"), "Eb") == 0 && strstr(key_text(b_key, "material"), "major") != NULL);
    assert(strcmp(key_text(b_key, "relation"), "omt.relation.mediant") == 0 && strcmp(key_text(b_key, "anchor"), "A") == 0);
    assert(strcmp(key_text(key_span(plan, 2), "tonic"), "C") == 0);

    /* A's melody is the minor theme's, B's the major theme's moved from C major to E flat major. */
    const cJSON *a = section_named(plan, "A");
    const cJSON *b = section_named(plan, "B");
    assert(a && b);
    double a_start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(a, "span"), "start"));
    double b_start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b, "span"), "start"));
    int theme[512];
    int got[512];
    size_t theme_count = melody_pitches(minor_copy, 0, 8, 0, theme, 512);
    size_t got_count = melody_pitches(plan, a_start, a_start + 8, 0, got, 512);
    assert(theme_count > 0 && got_count == theme_count && memcmp(theme, got, theme_count * sizeof(int)) == 0);
    theme_count = melody_pitches(major_copy, 0, 8, 3, theme, 512);
    got_count = melody_pitches(plan, b_start, b_start + 8, 0, got, 512);
    assert(theme_count > 0 && got_count == theme_count && memcmp(theme, got, theme_count * sizeof(int)) == 0);

    /* The section descriptions carry the theme's pairing, and A return is a varied return of A. */
    const cJSON *b_section = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b, "composer"), "section");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(b_section, "beginning")->valuestring, "presentation") == 0);
    const cJSON *again = section_named(plan, "A return");
    const cJSON *again_section = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(again, "composer"), "section");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(again_section, "returns")->valuestring, "A") == 0 &&
           strcmp(cJSON_GetObjectItemCaseSensitive(again_section, "variation")->valuestring, "varied") == 0);
    cJSON_Delete(plan);
    cJSON_Delete(minor_copy);
    cJSON_Delete(major_copy);
}

/* `previous` names the section before: B in the mediant of A, which is the section before it. */
static void test_previous_anchor(void) {
    cJSON *minor = theme_plan("period", "minor");
    cJSON *major = theme_plan("sentence", "major");
    cJSON *sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_key("C", "minor"), theme_fill(minor))));
    assert(cJSON_AddItemToArray(sections, overlay("B", region_relation("mediant", "previous"), theme_fill(major))));
    ComposerResponse response;
    assert(run(wrap(sized_request("ternary", sections), PIECE_SEED), &response) == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan && strcmp(key_text(key_span(plan, 1), "tonic"), "Eb") == 0 && strcmp(key_text(key_span(plan, 1), "anchor"), "previous") == 0);
    cJSON_Delete(plan);
}

/* A motif overlay whose motive is not the piece's gives the section a motive node of its own. */
static void test_own_motive(void) {
    cJSON *sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("B", NULL, own_motif())));
    ComposerResponse response;
    ComposerStatus status = run(wrap(sized_request("ternary", sections), PIECE_SEED), &response);
    assert(status == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan && strstr(cJSON_PrintUnformatted(plan), "\"label\":\"own B\"") != NULL);
    const cJSON *b = section_named(plan, "B");
    const cJSON *constraints = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b, "composer"), "constraints");
    bool named = false;
    const cJSON *constraint;
    cJSON_ArrayForEach(constraint, constraints) {
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(constraint, "kind");
        if (cJSON_IsString(kind) && strcmp(kind->valuestring, "motive_ref") == 0) {
            named = cJSON_GetObjectItemCaseSensitive(constraint, "step") == NULL; /* the motive itself, not m0 made over */
        }
    }
    assert(named);
    cJSON_Delete(plan);
}

/* A game loop with a body section in the dominant: its key span names the relation and home, and the seam takes its key. */
static void test_game_loop_region(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/piece/loop-and-ending.json", NULL);
    cJSON *piece = cJSON_Parse(text);
    free(text);
    assert(piece);
    cJSON_ReplaceItemInObjectCaseSensitive(piece, "style", cJSON_CreateNull());
    cJSON_ReplaceItemInObjectCaseSensitive(piece, "mode", cJSON_CreateString("minor"));
    cJSON *sections = cJSON_GetObjectItemCaseSensitive(piece, "sections");
    for (int i = 1; i <= 2; i++) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, i), "fill");
        cJSON_ReplaceItemInObjectCaseSensitive(fill, "into", cJSON_CreateString("period")); /* a key opens on a phrase model */
    }
    cJSON *body = cJSON_GetArrayItem(sections, 1);
    assert(cJSON_AddItemToObject(body, "region", region_relation("dominant", "home")));
    assert(cJSON_AddItemToObject(cJSON_GetArrayItem(sections, 2), "region", cJSON_CreateString("home")));
    ComposerResponse response;
    ComposerStatus status = run(wrap(piece, PIECE_SEED), &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "game loop with a region: %s\n", response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    assert(strcmp(key_text(key_span(plan, 0), "tonic"), "D") == 0 && strcmp(key_text(key_span(plan, 1), "tonic"), "A") == 0);
    assert(strcmp(key_text(key_span(plan, 1), "relation"), "omt.relation.dominant") == 0 && strcmp(key_text(key_span(plan, 1), "anchor"), "home") == 0);
    cJSON_Delete(plan);
}

/* --- refusals ------------------------------------------------------------------------------------------------------ */

static void test_refusals(void) {
    /* A in C locrian: named at A with P69; A in C minor and B two steps brighter: C mixolydian, named at B. */
    cJSON *sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_key("C", "locrian"), NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/0/region", "until P69");
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_key("C", "minor"), NULL)));
    assert(cJSON_AddItemToArray(sections, overlay("B", region_relation("two steps brighter", "A"), NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/1/region", "C mixolydian");
    /* the names and the anchors */
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("Z", NULL, NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/0/name", "no section 'Z' in the ternary form");
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("B", NULL, NULL)));
    assert(cJSON_AddItemToArray(sections, overlay("B", NULL, NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/1/name", "given twice");
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_relation("dominant", "B"), NULL)));
    assert(cJSON_AddItemToArray(sections, overlay("B", NULL, NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/0/region", "must be an earlier section");
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A", region_relation("dominant", "previous"), NULL)));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/0/region", "no previous region");
    /* a fill on a section that returns another goes to the section it returns */
    sections = cJSON_CreateArray();
    assert(cJSON_AddItemToArray(sections, overlay("A return", NULL, own_motif())));
    expect_refusal(wrap(sized_request("ternary", sections), 1), "/request/sections/0/fill", "returns 'A': give it to 'A'");
}

int main(void) {
    test_ternary_regions();
    test_previous_anchor();
    test_own_motive();
    test_game_loop_region();
    test_refusals();
    return 0;
}
