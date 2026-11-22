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
 * SectionBeginning (presentation, antecedent, compound basic idea), the SectionEnding (continuation, consequent,
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
    SectionPairing pairing = {(SectionBeginning)beginning, (SectionEnding)ending};
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place = GROW_PLACE_COUNT;
        bool has = grow_idea_place(&pairing, (GrowIdea)idea, &place);
        assert(has == IDEA_PLACES[beginning][ending][idea].has);
        assert(grow_idea_in_pairing(&pairing, (GrowIdea)idea) == has);
        assert(!has || place == IDEA_PLACES[beginning][ending][idea].place);
        assert(!has || (unsigned)place < GROW_PLACE_COUNT);
        assert(grow_idea_place(&pairing, (GrowIdea)idea, NULL) == has);
        /* The row of the idea under the pairing exists exactly where the table says, at its place, with its name. */
        const ThemeShapeIdea *row = theme_shape_idea_in(&pairing, (GrowIdea)idea);
        assert((row != NULL) == has);
        assert(!has || (row->idea == (GrowIdea)idea && row->place == IDEA_PLACES[beginning][ending][idea].place &&
                        strcmp(row->name, IDEA_NAMES[idea]) == 0 && row->role == IDEA_ROLES[idea]));
    }
    /* The row at a place: it exists where some idea sits there, and is that idea's row, with the place's slot, first flag and length. */
    for (unsigned place = 0; place < GROW_PLACE_COUNT; place++) {
        const ThemeShapeIdea *row = theme_shape_place_in(&pairing, place);
        bool occupied = false;
        for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
            occupied = occupied || (IDEA_PLACES[beginning][ending][idea].has && IDEA_PLACES[beginning][ending][idea].place == place);
        }
        assert((row != NULL) == occupied);
        assert(!row || (row->place == place && row->slot == PLACES[beginning][ending][place].slot &&
                        row->first == PLACES[beginning][ending][place].first && row->units == PLACES[beginning][ending][place].units));
    }
    /* A continuation ends on the cadential place; the other endings end on the second fragment. */
    assert(theme_shape_last_place(&pairing) == (ending == SECTION_ENDING_CONTINUATION ? GROW_PLACE_CADENTIAL : GROW_PLACE_FRAGMENT_2));
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
        assert(strcmp(theme_shape_slot((GrowSlot)slot)->function, SLOT_FUNCTIONS[slot]) == 0);
        const char *node = grow_slot_node_name((GrowSlot)slot);
        assert(SLOT_NODES[slot] ? node && strcmp(node, SLOT_NODES[slot]) == 0 : node == NULL);
    }
    assert(strcmp(grow_slot_name(GROW_SLOT_COUNT), "unknown") == 0);
    assert(theme_shape_slot(GROW_SLOT_COUNT) == NULL);
    assert(grow_slot_node_name(GROW_SLOT_COUNT) == NULL);

    /* The halves: the plan names (the section tables' too) and the library names (standard-form). */
    static const char *const BEGINNING_PLAN[3] = {"presentation", "antecedent", "compound_basic_idea"};
    static const char *const BEGINNING_LIBRARY[3] = {"presentation", "antecedent", "compound-basic-idea"};
    static const char *const ENDING_NAMES[3] = {"continuation", "consequent", "cadential"};
    for (int i = 0; i < 3; i++) {
        const ThemeShapeHalf *beginning = theme_shape_beginning((SectionBeginning)i);
        const ThemeShapeHalf *ending = theme_shape_ending((SectionEnding)i);
        assert(beginning && beginning->beginning && beginning->value == i);
        assert(strcmp(beginning->plan_name, BEGINNING_PLAN[i]) == 0);
        assert(strcmp(beginning->library_name, BEGINNING_LIBRARY[i]) == 0);
        assert(ending && !ending->beginning && ending->value == i);
        assert(strcmp(ending->plan_name, ENDING_NAMES[i]) == 0);
        assert(strcmp(ending->library_name, ENDING_NAMES[i]) == 0);
    }
    assert(theme_shape_beginning((SectionBeginning)3) == NULL);
    assert(theme_shape_ending((SectionEnding)-1) == NULL);

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
    /* The idea by name, in any half (the first row), and the slot of an idea (the first row it has). */
    static const GrowSlot SLOT_OF_IDEA[GROW_IDEA_COUNT] = {GROW_SLOT_BASIC,        GROW_SLOT_BASIC,        GROW_SLOT_CONTRASTING,
                                                           GROW_SLOT_BASIC,        GROW_SLOT_CONTRASTING,  GROW_SLOT_CONTINUATION,
                                                           GROW_SLOT_CONTINUATION, GROW_SLOT_CADENTIAL,    GROW_SLOT_CADENTIAL,
                                                           GROW_SLOT_CADENTIAL};
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_find(IDEA_NAMES[idea]);
        assert(row && row->idea == (GrowIdea)idea && strcmp(row->name, IDEA_NAMES[idea]) == 0);
        assert(theme_shape_idea_slot((GrowIdea)idea) == SLOT_OF_IDEA[idea]);
    }
    assert(theme_shape_idea_find("bogus") == NULL && theme_shape_idea_find("") == NULL && theme_shape_idea_find(NULL) == NULL);
    assert(theme_shape_idea_slot(GROW_IDEA_COUNT) == GROW_SLOT_BASIC && theme_shape_idea_slot((GrowIdea)-1) == GROW_SLOT_BASIC);

    /* The bars of a construction: a sentence (presentation, continuation) of 2 2 1 1 2 bars is 4 and 4 with nothing extra; each extension
       adds its bars to the half it follows, or after the cadence. */
    const SectionPairing sentence = {SECTION_BEGINNING_PRESENTATION, SECTION_ENDING_CONTINUATION};
    SectionConstruction construction;
    GrowConstructionBars bars;
    memset(&construction, 0, sizeof(construction));
    construction.idea_bars[SECTION_IDEA_BASIC] = 2;
    construction.idea_bars[SECTION_IDEA_REPETITION] = 2;
    construction.idea_bars[SECTION_IDEA_FRAGMENT_1] = 1;
    construction.idea_bars[SECTION_IDEA_FRAGMENT_2] = 1;
    construction.idea_bars[SECTION_IDEA_CADENTIAL] = 2;
    grow_construction_bars(&sentence, &construction, &bars);
    assert(bars.beginning == 4 && bars.ending == 4 && bars.evaded == 0 && bars.addition == 0);
    assert(grow_construction_extra_bars(&sentence, &construction) == 0);
    /* A repeat of the cadential idea (bars 0: as long as it is): the ending is 6 and 2 are extra. */
    construction.extension_count = 1;
    construction.extensions[0] = (SectionExtension){SECTION_EXTENSION_REPEAT, SECTION_IDEA_CADENTIAL, 0, false};
    grow_construction_bars(&sentence, &construction, &bars);
    assert(bars.beginning == 4 && bars.ending == 6 && grow_construction_extra_bars(&sentence, &construction) == 2);
    /* A stretch of the basic idea by one bar (bars 0: one): the beginning is 5. */
    construction.extensions[0] = (SectionExtension){SECTION_EXTENSION_STRETCH, SECTION_IDEA_BASIC, 0, false};
    grow_construction_bars(&sentence, &construction, &bars);
    assert(bars.beginning == 5 && bars.ending == 4 && grow_construction_extra_bars(&sentence, &construction) == 1);
    /* An evaded cadence repeats the last place's bars after the ending; an addition is its own bars. */
    construction.extensions[0] = (SectionExtension){SECTION_EXTENSION_EVADED, SECTION_IDEA_CADENTIAL, 0, false};
    grow_construction_bars(&sentence, &construction, &bars);
    assert(bars.evaded == 2 && bars.ending == 4 && grow_construction_extra_bars(&sentence, &construction) == 2);
    construction.extensions[0] = (SectionExtension){SECTION_EXTENSION_ADDITION, SECTION_IDEA_CADENTIAL, 3, false};
    grow_construction_bars(&sentence, &construction, &bars);
    assert(bars.addition == 3 && grow_construction_extra_bars(&sentence, &construction) == 3);
    size_t places[GROW_PLACE_COUNT];
    size_t repeated[GROW_PLACE_COUNT];
    size_t evaded = 99;
    size_t addition = 99;
    construction.extension_count = 0;
    theme_shape_construction_places(&sentence, &construction, places, repeated, &evaded, &addition);
    assert(places[GROW_PLACE_BASIC] == 2 && places[GROW_PLACE_REPETITION] == 2 && places[GROW_PLACE_FRAGMENT_1] == 1 &&
           places[GROW_PLACE_FRAGMENT_2] == 1 && places[GROW_PLACE_CADENTIAL] == 2 && evaded == 0 && addition == 0);
    for (int place = 0; place < GROW_PLACE_COUNT; place++) {
        assert(repeated[place] == 0);
    }

    printf("test_theme_shape: ok\n");
    return 0;
}
