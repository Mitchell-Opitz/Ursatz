#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"

/*
 * The theme shape table (R-416) against what the switch code of grow_request.c said before the table replaced it: for
 * each of the nine pairings, the ideas it has and their places, the slot, first flag and length of each place, and the
 * slots it uses. The expected values below are written out from that old code, not read from the table. Indexed by the
 * PlanThemeBeginning (presentation, antecedent, compound basic idea), the PlanThemeEnding (continuation, consequent,
 * cadential) and then the GrowIdea, the GrowPlace (basic, repetition, fragment 1, fragment 2, cadential) or the GrowSlot.
 * The units of a slot are only checked for a slot the pairing uses.
 */

static const struct { bool has; unsigned place; } IDEA_PLACES[3][3][GROW_IDEA_COUNT] = {
    {
        {{true, 0}, {true, 1}, {false, 0}, {false, 0}, {false, 0}, {true, 2}, {true, 3}, {true, 4}, {false, 0}, {false, 0}},
        {{true, 0}, {true, 1}, {false, 0}, {true, 2}, {true, 3}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}},
        {{true, 0}, {true, 1}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {true, 2}, {true, 3}},
    },
    {
        {{true, 0}, {false, 0}, {true, 1}, {false, 0}, {false, 0}, {true, 2}, {true, 3}, {true, 4}, {false, 0}, {false, 0}},
        {{true, 0}, {false, 0}, {true, 1}, {true, 2}, {true, 3}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}},
        {{true, 0}, {false, 0}, {true, 1}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {true, 2}, {true, 3}},
    },
    {
        {{true, 0}, {false, 0}, {true, 1}, {false, 0}, {false, 0}, {true, 2}, {true, 3}, {true, 4}, {false, 0}, {false, 0}},
        {{true, 0}, {false, 0}, {true, 1}, {true, 2}, {true, 3}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}},
        {{true, 0}, {false, 0}, {true, 1}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {true, 2}, {true, 3}},
    },
};
static const struct { GrowSlot slot; bool first; size_t units; } PLACES[3][3][5] = {
    {
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_CONTINUATION, true, 1}, {GROW_SLOT_CONTINUATION, false, 1}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_CONTRASTING, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}, {GROW_SLOT_CADENTIAL, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
    },
    {
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_CONTINUATION, true, 1}, {GROW_SLOT_CONTINUATION, false, 1}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_CONTRASTING, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_CADENTIAL, true, 2}, {GROW_SLOT_CADENTIAL, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
    },
    {
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_CONTINUATION, true, 1}, {GROW_SLOT_CONTINUATION, false, 1}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_BASIC, false, 2}, {GROW_SLOT_CONTRASTING, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
        {{GROW_SLOT_BASIC, true, 2}, {GROW_SLOT_CONTRASTING, true, 2}, {GROW_SLOT_CADENTIAL, true, 2}, {GROW_SLOT_CADENTIAL, false, 2}, {GROW_SLOT_CADENTIAL, true, 2}},
    },
};
static const struct { bool used; size_t units; } SLOT_USE[3][3][GROW_SLOT_COUNT] = {
    {
        {{true, 2}, {false, 0}, {true, 1}, {true, 2}},
        {{true, 2}, {true, 0}, {false, 0}, {false, 0}},
        {{true, 2}, {false, 0}, {false, 0}, {true, 2}},
    },
    {
        {{true, 2}, {true, 2}, {true, 1}, {true, 2}},
        {{true, 2}, {true, 2}, {false, 0}, {false, 0}},
        {{true, 2}, {true, 2}, {false, 0}, {true, 2}},
    },
    {
        {{true, 2}, {true, 2}, {true, 1}, {true, 2}},
        {{true, 2}, {true, 2}, {false, 0}, {false, 0}},
        {{true, 2}, {true, 2}, {false, 0}, {true, 2}},
    },
};

static const char *const IDEA_NAMES[GROW_IDEA_COUNT] = {
    "basic idea", "repetition", "contrasting idea", "consequent basic idea", "consequent contrasting idea",
    "fragment 1", "fragment 2", "cadential", "cadential 1", "cadential 2"};

static const GrowIdeaRole IDEA_ROLES[GROW_IDEA_COUNT] = {
    GROW_ROLE_PRESENTATION, GROW_ROLE_PRESENTATION, GROW_ROLE_CONTRAST,     GROW_ROLE_PRESENTATION, GROW_ROLE_CONTRAST,
    GROW_ROLE_CONTINUATION, GROW_ROLE_CONTINUATION, GROW_ROLE_CADENTIAL,    GROW_ROLE_CADENTIAL,    GROW_ROLE_CADENTIAL};

static const char *const SLOT_NAMES[GROW_SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};
static const char *const SLOT_FUNCTIONS[GROW_SLOT_COUNT] = {"beginning", "contrast", "continuation", "cadential"};
static const char *const SLOT_NODES[GROW_SLOT_COUNT] = {NULL, "s1", "s2", "s3"};

