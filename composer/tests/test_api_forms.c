#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"

/*
 * The forms entry (docs/api/vocabulary-and-validation.md, R-428): the seven built-in forms in registry order, the sizes and
 * sections of the ones the documents give, and the roles, loop and ending of the game loop. The response is written beside
 * the test, for the schema test.
 */

static cJSON *forms;

static const cJSON *member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    assert(item);
    return item;
}

static const char *text(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static int number(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    assert(cJSON_IsNumber(item));
    return item->valueint;
}

static bool flag(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    assert(cJSON_IsBool(item));
    return cJSON_IsTrue(item);
}

static const cJSON *form_named(const char *name) {
    const cJSON *form = NULL;
    cJSON_ArrayForEach(form, member(forms, "forms")) {
        if (strcmp(text(form, "name"), name) == 0) {
            return form;
        }
    }
    fprintf(stderr, "no form %s\n", name);
    assert(0);
    return NULL;
}

static const cJSON *size_with_bars(const cJSON *form, int bars) {
    const cJSON *size = NULL;
    cJSON_ArrayForEach(size, member(form, "sizes")) {
        if (number(size, "bars") == bars) {
            return size;
        }
    }
    assert(0);
    return NULL;
}

static const cJSON *default_size(const cJSON *form) {
    const cJSON *size = NULL;
    cJSON_ArrayForEach(size, member(form, "sizes")) {
        if (flag(size, "default")) {
            return size;
        }
    }
    assert(0);
    return NULL;
}

static void assert_strings(const cJSON *list, const char *const *expected, size_t count) {
    assert(cJSON_IsArray(list));
    assert((size_t)cJSON_GetArraySize(list) == count);
    for (size_t i = 0; i < count; i++) {
        assert(strcmp(cJSON_GetArrayItem(list, (int)i)->valuestring, expected[i]) == 0);
    }
}

static void assert_choice(const cJSON *object, const char *name, const char *value, const char *label) {
    const cJSON *choice = member(object, name);
    assert(strcmp(text(choice, "value"), value) == 0);
    assert(strcmp(text(choice, "label"), label) == 0);
}

static void test_registry_order(void) {
    const char *const names[] = {"binary", "rounded-binary", "ternary", "variations", "loop", "layered", "game-loop"};
    const cJSON *list = member(forms, "forms");
    assert(cJSON_GetArraySize(list) == 7);
    for (int i = 0; i < 7; i++) {
        const cJSON *form = cJSON_GetArrayItem(list, i);
        assert(strcmp(text(form, "name"), names[i]) == 0);
        assert(*text(form, "label"));
        bool game_loop = i == 6;
        assert(strcmp(text(form, "sections"), game_loop ? "request" : "sizes") == 0);
        assert(flag(form, "closing") == (i < 4)); /* the four with sections that end in the home key close */
        assert((cJSON_GetArraySize(member(form, "sizes")) == 0) == game_loop);
    }
}

static void test_rounded_binary(void) {
    const cJSON *form = form_named("rounded-binary");
    assert(strcmp(text(form, "label"), "Rounded binary") == 0);
    assert(cJSON_GetArraySize(member(form, "sizes")) == 2);
    const char *const regions[] = {"away"};
    assert_strings(member(form, "regions"), regions, 1);
    const cJSON *versions = member(form, "versions");
    assert(cJSON_GetArraySize(versions) == 1);
    assert(strcmp(text(cJSON_GetArrayItem(versions, 0), "value"), "piece") == 0);
    assert(strcmp(text(cJSON_GetArrayItem(versions, 0), "label"), "Piece") == 0);
    assert(!flag(cJSON_GetArrayItem(versions, 0), "loop"));
    const cJSON *settings = member(form, "settings");
    assert(flag(settings, "modulate"));
    assert(strcmp(text(settings, "accompaniment"), "optional") == 0);
    assert(!flag(settings, "voicing") && !flag(settings, "section_fills"));
    assert(cJSON_GetArraySize(member(form, "roles")) == 0 && cJSON_GetArraySize(member(form, "loop_roles")) == 0);
    assert(cJSON_IsNull(member(form, "cadences")) && cJSON_IsNull(member(form, "default_ending")));

    /* docs/forms.md "The built-in forms": A 8 (presentation + continuation, PAC, closed), B 4 or 8, A' 4 or 8. */
    const cJSON *size = default_size(form);
    assert(number(size, "bars") == 16 && number(size, "count") == 0);
    assert(size_with_bars(form, 24) != size);
    const cJSON *sections = member(size, "sections");
    assert(cJSON_GetArraySize(sections) == 3);
    const cJSON *a = cJSON_GetArrayItem(sections, 0);
    const cJSON *b = cJSON_GetArrayItem(sections, 1);
    const cJSON *again = cJSON_GetArrayItem(sections, 2);
    assert(strcmp(text(a, "name"), "A") == 0 && strcmp(text(a, "letter"), "A") == 0);
    assert_choice(a, "role", "theme", "Theme");
    assert(number(a, "bars") == 8 && cJSON_IsNull(member(a, "beats")));
    assert_choice(a, "beginning", "presentation", "Presentation");
    assert_choice(a, "ending", "continuation", "Continuation");
    const cJSON *cadence = member(a, "cadence");
    assert(strcmp(text(cadence, "value"), "perfect-authentic") == 0 && *text(cadence, "label"));
    assert(strcmp(text(cadence, "core"), "PAC") == 0);
    assert(flag(a, "closed"));
    assert(strcmp(text(member(a, "keys"), "start"), "home") == 0 && strcmp(text(member(a, "keys"), "end"), "away") == 0);
    assert(cJSON_IsNull(member(a, "motive")) && cJSON_IsNull(member(a, "returns")) && cJSON_IsNull(member(a, "variation")));
    assert(strcmp(text(a, "layers"), "all") == 0);
    const cJSON *slots = member(a, "slots");
    assert(cJSON_GetArraySize(slots) == 3);
    const char *const basic[] = {"basic idea", "repetition"};
    const char *const continuation[] = {"fragment 1", "fragment 2"};
    const char *const cadential[] = {"cadential"};
    assert(strcmp(text(cJSON_GetArrayItem(slots, 0), "slot"), "basic") == 0);
    assert_strings(member(cJSON_GetArrayItem(slots, 0), "ideas"), basic, 2);
    assert(strcmp(text(cJSON_GetArrayItem(slots, 1), "slot"), "continuation") == 0);
    assert_strings(member(cJSON_GetArrayItem(slots, 1), "ideas"), continuation, 2);
    assert(strcmp(text(cJSON_GetArrayItem(slots, 2), "slot"), "cadential") == 0);
    assert_strings(member(cJSON_GetArrayItem(slots, 2), "ideas"), cadential, 1);

    assert_choice(b, "role", "contrast", "Contrast");
    assert(number(b, "bars") == 4);
    assert_choice(b, "beginning", "compound_basic_idea", "Compound basic idea");
    assert(strcmp(text(member(b, "cadence"), "core"), "HC") == 0);
    assert(strcmp(text(b, "motive"), "invert tonal=1") == 0);
    assert(strcmp(text(member(b, "keys"), "start"), "away") == 0 && strcmp(text(member(b, "keys"), "end"), "home") == 0);

    assert_choice(again, "role", "return", "Return");
    assert(strcmp(text(again, "returns"), "A") == 0 && strcmp(text(again, "variation"), "exact") == 0);
    assert(cJSON_IsNull(member(again, "motive")));

    const cJSON *longer = member(size_with_bars(form, 24), "sections");
    assert(cJSON_GetArraySize(longer) == 3);
    assert(number(cJSON_GetArrayItem(longer, 1), "bars") == 8 && number(cJSON_GetArrayItem(longer, 2), "bars") == 8);
}

static void test_variations(void) {
    const cJSON *form = form_named("variations");
    const cJSON *size = default_size(form);
    assert(number(size, "count") == 3);
    const cJSON *sections = member(size, "sections");
    const char *const names[] = {"theme", "variation 1", "variation 2", "variation 3"};
    assert(cJSON_GetArraySize(sections) == 4);
    for (int i = 0; i < 4; i++) {
        assert(strcmp(text(cJSON_GetArrayItem(sections, i), "name"), names[i]) == 0);
    }
    assert_choice(cJSON_GetArrayItem(sections, 0), "role", "theme", "Theme");
    assert_choice(cJSON_GetArrayItem(sections, 1), "role", "variation", "Variation");
    assert(strcmp(text(cJSON_GetArrayItem(sections, 1), "returns"), "theme") == 0);
    assert(strcmp(text(cJSON_GetArrayItem(sections, 1), "variation"), "ornament") == 0);
    assert(strcmp(text(cJSON_GetArrayItem(sections, 2), "variation"), "reharmonize") == 0);
    assert(cJSON_GetArraySize(member(form, "sizes")) > 3);
}

static void test_layered(void) {
    const cJSON *form = form_named("layered");
    assert(strcmp(text(member(form, "settings"), "accompaniment"), "required") == 0);
    assert(!flag(member(form, "settings"), "modulate"));
    const cJSON *sections = member(default_size(form), "sections");
    assert(cJSON_GetArraySize(sections) == 4);
    assert_choice(cJSON_GetArrayItem(sections, 0), "role", "layer", "Layer");
    const char *const bass[] = {"bass"};
    assert_strings(member(cJSON_GetArrayItem(sections, 0), "layers"), bass, 1);
    const char *const all[] = {"bass", "inner_voice", "melody"};
    assert_strings(member(cJSON_GetArrayItem(sections, 3), "layers"), all, 3);
}

/* Ternary's B is written over a dominant pedal at both sizes (R-446); no other section of any form names one. */
static void test_pedal(void) {
    const cJSON *list = member(forms, "forms");
    const cJSON *form = NULL;
    size_t pedals = 0;
    cJSON_ArrayForEach(form, list) {
        const cJSON *size = NULL;
        cJSON_ArrayForEach(size, member(form, "sizes")) {
            const cJSON *section = NULL;
            cJSON_ArrayForEach(section, member(size, "sections")) {
                const cJSON *pedal = member(section, "pedal");
                if (strcmp(text(form, "name"), "ternary") == 0 && strcmp(text(section, "name"), "B") == 0) {
                    assert_choice(section, "pedal", "dominant", "Dominant pedal");
                    pedals++;
                } else {
                    assert(cJSON_IsNull(pedal));
                }
            }
        }
    }
    assert((int)pedals == cJSON_GetArraySize(member(form_named("ternary"), "sizes")) && pedals >= 2);
}

static void test_game_loop(void) {
    const cJSON *form = form_named("game-loop");
    assert(strcmp(text(form, "sections"), "request") == 0 && !flag(form, "closing"));
    assert(cJSON_GetArraySize(member(form, "sizes")) == 0 && cJSON_GetArraySize(member(form, "regions")) == 0);
    const cJSON *versions = member(form, "versions");
    assert(cJSON_GetArraySize(versions) == 2);
    const cJSON *loop = cJSON_GetArrayItem(versions, 0);
    const cJSON *ending = cJSON_GetArrayItem(versions, 1);
    assert(strcmp(text(loop, "value"), "loop") == 0 && strcmp(text(loop, "label"), "Loop version") == 0 && flag(loop, "loop"));
    assert(!cJSON_GetObjectItemCaseSensitive(loop, "after"));
    assert(strcmp(text(ending, "value"), "ending") == 0 && strcmp(text(ending, "label"), "Ending version") == 0);
    assert(!flag(ending, "loop") && strcmp(text(ending, "after"), "loop") == 0);
    const cJSON *settings = member(form, "settings");
    assert(!flag(settings, "modulate") && flag(settings, "voicing") && flag(settings, "section_fills"));
    assert(strcmp(text(settings, "accompaniment"), "optional") == 0);

    const char *const roles[] = {"intro", "body", "turnaround", "ending"};
    const char *const labels[] = {"Intro", "Loop body", "Turnaround", "Ending"};
    const cJSON *list = member(form, "roles");
    assert(cJSON_GetArraySize(list) == 4);
    for (int i = 0; i < 4; i++) {
        const cJSON *role = cJSON_GetArrayItem(list, i);
        assert(strcmp(text(role, "value"), roles[i]) == 0 && strcmp(text(role, "label"), labels[i]) == 0);
        assert(flag(role, "required") == (i == 1));
    }
    const char *const loop_roles[] = {"body", "turnaround"};
    assert_strings(member(form, "loop_roles"), loop_roles, 2);

    const cJSON *cadences = member(form, "cadences");
    assert(strcmp(text(cadences, "default"), "IAC") == 0);
    const cJSON *last = member(cadences, "last");
    assert(cJSON_GetArraySize(last) == 3);
    const char *const ending_role[] = {"ending"};
    assert_strings(member(cJSON_GetArrayItem(last, 0), "roles"), ending_role, 1);
    assert(strcmp(text(cJSON_GetArrayItem(last, 0), "cadence"), "PAC") == 0);

    const cJSON *default_ending = member(form, "default_ending");
    assert(strcmp(text(default_ending, "id"), "end") == 0 && strcmp(text(default_ending, "letter"), "E") == 0);
    assert(strcmp(text(default_ending, "role"), "ending") == 0 && number(default_ending, "bars") == 2);
    assert(strcmp(text(default_ending, "cadence"), "PAC") == 0);
}

int main(void) {
    ComposerResponse response;
    assert(composer_api_forms(&response) == COMPOSER_OK);
    assert(response.json);
    test_write_file("api_forms_response_core.json", response.json);
    forms = cJSON_Parse(response.json);
    assert(forms);
    composer_response_destroy(&response);

    test_registry_order();
    test_rounded_binary();
    test_variations();
    test_layered();
    test_pedal();
    test_game_loop();
    cJSON_Delete(forms);
    printf("test_api_forms: ok\n");
    return 0;
}
