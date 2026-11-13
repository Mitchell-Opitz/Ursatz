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
    const char *const names[] = {"binary", "rounded-binary", "ternary", "variations", "loop", "layered", "minuet",
                                 "trio", "minuet-and-trio", "scherzo", "scherzo-and-trio", "figured-variations", "game-loop"};
    const cJSON *list = member(forms, "forms");
    assert(cJSON_GetArraySize(list) == 13);
    for (int i = 0; i < 13; i++) {
        const cJSON *form = cJSON_GetArrayItem(list, i);
        assert(strcmp(text(form, "name"), names[i]) == 0);
        assert(*text(form, "label"));
        bool game_loop = i == 12;
        assert(strcmp(text(form, "sections"), game_loop ? "request" : "sizes") == 0);
        /* The four with sections that end in the home key close, and so do the minuet, the trio, the scherzo, the
           forms that put two together and the figured variations. */
        assert(flag(form, "closing") == (i < 4 || (i >= 6 && i < 12)));
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
    assert(flag(settings, "modulate") && !flag(form, "modulates"));
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

/* The figured variations (R-500): 40 and 56 bars, 2/4 first of four meters, the theme, five figurate variations, the coda. */
static void test_figured_variations(void) {
    const cJSON *form = form_named("figured-variations");
    assert(strcmp(text(form, "label"), "Figured variations") == 0 && strcmp(text(form, "sections"), "sizes") == 0 && flag(form, "closing"));
    const char *const meters[] = {"2/4", "3/4", "4/4", "6/8"};
    assert_strings(member(form, "meters"), meters, 4);
    assert(cJSON_GetArraySize(member(form, "regions")) == 0 && cJSON_IsNull(member(form, "tempo")));
    assert(cJSON_GetArraySize(member(form, "sizes")) == 2);
    const cJSON *size = default_size(form);
    assert(number(size, "bars") == 56 && number(size, "count") == 5);
    const cJSON *sections = member(size, "sections");
    const char *const names[] = {"theme", "variation 1", "variation 2", "variation 3", "variation 4", "variation 5", "C1", "C2"};
    assert(cJSON_GetArraySize(sections) == 8);
    for (int i = 0; i < 8; i++) {
        const cJSON *section = cJSON_GetArrayItem(sections, i);
        assert(strcmp(text(section, "name"), names[i]) == 0);
        assert(number(section, "bars") == (i < 6 ? 8 : 4));
        const bool variation = i >= 1 && i <= 5;
        assert_choice(section, "role", i == 0 ? "theme" : variation ? "variation" : "codetta", i == 0 ? "Theme" : variation ? "Variation" : "Codetta");
        if (variation) {
            assert(strcmp(text(section, "returns"), "theme") == 0 && strcmp(text(section, "variation"), "figurate") == 0);
        } else {
            assert(cJSON_IsNull(member(section, "returns")) && cJSON_IsNull(member(section, "variation")));
        }
    }
    const cJSON *small = size_with_bars(form, 40);
    assert(number(small, "count") == 3 && cJSON_GetArraySize(member(small, "sections")) == 6);
    assert(strcmp(text(cJSON_GetArrayItem(member(small, "sections"), 3), "variation"), "figurate") == 0);
    /* The figure and the mode of each section (R-501). */
    const char *const kinds[] = {"neighbor", "neighbor", "scale", "scale", "broken-chord"};
    const char *const rhythms[] = {"eighths", "sixteenths", "eighths", "sixteenths", "sixteenths"};
    for (int i = 0; i < 8; i++) {
        const cJSON *section = cJSON_GetArrayItem(sections, i);
        const cJSON *figure = member(section, "figure");
        if (i < 1 || i > 5) {
            assert(cJSON_IsNull(figure));
        } else {
            assert(cJSON_IsObject(figure) && strcmp(text(figure, "kind"), kinds[i - 1]) == 0 &&
                   strcmp(text(figure, "rhythm"), rhythms[i - 1]) == 0);
            assert(strcmp(text(figure, "harmony"), i == 4 ? "reharmonized" : "kept") == 0 || i == 3);
            assert(i == 4 ? number(figure, "change_at") == 3 : cJSON_IsNull(member(figure, "change_at")));
            assert(flag(figure, "climax") == (i == 5));
        }
        const cJSON *mode = member(section, "mode");
        assert(i == 3 ? (cJSON_IsString(mode) && strcmp(mode->valuestring, "parallel") == 0) : cJSON_IsNull(mode));
    }
    const cJSON *small_sections = member(small, "sections");
    assert(strcmp(text(member(cJSON_GetArrayItem(small_sections, 2), "figure"), "kind"), "scale") == 0 &&
           strcmp(text(cJSON_GetArrayItem(small_sections, 2), "mode"), "parallel") == 0);
    assert(flag(member(cJSON_GetArrayItem(small_sections, 3), "figure"), "climax"));
    /* The old variations form has no figure and no mode. */
    const cJSON *old = member(default_size(form_named("variations")), "sections");
    for (int i = 0; i < cJSON_GetArraySize(old); i++) {
        assert(cJSON_IsNull(member(cJSON_GetArrayItem(old, i), "figure")) && cJSON_IsNull(member(cJSON_GetArrayItem(old, i), "mode")));
    }
}

/* No section of any other form has a figure or a mode (R-501); the scherzo's sections in particular. */
static void test_no_figure_elsewhere(void) {
    const cJSON *form = NULL;
    size_t checked = 0;
    cJSON_ArrayForEach(form, member(forms, "forms")) {
        if (strcmp(text(form, "name"), "figured-variations") == 0) {
            continue;
        }
        const cJSON *size = NULL;
        cJSON_ArrayForEach(size, member(form, "sizes")) {
            const cJSON *section = NULL;
            cJSON_ArrayForEach(section, member(size, "sections")) {
                assert(cJSON_IsNull(member(section, "figure")) && cJSON_IsNull(member(section, "mode")));
                checked++;
            }
        }
    }
    assert(checked > 0);
    const cJSON *scherzo = member(default_size(form_named("scherzo")), "sections");
    for (int i = 0; i < cJSON_GetArraySize(scherzo); i++) {
        assert(cJSON_IsNull(member(cJSON_GetArrayItem(scherzo, i), "figure")) && cJSON_IsNull(member(cJSON_GetArrayItem(scherzo, i), "mode")));
    }
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

/* The B of the ternary, the minuet and the trio is written over a dominant pedal at both sizes (R-446, R-487); no other
 * section of any form names one. */
static void test_pedal(void) {
    const cJSON *list = member(forms, "forms");
    const cJSON *form = NULL;
    size_t pedals = 0;
    cJSON_ArrayForEach(form, list) {
        const char *name = text(form, "name");
        bool pedal_form = strcmp(name, "ternary") == 0 || strcmp(name, "minuet") == 0 || strcmp(name, "trio") == 0;
        const cJSON *size = NULL;
        cJSON_ArrayForEach(size, member(form, "sizes")) {
            const cJSON *section = NULL;
            cJSON_ArrayForEach(section, member(size, "sections")) {
                const cJSON *pedal = member(section, "pedal");
                if (pedal_form && strcmp(text(section, "name"), "B") == 0) {
                    assert_choice(section, "pedal", "dominant", "Dominant pedal");
                    pedals++;
                } else {
                    assert(cJSON_IsNull(pedal));
                }
            }
        }
    }
    size_t sizes = (size_t)(cJSON_GetArraySize(member(form_named("ternary"), "sizes")) +
                            cJSON_GetArraySize(member(form_named("minuet"), "sizes")) +
                            cJSON_GetArraySize(member(form_named("trio"), "sizes")));
    assert(pedals == sizes && pedals >= 6);
}

/* The minuet, the trio and the minuet and trio (R-487): two sizes each, 24 bars the default, a region each; the minuet and
 * the trio have the ternary's three sections, the compound form has none of its own. */
static void test_minuet(void) {
    const char *const dance[] = {"minuet", "trio"};
    for (int i = 0; i < 2; i++) {
        const cJSON *form = form_named(dance[i]);
        assert(cJSON_GetArraySize(member(form, "sizes")) == 2 && number(default_size(form), "bars") == 24);
        assert(number(size_with_bars(form, 12), "bars") == 12);
        const char *const regions[] = {"away"};
        assert_strings(member(form, "regions"), regions, 1);
        const cJSON *sections = member(default_size(form), "sections");
        assert(cJSON_GetArraySize(sections) == 3);
        assert(strcmp(text(cJSON_GetArrayItem(sections, 0), "name"), "A") == 0 &&
               strcmp(text(cJSON_GetArrayItem(sections, 1), "name"), "B") == 0 &&
               strcmp(text(cJSON_GetArrayItem(sections, 2), "name"), "A return") == 0);
        assert(number(cJSON_GetArrayItem(sections, 0), "bars") == 8);
    }
    assert(strcmp(text(form_named("minuet"), "label"), "Minuet") == 0 && strcmp(text(form_named("trio"), "label"), "Trio") == 0);
    /* The minuet: 3/4 at 112, A and B repeated, performed; B leads into A return with a connective on some seeds. */
    const cJSON *minuet = form_named("minuet");
    const char *const three_four[] = {"3/4"};
    assert_strings(member(minuet, "meters"), three_four, 1);
    assert(number(minuet, "tempo") == 112 && flag(minuet, "performed") && cJSON_IsNull(member(minuet, "parts")));
    /* The minuet leaves home only when the request asks: it does not modulate by itself (R-493). */
    assert(!flag(minuet, "modulates") && flag(member(minuet, "settings"), "modulate"));
    const cJSON *groups = member(minuet, "repeats");
    const char *const first_group[] = {"A"};
    const char *const second_group[] = {"B", "A return"};
    assert(cJSON_GetArraySize(groups) == 2);
    assert_strings(cJSON_GetArrayItem(groups, 0), first_group, 1);
    assert_strings(cJSON_GetArrayItem(groups, 1), second_group, 2);
    const cJSON *minuet_sections = member(default_size(minuet), "sections");
    const cJSON *held = cJSON_GetArrayItem(minuet_sections, 1);
    assert(flag(held, "leads_in") && strcmp(text(member(held, "connective"), "value"), "sometimes") == 0 &&
           strcmp(text(member(held, "connective"), "label"), "Sometimes") == 0);
    assert(!flag(cJSON_GetArrayItem(minuet_sections, 0), "leads_in") && cJSON_IsNull(member(cJSON_GetArrayItem(minuet_sections, 0), "connective")));
    /* A form with none of them: any meter, no tempo, not performed. */
    const cJSON *ternary = form_named("ternary");
    assert(cJSON_GetArraySize(member(ternary, "meters")) == 0 && cJSON_IsNull(member(ternary, "tempo")) &&
           !flag(ternary, "performed") && cJSON_GetArraySize(member(ternary, "repeats")) == 0 && cJSON_IsNull(member(ternary, "parts")));
    const cJSON *both = form_named("minuet-and-trio");
    assert(flag(both, "performed") && number(both, "tempo") == 112);
    const cJSON *parts = member(both, "parts");
    assert(cJSON_GetArraySize(parts) == 3);
    const cJSON *trio = cJSON_GetArrayItem(parts, 1);
    assert(strcmp(text(trio, "name"), "Trio") == 0 && strcmp(text(member(trio, "form"), "value"), "trio") == 0 &&
           strcmp(text(member(trio, "form"), "label"), "Trio") == 0 && strcmp(text(trio, "key"), "trio") == 0 &&
           strcmp(text(trio, "motive"), "new") == 0 && cJSON_IsNull(member(trio, "again")) && flag(trio, "repeats"));
    /* No part of the minuet and trio gives its own bars. */
    for (int i = 0; i < 3; i++) {
        assert(cJSON_IsNull(member(cJSON_GetArrayItem(parts, i), "bars")));
    }
    const cJSON *again = cJSON_GetArrayItem(parts, 2);
    assert(cJSON_IsNull(member(again, "form")) && strcmp(text(again, "again"), "Minuet") == 0 && !flag(again, "repeats"));
    assert(strcmp(text(both, "label"), "Minuet and trio") == 0);
    assert(cJSON_GetArraySize(member(both, "sizes")) == 2 && number(default_size(both), "bars") == 24 &&
           number(size_with_bars(both, 12), "bars") == 12);
    const char *const trio_region[] = {"trio"};
    assert_strings(member(both, "regions"), trio_region, 1);
    assert(cJSON_GetArraySize(member(default_size(both), "sections")) == 0);
}

/* The scherzo and the scherzo and trio (R-492, R-495): 44 bars the default and 60, 176 quarters a minute, nine sections at
 * both sizes with the liquidation the fifth, the regions of the planned keys, the repeats of the minuet's shape; the
 * compound form puts the scherzo, a trio and the scherzo again. */
static void test_scherzo(void) {
    const cJSON *scherzo = form_named("scherzo");
    assert(strcmp(text(scherzo, "label"), "Scherzo") == 0 && cJSON_GetArraySize(member(scherzo, "sizes")) == 2);
    assert(number(default_size(scherzo), "bars") == 44 && number(size_with_bars(scherzo, 60), "bars") == 60);
    const char *const regions[] = {"pattern", "sequence", "episode"};
    assert_strings(member(scherzo, "regions"), regions, 3);
    const char *const meters[] = {"3/4", "2/4", "6/8"};
    assert_strings(member(scherzo, "meters"), meters, 3);
    assert(number(scherzo, "tempo") == 176 && flag(scherzo, "performed") && cJSON_IsNull(member(scherzo, "parts")));
    /* The scherzo always modulates (R-493): it has regions, and the modulate choice is not offered for it. */
    assert(flag(scherzo, "modulates") && !flag(member(scherzo, "settings"), "modulate"));
    const cJSON *groups = member(scherzo, "repeats");
    const char *const first_group[] = {"A"};
    const char *const second_group[] = {"P", "S1", "S2", "L", "A return"};
    assert(cJSON_GetArraySize(groups) == 2);
    assert_strings(cJSON_GetArrayItem(groups, 0), first_group, 1);
    assert_strings(cJSON_GetArrayItem(groups, 1), second_group, 5);
    static const struct {
        const char *name;
        const char *role;
        int bars;
        const char *start;
        const char *end;
    } SECTIONS[] = {
        {"A", "theme", 8, "home", "home"},          {"P", "pattern", 4, "pattern", "pattern"},
        {"S1", "sequence", 4, "sequence", "sequence"}, {"S2", "sequence", 4, "home", "home"},
        {"L", "liquidation", 4, "home", "home"},    {"A return", "return", 8, "home", "home"},
        {"E", "episode", 4, "episode", "episode"},  {"C1", "codetta", 4, "home", "home"},
        {"C2", "codetta", 4, "home", "home"},
    };
    for (int z = 0; z < 2; z++) {
        const cJSON *size = z == 0 ? default_size(scherzo) : size_with_bars(scherzo, 60);
        const cJSON *sections = member(size, "sections");
        assert(cJSON_GetArraySize(sections) == 9);
        int total = 0;
        for (int i = 0; i < 9; i++) {
            const cJSON *section = cJSON_GetArrayItem(sections, i);
            int bars = z == 1 && (i == 0 || i == 5) ? 16 : SECTIONS[i].bars;
            assert(strcmp(text(section, "name"), SECTIONS[i].name) == 0 && strcmp(text(member(section, "role"), "value"), SECTIONS[i].role) == 0);
            assert(number(section, "bars") == bars);
            assert(strcmp(text(member(section, "keys"), "start"), SECTIONS[i].start) == 0 &&
                   strcmp(text(member(section, "keys"), "end"), SECTIONS[i].end) == 0);
            /* Only the 16-bar themes of the larger size spell their construction out. */
            assert((cJSON_GetObjectItemCaseSensitive(section, "construction") != NULL) == (z == 1 && (i == 0 || i == 5)));
            total += bars;
        }
        assert(total == number(size, "bars"));
        const cJSON *later = cJSON_GetArrayItem(sections, 2);
        assert(strcmp(text(later, "returns"), "P") == 0 && strcmp(text(later, "variation"), "exact") == 0);
        assert(strcmp(text(cJSON_GetArrayItem(sections, 5), "returns"), "A") == 0 &&
               strcmp(text(cJSON_GetArrayItem(sections, 5), "variation"), "varied") == 0);
        assert(strcmp(text(cJSON_GetArrayItem(sections, 4), "letter"), "B") == 0);
    }
    const cJSON *both = form_named("scherzo-and-trio");
    assert(strcmp(text(both, "label"), "Scherzo and trio") == 0 && flag(both, "performed") && number(both, "tempo") == 176);
    const char *const three_four[] = {"3/4"};
    assert_strings(member(both, "meters"), three_four, 1);
    const char *const trio_region[] = {"trio"};
    assert_strings(member(both, "regions"), trio_region, 1);
    assert(cJSON_GetArraySize(member(both, "sizes")) == 2 && number(default_size(both), "bars") == 44 &&
           number(size_with_bars(both, 60), "bars") == 60 && cJSON_GetArraySize(member(default_size(both), "sections")) == 0);
    const cJSON *parts = member(both, "parts");
    assert(cJSON_GetArraySize(parts) == 3);
    const cJSON *first = cJSON_GetArrayItem(parts, 0);
    assert(strcmp(text(first, "name"), "Scherzo") == 0 && strcmp(text(member(first, "form"), "value"), "scherzo") == 0 &&
           strcmp(text(member(first, "form"), "label"), "Scherzo") == 0 && strcmp(text(first, "key"), "home") == 0 && flag(first, "repeats"));
    const cJSON *trio = cJSON_GetArrayItem(parts, 1);
    assert(strcmp(text(trio, "name"), "Trio") == 0 && strcmp(text(member(trio, "form"), "value"), "trio") == 0 &&
           strcmp(text(trio, "key"), "trio") == 0 && strcmp(text(trio, "motive"), "new") == 0 && cJSON_IsNull(member(trio, "again")) &&
           flag(trio, "repeats"));
    const cJSON *again = cJSON_GetArrayItem(parts, 2);
    assert(cJSON_IsNull(member(again, "form")) && strcmp(text(again, "again"), "Scherzo") == 0 && !flag(again, "repeats"));
    /* The trio part gives its own 24 bars (R-495); the others take the compound's. The compound form does not modulate by itself:
       each part decides, and the region the trio's key is drawn from is a choice the request still has. */
    assert(number(trio, "bars") == 24 && cJSON_IsNull(member(first, "bars")) && cJSON_IsNull(member(again, "bars")));
    assert(!flag(both, "modulates") && flag(member(both, "settings"), "modulate"));
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
    test_figured_variations();
    test_no_figure_elsewhere();
    test_layered();
    test_pedal();
    test_minuet();
    test_scherzo();
    test_game_loop();
    cJSON_Delete(forms);
    printf("test_api_forms: ok\n");
    return 0;
}
