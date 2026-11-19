#include <ursatz_composer/theme_shape.h>

#include <string.h>


/* The beginnings and endings, in SectionBeginning and SectionEnding order. The plan names have underscores, the library names hyphens. */
static const ThemeShapeHalf HALVES[] = {
    {true, SECTION_BEGINNING_PRESENTATION, "presentation", "presentation"},
    {true, SECTION_BEGINNING_ANTECEDENT, "antecedent", "antecedent"},
    {true, SECTION_BEGINNING_COMPOUND_BASIC_IDEA, "compound_basic_idea", "compound-basic-idea"},
    {false, SECTION_ENDING_CONTINUATION, "continuation", "continuation"},
    {false, SECTION_ENDING_CONSEQUENT, "consequent", "consequent"},
    {false, SECTION_ENDING_CADENTIAL, "cadential", "cadential"},
};

/* The ideas of each half, in the order of the halves. The place is the GrowPlace index, the units the eighths of the theme. */
static const ThemeShapeIdea IDEAS[] = {
    {GROW_IDEA_BASIC, "basic idea", true, SECTION_BEGINNING_PRESENTATION, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_REPETITION, "repetition", true, SECTION_BEGINNING_PRESENTATION, GROW_PLACE_REPETITION, GROW_SLOT_BASIC,
     false, GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_BASIC, "basic idea", true, SECTION_BEGINNING_ANTECEDENT, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONTRASTING, "contrasting idea", true, SECTION_BEGINNING_ANTECEDENT, GROW_PLACE_CONTRAST,
     GROW_SLOT_CONTRASTING, true, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_BASIC, "basic idea", true, SECTION_BEGINNING_COMPOUND_BASIC_IDEA, GROW_PLACE_BASIC, GROW_SLOT_BASIC, true,
     GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONTRASTING, "contrasting idea", true, SECTION_BEGINNING_COMPOUND_BASIC_IDEA, GROW_PLACE_CONTRAST,
     GROW_SLOT_CONTRASTING, true, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_FRAGMENT_1, "fragment 1", false, SECTION_ENDING_CONTINUATION, GROW_PLACE_FRAGMENT_1,
     GROW_SLOT_CONTINUATION, true, GROW_ROLE_CONTINUATION, 1},
    {GROW_IDEA_FRAGMENT_2, "fragment 2", false, SECTION_ENDING_CONTINUATION, GROW_PLACE_FRAGMENT_2,
     GROW_SLOT_CONTINUATION, false, GROW_ROLE_CONTINUATION, 1},
    {GROW_IDEA_CADENTIAL, "cadential", false, SECTION_ENDING_CONTINUATION, GROW_PLACE_CADENTIAL, GROW_SLOT_CADENTIAL,
     true, GROW_ROLE_CADENTIAL, 2},
    {GROW_IDEA_CONSEQUENT_BASIC, "consequent basic idea", false, SECTION_ENDING_CONSEQUENT, GROW_PLACE_RESTATED,
     GROW_SLOT_BASIC, false, GROW_ROLE_PRESENTATION, 2},
    {GROW_IDEA_CONSEQUENT_CONTRASTING, "consequent contrasting idea", false, SECTION_ENDING_CONSEQUENT,
     GROW_PLACE_CONSEQUENT_CONTRAST, GROW_SLOT_CONTRASTING, false, GROW_ROLE_CONTRAST, 2},
    {GROW_IDEA_CADENTIAL_1, "cadential 1", false, SECTION_ENDING_CADENTIAL, GROW_PLACE_CADENTIAL_1, GROW_SLOT_CADENTIAL,
     true, GROW_ROLE_CADENTIAL, 2},
    {GROW_IDEA_CADENTIAL_2, "cadential 2", false, SECTION_ENDING_CADENTIAL, GROW_PLACE_CADENTIAL_2, GROW_SLOT_CADENTIAL,
     false, GROW_ROLE_CADENTIAL, 2},
};

static const ThemeShapeSlot SLOTS[GROW_SLOT_COUNT] = {
    {GROW_SLOT_BASIC, "basic", "beginning", NULL},
    {GROW_SLOT_CONTRASTING, "contrasting", "contrast", "s1"},
    {GROW_SLOT_CONTINUATION, "continuation", "continuation", "s2"},
    {GROW_SLOT_CADENTIAL, "cadential", "cadential", "s3"},
};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

const ThemeShapeHalf *theme_shape_beginning(SectionBeginning beginning) {
    return beginning >= 0 && beginning <= SECTION_BEGINNING_COMPOUND_BASIC_IDEA ? &HALVES[beginning] : NULL;
}

const ThemeShapeHalf *theme_shape_ending(SectionEnding ending) {
    return ending >= 0 && ending <= SECTION_ENDING_CADENTIAL ? &HALVES[3 + (int)ending] : NULL;
}

size_t theme_shape_idea_count(void) {
    return COUNT_OF(IDEAS);
}

