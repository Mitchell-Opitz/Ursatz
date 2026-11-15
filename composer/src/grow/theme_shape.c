#include <ursatz_composer/theme_shape.h>

#include <string.h>

#include "grow/grow_internal.h"

/* The beginnings and endings, in PlanThemeBeginning and PlanThemeEnding order. The plan names have underscores, the library names hyphens. */
static const ThemeShapeHalf HALVES[] = {
    {true, PLAN_THEME_BEGINNING_PRESENTATION, "presentation", "presentation"},
    {true, PLAN_THEME_BEGINNING_ANTECEDENT, "antecedent", "antecedent"},
    {true, PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, "compound_basic_idea", "compound-basic-idea"},
    {false, PLAN_THEME_ENDING_CONTINUATION, "continuation", "continuation"},
    {false, PLAN_THEME_ENDING_CONSEQUENT, "consequent", "consequent"},
    {false, PLAN_THEME_ENDING_CADENTIAL, "cadential", "cadential"},
};

/* The ideas of each half, in the order of the halves. The place is the GrowPlace index, the units the eighths of the theme. */
static const ThemeShapeIdea IDEAS[] = {
    {GROW_IDEA_BASIC, "basic idea", true, PLAN_THEME_BEGINNING_PRESENTATION, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_REPETITION, "repetition", true, PLAN_THEME_BEGINNING_PRESENTATION, GROW_PLACE_REPETITION, GROW_SLOT_BASIC,
     false, GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_BASIC, "basic idea", true, PLAN_THEME_BEGINNING_ANTECEDENT, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONTRASTING, "contrasting idea", true, PLAN_THEME_BEGINNING_ANTECEDENT, GROW_PLACE_CONTRAST,
     GROW_SLOT_CONTRASTING, true, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_BASIC, "basic idea", true, PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONTRASTING, "contrasting idea", true, PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, GROW_PLACE_CONTRAST,
     GROW_SLOT_CONTRASTING, true, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_FRAGMENT_1, "fragment 1", false, PLAN_THEME_ENDING_CONTINUATION, GROW_PLACE_FRAGMENT_1,
     GROW_SLOT_CONTINUATION, true, GROW_ROLE_CONTINUATION, 1},
    {GROW_IDEA_FRAGMENT_2, "fragment 2", false, PLAN_THEME_ENDING_CONTINUATION, GROW_PLACE_FRAGMENT_2,
     GROW_SLOT_CONTINUATION, false, GROW_ROLE_CONTINUATION, 1},
    {GROW_IDEA_CADENTIAL, "cadential", false, PLAN_THEME_ENDING_CONTINUATION, GROW_PLACE_CADENTIAL, GROW_SLOT_CADENTIAL,
     true, GROW_ROLE_CADENTIAL, 2},
    {GROW_IDEA_CONSEQUENT_BASIC, "consequent basic idea", false, PLAN_THEME_ENDING_CONSEQUENT, GROW_PLACE_RESTATED,
     GROW_SLOT_BASIC, false, GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONSEQUENT_CONTRASTING, "consequent contrasting idea", false, PLAN_THEME_ENDING_CONSEQUENT,
     GROW_PLACE_CONSEQUENT_CONTRAST, GROW_SLOT_CONTRASTING, false, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_CADENTIAL_1, "cadential 1", false, PLAN_THEME_ENDING_CADENTIAL, GROW_PLACE_CADENTIAL_1, GROW_SLOT_CADENTIAL,
     true, GROW_ROLE_CADENTIAL, 2},
    {GROW_IDEA_CADENTIAL_2, "cadential 2", false, PLAN_THEME_ENDING_CADENTIAL, GROW_PLACE_CADENTIAL_2, GROW_SLOT_CADENTIAL,
     false, GROW_ROLE_CADENTIAL, 2},
};

/* The ideas in GrowIdea order, by name. */
static const char *const IDEA_NAMES[GROW_IDEA_COUNT] = {
    "basic idea", "repetition", "contrasting idea", "consequent basic idea", "consequent contrasting idea",
    "fragment 1", "fragment 2", "cadential", "cadential 1", "cadential 2"};

