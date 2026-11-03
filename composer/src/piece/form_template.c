#include <ursatz_composer/form_template.h>

#include <stdio.h>
#include <string.h>

static const char *const NAMES[FORM_TEMPLATE_KIND_COUNT] = {"binary",     "rounded-binary", "ternary",
                                                            "variations", "loop",           "layered"};

const char *form_template_name(FormTemplateKind kind) {
    return kind >= FORM_TEMPLATE_BINARY && kind <= FORM_TEMPLATE_LAYERED ? NAMES[kind] : NULL;
}

bool form_template_from_name(const char *name, FormTemplateKind *out) {
    for (int kind = 0; name && kind < FORM_TEMPLATE_KIND_COUNT; kind++) {
        if (strcmp(name, NAMES[kind]) == 0) {
            *out = (FormTemplateKind)kind;
            return true;
        }
    }
    return false;
}

size_t form_template_default_bars(FormTemplateKind kind) {
    switch (kind) {
    case FORM_TEMPLATE_BINARY:
    case FORM_TEMPLATE_ROUNDED_BINARY:
        return 16;
    case FORM_TEMPLATE_TERNARY:
        return 24;
    case FORM_TEMPLATE_VARIATIONS:
        return 32;
    case FORM_TEMPLATE_LOOP:
        return 8;
    case FORM_TEMPLATE_LAYERED:
        return 32;
    }
    return 0;
}

static const PlanThemePairing SENTENCE = {PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION};
static const PlanThemePairing PERIOD = {PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT};
static const PlanThemePairing HYBRID_CBI = {PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, PLAN_THEME_ENDING_CONTINUATION};

/* Adds a section made from m0 itself that returns none. */
static FormTemplateSection *add_section(FormTemplate *out, const char *letter, const char *name, size_t bars,
                                        PlanThemePairing pairing, CadenceType cadence) {
    FormTemplateSection *section = &out->sections[out->section_count++];
    memset(section, 0, sizeof(*section));
    snprintf(section->letter, sizeof(section->letter), "%s", letter);
    snprintf(section->name, sizeof(section->name), "%s", name);
    section->bars = bars;
    section->pairing = pairing;
    section->cadence = cadence;
    section->returns = FORM_TEMPLATE_NO_RETURN;
    section->variation = PLAN_RETURN_EXACT;
    section->motive = PLAN_TRANSFORM_NONE;
    return section;
}

/* A B section: its motive is m0 inverted in the key's steps, so it sounds unlike A. */
static void contrast_motive(FormTemplateSection *section) {
    section->motive = PLAN_TRANSFORM_INVERTED;
    section->motive_tonal = true;
}

/* A section that returns an earlier one takes the earlier one's motive. */
static void returns_to(FormTemplate *out, FormTemplateSection *section, size_t source, PlanReturnVariation variation) {
    section->returns = source;
    section->variation = variation;
    section->motive = out->sections[source].motive;
    section->motive_tonal = out->sections[source].motive_tonal;
}

static bool binary(size_t bars, FormTemplate *out) {
    if (bars != 16 && bars != 8) {
        return false;
    }
    size_t half = bars / 2;
    add_section(out, "A", "A", half, SENTENCE, CADENCE_TYPE_PERFECT_AUTHENTIC);
    contrast_motive(add_section(out, "B", "B", half, SENTENCE, CADENCE_TYPE_PERFECT_AUTHENTIC));
    return true;
}

static bool rounded_binary(size_t bars, FormTemplate *out) {
    if (bars != 16 && bars != 24) {
        return false;
    }
    size_t short_bars = bars == 16 ? 4 : 8;
    add_section(out, "A", "A", 8, SENTENCE, CADENCE_TYPE_PERFECT_AUTHENTIC);
    contrast_motive(add_section(out, "B", "B", short_bars, HYBRID_CBI, CADENCE_TYPE_HALF));
    returns_to(out, add_section(out, "A'", "A'", short_bars, SENTENCE, CADENCE_TYPE_PERFECT_AUTHENTIC), 0, PLAN_RETURN_EXACT);
    return true;
}

static bool ternary(size_t bars, FormTemplate *out) {
    if (bars != 24 && bars != 12) {
        return false;
    }
    size_t third = bars / 3;
    add_section(out, "A", "A", third, PERIOD, CADENCE_TYPE_PERFECT_AUTHENTIC);
    contrast_motive(add_section(out, "B", "B", third, SENTENCE, CADENCE_TYPE_PERFECT_AUTHENTIC));
    returns_to(out, add_section(out, "A", "A return", third, PERIOD, CADENCE_TYPE_PERFECT_AUTHENTIC), 0, PLAN_RETURN_EXACT);
    return true;
}

#define VARIATIONS_DEFAULT 3
#define VARIATIONS_MAX 8

/*
 * Theme and variations: the theme, then n variations of 8 bars, each returning the theme. Variation k's motive is m0
 * diminished (k odd) or with an interval changed (k even), so each sounds unlike the one before; the return is an
 * ornament of the theme for a diminution and a reharmonization for an interval change (B6 makes them so).
 */