const ThemeShapeIdea *theme_shape_idea_at(size_t index) {
    return index < COUNT_OF(IDEAS) ? &IDEAS[index] : NULL;
}

/* Whether the row belongs to the pairing's beginning or its ending. */
static bool row_in(const ThemeShapeIdea *row, const SectionPairing *pairing) {
    return row->half == (row->in_beginning ? (int)pairing->beginning : (int)pairing->ending);
}

const ThemeShapeIdea *theme_shape_idea_in(const SectionPairing *pairing, GrowIdea idea) {
    for (size_t i = 0; pairing && i < COUNT_OF(IDEAS); i++) {
        if (IDEAS[i].idea == idea && row_in(&IDEAS[i], pairing)) {
            return &IDEAS[i];
        }
    }
    return NULL;
}

const ThemeShapeIdea *theme_shape_place_in(const SectionPairing *pairing, unsigned place) {
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
    return idea >= 0 && idea < GROW_IDEA_COUNT ? section_idea_name((SectionIdea)idea) : "unknown";
}

const ThemeShapeSlot *theme_shape_slot(GrowSlot slot) {
    return slot >= 0 && slot < GROW_SLOT_COUNT ? &SLOTS[slot] : NULL;
}

/* The place the theme's last idea holds under the pairing. */
GrowPlace theme_shape_last_place(const SectionPairing *pairing) {
    for (int place = GROW_PLACE_CADENTIAL; place > GROW_PLACE_REPETITION; place--) {
        if (theme_shape_place_in(pairing, (unsigned)place)) {
            return (GrowPlace)place;
        }
    }
    return GROW_PLACE_BASIC;
}


bool grow_idea_place(const SectionPairing *pairing, GrowIdea idea, GrowPlace *place) {
    const ThemeShapeIdea *row = theme_shape_idea_in(pairing, idea);
    if (row && place) {
        *place = (GrowPlace)row->place;
    }
    return row != NULL;
}

bool grow_idea_in_pairing(const SectionPairing *pairing, GrowIdea idea) {
    return theme_shape_idea_in(pairing, idea) != NULL;
}

size_t grow_place_units(const SectionPairing *pairing, GrowPlace place) {
    const ThemeShapeIdea *row = theme_shape_place_in(pairing, (unsigned)place);
    return row ? row->units : 2;
}


/*
 * The bars of each place of the construction (with its stretches), of the repeats that follow it, and of the evaded idea
 * and the addition after the last place, in the order the extensions are listed.
 */
void theme_shape_construction_places(const SectionPairing *pairing, const SectionConstruction *construction,
                                size_t bars[GROW_PLACE_COUNT], size_t repeated[GROW_PLACE_COUNT], size_t *evaded,
                                size_t *addition) {
    GrowPlace last = theme_shape_last_place(pairing);
    memset(bars, 0, sizeof(size_t) * GROW_PLACE_COUNT);
    memset(repeated, 0, sizeof(size_t) * GROW_PLACE_COUNT);
    *evaded = 0;
    *addition = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(pairing, (GrowIdea)idea);
        if (row) {
            bars[row->place] = construction->idea_bars[idea];
        }
    }
    for (size_t k = 0; k < construction->extension_count && k < SECTION_CONSTRUCTION_EXTENSIONS; k++) {
        const SectionExtension *extension = &construction->extensions[k];
        GrowPlace place = last;
        if ((extension->kind == SECTION_EXTENSION_REPEAT || extension->kind == SECTION_EXTENSION_STRETCH) &&
            !grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
            continue;
        }
        switch (extension->kind) {
        case SECTION_EXTENSION_REPEAT:
            repeated[place] += extension->bars ? extension->bars : bars[place];
            break;
        case SECTION_EXTENSION_STRETCH:
            bars[place] += extension->bars ? extension->bars : 1;
            break;
        case SECTION_EXTENSION_EVADED:
            *evaded = extension->bars ? extension->bars : bars[last];
            break;
        case SECTION_EXTENSION_ADDITION:
            *addition = extension->bars ? extension->bars : 1;
            break;
        }
    }
}

void grow_construction_bars(const SectionPairing *pairing, const SectionConstruction *construction, GrowConstructionBars *out) {
    size_t bars[GROW_PLACE_COUNT];
    size_t repeated[GROW_PLACE_COUNT];
    memset(out, 0, sizeof(*out));
    theme_shape_construction_places(pairing, construction, bars, repeated, &out->evaded, &out->addition);
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        size_t *half = place <= GROW_PLACE_REPETITION ? &out->beginning : &out->ending;
        *half += bars[place] + repeated[place];
    }
}

size_t grow_construction_extra_bars(const SectionPairing *pairing, const SectionConstruction *construction) {
    GrowConstructionBars halves;
    size_t ideas = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        ideas += theme_shape_idea_in(pairing, (GrowIdea)idea) ? construction->idea_bars[idea] : 0;
    }
    grow_construction_bars(pairing, construction, &halves);
    return halves.beginning + halves.ending + halves.evaded + halves.addition - ideas;
}