static void check_pairing(int beginning, int ending) {
    PlanThemePairing pairing = {(PlanThemeBeginning)beginning, (PlanThemeEnding)ending};
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place = GROW_PLACE_COUNT;
        bool has = grow_idea_place(&pairing, (GrowIdea)idea, &place);
        assert(has == IDEA_PLACES[beginning][ending][idea].has);
        assert(grow_idea_in_pairing(&pairing, (GrowIdea)idea) == has);
        assert(!has || place == IDEA_PLACES[beginning][ending][idea].place);
        assert(!has || (unsigned)place < GROW_PLACE_COUNT);
        assert(grow_idea_place(&pairing, (GrowIdea)idea, NULL) == has);
    }
    for (int place = 0; place < GROW_PLACE_COUNT; place++) {
        bool first = false;
        GrowSlot slot = grow_idea_slot(&pairing, (GrowPlace)place, &first);
        assert(slot == PLACES[beginning][ending][place].slot);
        assert(first == PLACES[beginning][ending][place].first);
        assert(grow_idea_slot(&pairing, (GrowPlace)place, NULL) == slot);
        assert(grow_place_units(&pairing, (GrowPlace)place) == PLACES[beginning][ending][place].units);
    }
    for (int slot = 0; slot < GROW_SLOT_COUNT; slot++) {
        size_t units = 99;
        bool used = grow_slot_used(&pairing, (GrowSlot)slot, &units);
        assert(used == SLOT_USE[beginning][ending][slot].used);
        assert(!used || units == SLOT_USE[beginning][ending][slot].units);
        assert(grow_slot_used(&pairing, (GrowSlot)slot, NULL) == used);
    }
}

int main(void) {
    for (int beginning = 0; beginning < 3; beginning++) {
        for (int ending = 0; ending < 3; ending++) {
            check_pairing(beginning, ending);
        }
    }

    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        assert(strcmp(grow_idea_name((GrowIdea)idea), IDEA_NAMES[idea]) == 0);
        assert(grow_idea_role(IDEA_NAMES[idea]) == IDEA_ROLES[idea]);
    }
    assert(strcmp(grow_idea_name(GROW_IDEA_COUNT), "unknown") == 0);
    assert(strcmp(grow_idea_name((GrowIdea)-1), "unknown") == 0);
    assert(grow_idea_role("unknown") == GROW_ROLE_NONE);
    assert(grow_idea_role("") == GROW_ROLE_NONE);
    assert(grow_idea_role(NULL) == GROW_ROLE_NONE);

    for (int slot = 0; slot < GROW_SLOT_COUNT; slot++) {
        assert(strcmp(grow_slot_name((GrowSlot)slot), SLOT_NAMES[slot]) == 0);
        assert(strcmp(grow_slot_function((GrowSlot)slot), SLOT_FUNCTIONS[slot]) == 0);
        const char *node = grow_slot_node_name((GrowSlot)slot);
        assert(SLOT_NODES[slot] ? node && strcmp(node, SLOT_NODES[slot]) == 0 : node == NULL);
    }
    assert(strcmp(grow_slot_name(GROW_SLOT_COUNT), "unknown") == 0);
    assert(strcmp(grow_slot_function(GROW_SLOT_COUNT), "unknown") == 0);
    assert(grow_slot_node_name(GROW_SLOT_COUNT) == NULL);

    /* The halves: the plan names (plan_json) and the library names (standard-form). */
    static const char *const BEGINNING_PLAN[3] = {"presentation", "antecedent", "compound_basic_idea"};
    static const char *const BEGINNING_LIBRARY[3] = {"presentation", "antecedent", "compound-basic-idea"};
    static const char *const ENDING_NAMES[3] = {"continuation", "consequent", "cadential"};
    assert(theme_shape_half_count() == 6);
    assert(theme_shape_half_at(6) == NULL);
    for (int i = 0; i < 3; i++) {
        const ThemeShapeHalf *beginning = theme_shape_beginning((PlanThemeBeginning)i);
        const ThemeShapeHalf *ending = theme_shape_ending((PlanThemeEnding)i);
        assert(beginning && beginning->beginning && beginning->value == i);
        assert(strcmp(beginning->plan_name, BEGINNING_PLAN[i]) == 0);
        assert(strcmp(beginning->library_name, BEGINNING_LIBRARY[i]) == 0);
        assert(theme_shape_half_find(true, BEGINNING_PLAN[i]) == beginning);
        assert(ending && !ending->beginning && ending->value == i);
        assert(strcmp(ending->plan_name, ENDING_NAMES[i]) == 0);
        assert(strcmp(ending->library_name, ENDING_NAMES[i]) == 0);
        assert(theme_shape_half_find(false, ENDING_NAMES[i]) == ending);
    }
    assert(theme_shape_beginning((PlanThemeBeginning)3) == NULL);
    assert(theme_shape_ending((PlanThemeEnding)-1) == NULL);
    assert(theme_shape_half_find(true, "continuation") == NULL);
    assert(theme_shape_half_find(false, "presentation") == NULL);
    assert(theme_shape_half_find(true, NULL) == NULL);

    /* The rows: 13 for 10 ideas, each idea at least once. */
    assert(theme_shape_idea_count() == 13);
    assert(theme_shape_idea_at(13) == NULL);
    bool seen[GROW_IDEA_COUNT] = {false};
    for (size_t i = 0; i < theme_shape_idea_count(); i++) {
        seen[theme_shape_idea_at(i)->idea] = true;
    }
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        assert(seen[idea]);
    }
    printf("test_theme_shape: ok\n");
    return 0;
}