static bool variations(size_t bars, size_t count, FormTemplate *out) {
    if (count == 0 && bars == 0) {
        count = VARIATIONS_DEFAULT;
    } else if (count == 0) {
        if (bars % 8 != 0 || bars < 16) {
            return false;
        }
        count = bars / 8 - 1;
    }
    if (count < 1 || count > VARIATIONS_MAX || (bars != 0 && bars != 8 * (1 + count))) {
        return false;
    }
    add_section(out, "T", "theme", 8, PERIOD, CADENCE_TYPE_PERFECT_AUTHENTIC);
    for (size_t k = 1; k <= count; k++) {
        char letter[PLAN_SECTION_LETTER_MAX];
        char name[FORM_TEMPLATE_NAME_MAX];
        snprintf(letter, sizeof(letter), "V%zu", k);
        snprintf(name, sizeof(name), "variation %zu", k);
        FormTemplateSection *section = add_section(out, letter, name, 8, PERIOD, CADENCE_TYPE_PERFECT_AUTHENTIC);
        bool odd = k % 2 == 1;
        returns_to(out, section, 0, odd ? PLAN_RETURN_ORNAMENT : PLAN_RETURN_REHARMONIZE);
        section->motive = odd ? PLAN_TRANSFORM_DIMINISHED : PLAN_TRANSFORM_INTERVAL_CHANGED;
        section->motive_tonal = false;
    }
    return true;
}

/*
 * A loop for game and media music: it ends on a half cadence so its last bar leads back to its first. One section of
 * 8 bars, or A ending in an IAC and B ending in the HC.
 */
static bool loop(size_t bars, FormTemplate *out) {
    if (bars == 8) {
        add_section(out, "A", "loop", 8, SENTENCE, CADENCE_TYPE_HALF);
        return true;
    }
    if (bars == 16) {
        add_section(out, "A", "A", 8, SENTENCE, CADENCE_TYPE_IMPERFECT_AUTHENTIC);
        contrast_motive(add_section(out, "B", "B", 8, SENTENCE, CADENCE_TYPE_HALF));
        return true;
    }
    return false;
}

#define LAYERS_DEFAULT 4
#define LAYERS_MIN 2
#define LAYERS_MAX 4

/*
 * A layered form: one 8-bar section heard with more lanes each time, every later layer returning the first. The
 * masks build up from the bass: the bass alone, then with the accompaniment, then with the melody, and the last layer
 * everything. With fewer than four layers the last is everything and the ones before it follow the same order.
 */
static bool layered(size_t bars, size_t count, FormTemplate *out) {
    static const unsigned BUILD_UP[LAYERS_MAX - 1] = {
        PLAN_LAYER_BIT(PLAN_LANE_BASS),
        PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE),
        PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE) | PLAN_LAYER_BIT(PLAN_LANE_MELODY)};
    if (count == 0 && bars == 0) {
        count = LAYERS_DEFAULT;
    } else if (count == 0) {
        if (bars % 8 != 0) {
            return false;
        }
        count = bars / 8;
    }
    if (count < LAYERS_MIN || count > LAYERS_MAX || (bars != 0 && bars != 8 * count)) {
        return false;
    }
    for (size_t k = 1; k <= count; k++) {
        char letter[PLAN_SECTION_LETTER_MAX];
        char name[FORM_TEMPLATE_NAME_MAX];
        snprintf(letter, sizeof(letter), "L%zu", k);
        snprintf(name, sizeof(name), "layer %zu", k);
        FormTemplateSection *section = add_section(out, letter, name, 8, SENTENCE, CADENCE_TYPE_HALF);
        if (k > 1) {
            returns_to(out, section, 0, PLAN_RETURN_EXACT);
        }
        section->layers = k == count ? PLAN_LAYERS_ALL : BUILD_UP[k - 1];
    }
    return true;
}

bool form_template_layout(FormTemplateKind kind, size_t bars, size_t count, FormTemplate *out) {
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->kind = kind;
    bool fixed = kind != FORM_TEMPLATE_VARIATIONS && kind != FORM_TEMPLATE_LAYERED;
    if (fixed && count != 0) {
        return false;
    }
    if (bars == 0 && count == 0) {
        bars = form_template_default_bars(kind);
    }
    bool ok = false;
    switch (kind) {
    case FORM_TEMPLATE_BINARY:
        ok = binary(bars, out);
        break;
    case FORM_TEMPLATE_ROUNDED_BINARY:
        ok = rounded_binary(bars, out);
        break;
    case FORM_TEMPLATE_TERNARY:
        ok = ternary(bars, out);
        break;
    case FORM_TEMPLATE_VARIATIONS:
        ok = variations(bars, count, out);
        break;
    case FORM_TEMPLATE_LOOP:
        ok = loop(bars, out);
        break;
    case FORM_TEMPLATE_LAYERED:
        ok = layered(bars, count, out);
        break;
    }
    if (!ok) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    out->bars = 0;
    for (size_t i = 0; i < out->section_count; i++) {
        out->bars += out->sections[i].bars;
    }
    return true;
}