static const ThemeShapeSlot SLOTS[GROW_SLOT_COUNT] = {
    {GROW_SLOT_BASIC, "basic", "beginning", NULL},
    {GROW_SLOT_CONTRASTING, "contrasting", "contrast", "s1"},
    {GROW_SLOT_CONTINUATION, "continuation", "continuation", "s2"},
    {GROW_SLOT_CADENTIAL, "cadential", "cadential", "s3"},
};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

size_t theme_shape_half_count(void) {
    return COUNT_OF(HALVES);
}

const ThemeShapeHalf *theme_shape_half_at(size_t index) {
    return index < COUNT_OF(HALVES) ? &HALVES[index] : NULL;
}

const ThemeShapeHalf *theme_shape_beginning(PlanThemeBeginning beginning) {
    return beginning >= 0 && beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA ? &HALVES[beginning] : NULL;
}

const ThemeShapeHalf *theme_shape_ending(PlanThemeEnding ending) {
    return ending >= 0 && ending <= PLAN_THEME_ENDING_CADENTIAL ? &HALVES[3 + (int)ending] : NULL;
}

const ThemeShapeHalf *theme_shape_half_find(bool beginning, const char *plan_name) {
    for (size_t i = 0; plan_name && i < COUNT_OF(HALVES); i++) {
        if (HALVES[i].beginning == beginning && strcmp(HALVES[i].plan_name, plan_name) == 0) {
            return &HALVES[i];
        }
    }
    return NULL;
}

size_t theme_shape_idea_count(void) {
    return COUNT_OF(IDEAS);
}

const ThemeShapeIdea *theme_shape_idea_at(size_t index) {
    return index < COUNT_OF(IDEAS) ? &IDEAS[index] : NULL;
}

/* Whether the row belongs to the pairing's beginning or its ending. */
static bool row_in(const ThemeShapeIdea *row, const PlanThemePairing *pairing) {
    return row->half == (row->in_beginning ? (int)pairing->beginning : (int)pairing->ending);
}

const ThemeShapeIdea *theme_shape_idea_in(const PlanThemePairing *pairing, GrowIdea idea) {
    for (size_t i = 0; pairing && i < COUNT_OF(IDEAS); i++) {
        if (IDEAS[i].idea == idea && row_in(&IDEAS[i], pairing)) {
            return &IDEAS[i];
        }
    }
    return NULL;
}

const ThemeShapeIdea *theme_shape_place_in(const PlanThemePairing *pairing, unsigned place) {
    bool from_beginning = place == GROW_PLACE_BASIC || place == GROW_PLACE_REPETITION;
    for (size_t i = 0; pairing && i < COUNT_OF(IDEAS); i++) {
        if (IDEAS[i].place == place && IDEAS[i].in_beginning == from_beginning && row_in(&IDEAS[i], pairing)) {
            return &IDEAS[i];
        }
    }
    return NULL;
}

const ThemeShapeIdea *theme_shape_idea_find(const char *name) {
    for (size_t i = 0; name && i < COUNT_OF(IDEAS); i++) {
        if (strcmp(IDEAS[i].name, name) == 0) {
            return &IDEAS[i];
        }
    }
    return NULL;
}

GrowSlot theme_shape_idea_slot(GrowIdea idea) {
    for (size_t i = 0; i < COUNT_OF(IDEAS); i++) {
        if (IDEAS[i].idea == idea) {
            return IDEAS[i].slot;
        }
    }
    return GROW_SLOT_BASIC;
}

const char *theme_shape_idea_name(GrowIdea idea) {
    return idea >= 0 && idea < GROW_IDEA_COUNT ? IDEA_NAMES[idea] : "unknown";
}

const ThemeShapeSlot *theme_shape_slot(GrowSlot slot) {
    return slot >= 0 && slot < GROW_SLOT_COUNT ? &SLOTS[slot] : NULL;
}
