#include "plan/section.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "theory/vocabulary/cadence_type.h"

/* ------------------------------------------------------------- the name tables */

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* The name at an index of a table whose rows may be NULL (a "none" value has no name). */
static const char *name_at(const char *const *names, size_t count, int value) {
    return value >= 0 && (size_t)value < count ? names[value] : NULL;
}

/* The index of a name; a NULL row is never found. */
static bool find_in(const char *const *names, size_t count, const char *name, int *out) {
    for (size_t i = 0; name && i < count; i++) {
        if (names[i] && strcmp(names[i], name) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return false;
}

typedef struct {
    const char *name;
    const char *label;
    bool theme;
} RoleRow;

static const RoleRow ROLES[SECTION_ROLE_COUNT] = {
    {"theme", "Theme", true},
    {"contrast", "Contrast", true},
    {"return", "Return", true},
    {"variation", "Variation", true},
    {"layer", "Layer", true},
    {"pattern", "Pattern", true},
    {"sequence", "Sequence", true},
    {"episode", "Episode", true},
    {"codetta", "Codetta", true},
    {"liquidation", "Liquidation", true},
    {"transition", "Transition", true},
    {"retransition", "Retransition", true},
    {"subordinate", "Subordinate theme", true},
    {"coda", "Coda", true},
    {"elaboration", "Elaboration", true},
    {"intro", "Intro", false},
    {"body", "Loop body", false},
    {"turnaround", "Turnaround", false},
    {"ending", "Ending", false},
    {"material", "Material", false},
    {"seam", "Seam", false},
};

const char *section_role_name(SectionRole role) {
    return role >= 0 && role < SECTION_ROLE_COUNT ? ROLES[role].name : NULL;
}

const char *section_role_label(SectionRole role) {
    return role >= 0 && role < SECTION_ROLE_COUNT ? ROLES[role].label : NULL;
}

bool section_role_find(const char *name, SectionRole *out) {
    for (int role = 0; name && role < SECTION_ROLE_COUNT; role++) {
        if (strcmp(name, ROLES[role].name) == 0) {
            *out = (SectionRole)role;
            return true;
        }
    }
    return false;
}

bool section_role_is_theme(SectionRole role) {
    return role >= 0 && role < SECTION_ROLE_COUNT && ROLES[role].theme;
}

bool section_role_is_operator(SectionRole role) {
    return role == SECTION_ROLE_LIQUIDATION || role == SECTION_ROLE_RETRANSITION || role == SECTION_ROLE_CODA;
}

#define NAMED(kind, type, table)                                                                            \
    const char *section_##kind##_name(type value) { return name_at(table, COUNT_OF(table), (int)value); } \
    bool section_##kind##_find(const char *name, type *out) {                                               \
        int found;                                                                                          \
        if (!find_in(table, COUNT_OF(table), name, &found)) {                                               \
            return false;                                                                                   \
        }                                                                                                   \
        *out = (type)found;                                                                                 \
        return true;                                                                                        \
    }

static const char *const BEGINNINGS[] = {"presentation", "antecedent", "compound_basic_idea"};
static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};
static const char *const IDEAS[] = {"basic idea",
                                    "repetition",
                                    "contrasting idea",
                                    "consequent basic idea",
                                    "consequent contrasting idea",
                                    "fragment 1",
                                    "fragment 2",
                                    "cadential",
                                    "cadential 1",
                                    "cadential 2"};
static const char *const EXTENSION_KINDS[] = {"repeat", "stretch", "addition", "evaded"};
static const char *const CONSTRUCTION_REASONS[] = {"given", "motive", "length"};
static const char *const VARIATIONS[] = {"exact",  "ornament", "reharmonize", "reregister",
                                         "varied", "figurate", "embellish",   "setting"};
static const char *const SETTINGS[] = {NULL, "block", "arpeggiated", "alberti", "oom-pah"};
static const char *const MODES[] = {NULL, "parallel", "major", "home"};
static const char *const CONNECTIVES[] = {NULL, "sometimes", "always"};
static const char *const FIGURE_KINDS[] = {"neighbor", "scale", "broken-chord"};
static const char *const FIGURE_RHYTHMS[] = {"eighths", "sixteenths"};
static const char *const FUNCTIONS[] = {"transition", "retransition", "subordinate", "coda"};
static const char *const LANES[] = {"meter", "key", "harmony", "bass", "melody", "inner_voice"};

NAMED(beginning, SectionBeginning, BEGINNINGS)
NAMED(ending, SectionEnding, ENDINGS)
NAMED(idea, SectionIdea, IDEAS)
NAMED(extension_kind, SectionExtensionKind, EXTENSION_KINDS)
NAMED(construction_reason, SectionConstructionReason, CONSTRUCTION_REASONS)
NAMED(return_variation, SectionReturnVariation, VARIATIONS)
NAMED(setting, SectionSetting, SETTINGS)
NAMED(mode, SectionModeShift, MODES)
NAMED(connective, SectionConnective, CONNECTIVES)
NAMED(figure_kind, SectionFigureKind, FIGURE_KINDS)
NAMED(figure_rhythm, SectionFigureRhythm, FIGURE_RHYTHMS)
NAMED(function_kind, SectionFunctionKind, FUNCTIONS)
NAMED(lane, SectionLane, LANES)

/* ------------------------------------------------------------- the description */

void section_description_init(SectionDescription *description) {
    memset(description, 0, sizeof(*description));
    description->variation = SECTION_RETURN_EXACT;
    description->key_start.kind = SECTION_REGION_NONE;
    description->key_end.kind = SECTION_REGION_NONE;
    description->key_start.anchor = SECTION_ANCHOR_HOME;
    description->key_end.anchor = SECTION_ANCHOR_HOME;
}

/* ------------------------------------------------------------- the rules */

static bool fail(SectionError *error, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        snprintf(error->path, sizeof(error->path), "%s", path);
        va_start(args, format);
        vsnprintf(error->message, sizeof(error->message), format, args);
        va_end(args);
    }
    return false;
}

static bool has_keys(const SectionDescription *d) {
    return d->key_start.kind != SECTION_REGION_NONE || d->key_end.kind != SECTION_REGION_NONE;
}

/* One region: its kind is one the use allows and its fields fit its kind. */
static bool check_region(const SectionRegion *r, SectionUse use, const char *path, SectionError *error) {
    char at[96];
    switch (r->kind) {
    case SECTION_REGION_NONE:
    case SECTION_REGION_HOME:
        break;
    case SECTION_REGION_FORM:
        if (use == SECTION_USE_REQUEST || use == SECTION_USE_OVERLAY) {
            return fail(error, path, "a request names home, a tonic and material, or a relation");
        }
        if (r->form_region[0] == '\0') {
            return fail(error, path, "a form region needs its name");
        }
        break;
    case SECTION_REGION_KEY:
        if (use == SECTION_USE_FORM) {
            return fail(error, path, "a form's keys are home or a region of the form");
        }
        if (r->tonic[0] == '\0' || r->material[0] == '\0') {
            return fail(error, path, "a key needs a tonic and a material");
        }
        break;
    case SECTION_REGION_RELATION:
        if (use == SECTION_USE_FORM) {
            return fail(error, path, "a form's keys are home or a region of the form");
        }
        if (r->relation[0] == '\0') {
            return fail(error, path, "a relation needs its name");
        }
        if (r->anchor == SECTION_ANCHOR_SECTION && r->anchor_section[0] == '\0') {
            snprintf(at, sizeof(at), "%s/anchor", path);
            return fail(error, at, "an anchor that is a section names it");
        }
        if (r->anchor != SECTION_ANCHOR_SECTION && r->anchor_section[0] != '\0') {
            snprintf(at, sizeof(at), "%s/anchor", path);
            return fail(error, at, "only an anchor that is a section has a section name");
        }
        break;
    default:
        return fail(error, path, "unknown region");
    }
    return true;
}

static bool check_harmony(const SectionHarmony *h, SectionError *error) {
    char at[96];
    switch (h->kind) {
    case SECTION_HARMONY_NONE:
        return true;
    case SECTION_HARMONY_PROGRESSION:
        if (h->numerals[0] == '\0') {
            return fail(error, "/harmony/progression", "a progression has numerals");
        }
        if (h->cells > 16) {
            return fail(error, "/harmony/cells", "must be from 0 to 16");
        }
        return true;
    case SECTION_HARMONY_SCHEMA:
        if (h->schema[0] == '\0') {
            return fail(error, "/harmony/schema", "a schema has a name");
        }
        return true;
    case SECTION_HARMONY_CHORDS:
        if (h->chord_count < 1 || h->chord_count > SECTION_CHORD_BARS_MAX) {
            return fail(error, "/harmony/chords", "must hold 1 to %d bars", SECTION_CHORD_BARS_MAX);
        }
        for (size_t i = 0; i < h->chord_count; i++) {
            if (h->chords[i].bar < 1 || h->chords[i].bar > 4096) {
                snprintf(at, sizeof(at), "/harmony/chords/%zu/bar", i);
                return fail(error, at, "must be from 1 to 4096");
            }
            if (h->chords[i].text[0] == '\0') {
                snprintf(at, sizeof(at), "/harmony/chords/%zu/text", i);
                return fail(error, at, "a chord has its text");
            }
            for (size_t j = 0; j < i; j++) {
                if (h->chords[j].bar == h->chords[i].bar) {
                    snprintf(at, sizeof(at), "/harmony/chords/%zu/bar", i);
                    return fail(error, at, "bar %u is given twice", h->chords[i].bar);
                }
            }
        }
        return true;
    }
    return fail(error, "/harmony", "unknown harmony");
}

static bool check_construction(const SectionDescription *d, SectionUse use, SectionError *error) {
    const SectionConstruction *c = &d->construction;
    char at[96];
    if (!d->has_construction) {
        return true;
    }
    if (d->has_role && !section_role_is_theme(d->role)) {
        return fail(error, "/construction", "only a theme-like section has a construction");
    }
    if ((use == SECTION_USE_FORM || use == SECTION_USE_REQUEST) && d->bars == 0) {
        return fail(error, "/construction", "a construction needs bars, not beats");
    }
    if (d->construction_auto) {
        if (use != SECTION_USE_FORM) {
            return fail(error, "/construction", "\"auto\" is for forms");
        }
        return true;
    }
    for (int idea = 0; idea < SECTION_CONSTRUCTION_IDEAS; idea++) {
        if (c->idea_bars[idea] > 16) {
            snprintf(at, sizeof(at), "/construction/ideas/%s", IDEAS[idea]);
            return fail(error, at, "must be a whole number of bars from 0 to 16");
        }
    }
    if (c->extension_count > SECTION_CONSTRUCTION_EXTENSIONS) {
        return fail(error, "/construction/extensions", "at most %d extensions", SECTION_CONSTRUCTION_EXTENSIONS);
    }
    for (size_t i = 0; i < c->extension_count; i++) {
        const SectionExtension *x = &c->extensions[i];
        if ((int)x->kind < 0 || (size_t)x->kind >= COUNT_OF(EXTENSION_KINDS) || x->idea >= SECTION_CONSTRUCTION_IDEAS ||
            x->bars > 4) {
            snprintf(at, sizeof(at), "/construction/extensions/%zu", i);
            return fail(error, at, "an extension is a kind, an idea and 0 to 4 bars");
        }
    }
    if (c->reason != SECTION_CONSTRUCTION_GIVEN && use != SECTION_USE_PLAN) {
        return fail(error, "/construction/reason", "\"reason\" is not given here");
    }
    return true;
}

/* The members a request and an overlay hold; every other member is refused. */
static bool refuse_others(const SectionDescription *d, SectionUse use, SectionError *error) {
    const struct {
        bool present;
        const char *path;
        unsigned allowed; /* the uses that hold the member */
    } ROWS[] = {
        {d->name[0] != '\0', "/name", SECTION_USE_OVERLAY},
        {d->id[0] != '\0', "/id", SECTION_USE_REQUEST},
        {d->letter[0] != '\0', "/letter", SECTION_USE_REQUEST},
        {d->has_role, "/role", SECTION_USE_REQUEST},
        {d->bars != 0, "/bars", SECTION_USE_REQUEST},
        {d->beats != 0, "/beats", SECTION_USE_REQUEST},
        {d->has_pickup, "/pickup", SECTION_USE_REQUEST},
        {d->has_pairing, "/beginning", 0},
        {d->has_cadence, "/cadence", SECTION_USE_REQUEST},
        {d->has_first_cadence, "/first_cadence", 0},
        {d->closed, "/closed", 0},
        {d->motive[0] != '\0', "/motive", 0},
        {d->returns[0] != '\0', "/returns", 0},
        {d->variation != SECTION_RETURN_EXACT, "/variation", 0},
        {d->setting != SECTION_SETTING_NONE, "/setting", 0},
        {d->mode != SECTION_MODE_NONE, "/mode", 0},
        {d->layers != 0, "/layers", 0},
        {d->pedal != 0, "/pedal", 0},
        {d->leads_in, "/leads_in", 0},
        {d->connective != SECTION_CONNECTIVE_NONE, "/connective", 0},
        {d->connective_written, "/connective_written", 0},
        {d->has_construction, "/construction", 0},
        {d->has_figure, "/figure", 0},
        {d->has_function, "/function", SECTION_USE_REQUEST},
        {d->function.independent, "/independent", 0},
        {d->function.lyric, "/lyric", 0},
        {d->free_phrase, "/free_phrase", 0},
    };
    for (size_t i = 0; i < COUNT_OF(ROWS); i++) {
        if (ROWS[i].present && !(ROWS[i].allowed & (unsigned)use)) {
            return fail(error, ROWS[i].path, "\"%s\" is not given here", ROWS[i].path + 1);
        }
    }
    return true;
}

static bool check_use(const SectionDescription *d, SectionUse use, SectionError *error) {
    switch (use) {
    case SECTION_USE_FORM:
        if (d->name[0] == '\0') {
            return fail(error, "/name", "required member is missing");
        }
        if (d->letter[0] == '\0') {
            return fail(error, "/letter", "required member is missing");
        }
        if (!d->has_role) {
            return fail(error, "/role", "required member is missing");
        }
        if (d->role >= SECTION_ROLE_FORM_COUNT) {
            return fail(error, "/role", "\"%s\" is not a role of a form", section_role_name(d->role));
        }
        /* an operator role refuses beats by name below, so a section that gives both is refused there */
        if ((d->bars != 0) == (d->beats != 0) && !(d->bars != 0 && section_role_is_operator(d->role))) {
            return fail(error, "", d->bars ? "give bars or beats, not both" : "give bars or beats");
        }
        if (!d->has_cadence) {
            return fail(error, "/cadence", "required member is missing");
        }
        if (d->id[0]) {
            return fail(error, "/id", "\"id\" is not given here");
        }
        if (d->has_pickup) {
            return fail(error, "/pickup", "\"pickup\" is not given here");
        }
        if (d->has_first_cadence) {
            return fail(error, "/first_cadence", "\"first_cadence\" is not given here");
        }
        if (d->connective_written) {
            return fail(error, "/connective_written", "\"connective_written\" is not given here");
        }
        if (d->free_phrase) {
            return fail(error, "/free_phrase", "\"free_phrase\" is not given here");
        }
        if (d->harmony.kind != SECTION_HARMONY_NONE) {
            return fail(error, "/harmony", "\"harmony\" is not given here");
        }
        if ((d->layers & ~(SECTION_LAYER_BIT(SECTION_LANE_BASS) | SECTION_LAYER_BIT(SECTION_LANE_INNER_VOICE) |
                           SECTION_LAYER_BIT(SECTION_LANE_MELODY))) &&
            d->layers != SECTION_LAYERS_ALL) {
            return fail(error, "/layers", "unknown lane (bass, inner_voice, melody)");
        }
        if (d->has_pairing == section_role_is_operator(d->role)) {
            const char *noun = section_role_name(d->role);
            return d->has_pairing ? fail(error, "/beginning", "a %s has no beginning or ending", noun)
                                  : fail(error, "/beginning", "required member is missing");
        }
        return true;
    case SECTION_USE_PLAN:
        if (d->name[0]) {
            return fail(error, "/name", "\"name\" is not given here");
        }
        if (d->motive[0]) {
            return fail(error, "/motive", "\"motive\" is not given here");
        }
        return true;
    case SECTION_USE_REQUEST:
        if (d->id[0] == '\0') {
            return fail(error, "/id", "required member is missing");
        }
        if (!d->has_role) {
            return fail(error, "/role", "required member is missing");
        }
        if (d->role < SECTION_ROLE_INTRO || d->role > SECTION_ROLE_ENDING) {
            return fail(error, "/role", "a request names intro, body, turnaround or ending");
        }
        if ((d->bars != 0) == (d->beats != 0)) {
            return fail(error, "", d->bars ? "give bars or beats, not both" : "give bars or beats");
        }
        if (d->has_function && d->function.function != SECTION_FUNCTION_TRANSITION &&
            d->function.function != SECTION_FUNCTION_CODA) {
            return fail(error, "/function", "a request takes transition or coda");
        }
        return refuse_others(d, use, error);
    case SECTION_USE_OVERLAY:
        if (d->name[0] == '\0') {
            return fail(error, "/name", "required member is missing");
        }
        return refuse_others(d, use, error);
    }
    return fail(error, "", "unknown use");
}

/* The first member an operator role refuses that the description holds; retransition_or_coda adds the wider list. */
static const char *operator_refusal(const SectionDescription *d, bool retransition_or_coda) {
    const struct {
        bool present;
        const char *name;
        bool wide; /* refused by a retransition or a coda only */
    } ROWS[] = {
        {d->has_construction, "construction", false},
        {d->motive[0] != '\0', "motive", false},
        {d->returns[0] != '\0', "returns", false},
        {d->variation != SECTION_RETURN_EXACT, "variation", false},
        {d->pedal != 0, "pedal", false},
        {d->leads_in, "leads_in", false},
        {d->connective != SECTION_CONNECTIVE_NONE, "connective", false},
        {d->layers != 0, "layers", false},
        {d->beats != 0, "beats", false},
        {d->has_figure, "figure", false},
        {d->mode != SECTION_MODE_NONE, "mode", true},
        {has_keys(d), "keys", true},
        {d->function.independent, "independent", true},
        {d->function.lyric, "lyric", true},
    };
    for (size_t i = 0; i < COUNT_OF(ROWS); i++) {
        if (ROWS[i].present && (!ROWS[i].wide || retransition_or_coda)) {
            return ROWS[i].name;
        }
    }
    return NULL;
}

bool section_description_check(const SectionDescription *d, SectionUse use, SectionError *error) {
    char at[96];
    if (!check_use(d, use, error)) {
        return false;
    }
    /* regions and harmony */
    if (use == SECTION_USE_REQUEST || use == SECTION_USE_OVERLAY) {
        if (!check_region(&d->key_end, use, "/region", error) || !check_region(&d->key_start, use, "/region", error)) {
            return false;
        }
    } else if (!check_region(&d->key_start, use, "/keys/start", error) || !check_region(&d->key_end, use, "/keys/end", error)) {
        return false;
    }
    if (!check_harmony(&d->harmony, error)) {
        return false;
    }
    if (use == SECTION_USE_REQUEST || use == SECTION_USE_OVERLAY) {
        if (d->has_pickup && rational_is_negative(d->pickup)) {
            return fail(error, "/pickup", "a pickup is not negative");
        }
        if (d->has_cadence && (d->cadence < 0 || d->cadence >= CADENCE_TYPE_UNKNOWN)) {
            return fail(error, "/cadence", "unknown cadence");
        }
        return true;
    }
    if (d->has_cadence && (d->cadence < 0 || d->cadence >= CADENCE_TYPE_UNKNOWN)) {
        return fail(error, "/cadence", "unknown cadence");
    }
    if (d->has_first_cadence && (d->first_cadence < 0 || d->first_cadence >= CADENCE_TYPE_UNKNOWN)) {
        return fail(error, "/first_cadence", "unknown cadence");
    }
    if (d->layers & ~SECTION_LAYERS_ALL) {
        return fail(error, "/layers", "unknown lane");
    }

    /* the roles with an operator of their own, and the transition */
    if (d->has_role && section_role_is_operator(d->role)) {
        const char *refused = operator_refusal(d, d->role != SECTION_ROLE_LIQUIDATION);
        if (refused) {
            snprintf(at, sizeof(at), "/%s", refused);
            return fail(error, at, "a %s has no %s", section_role_name(d->role), refused);
        }
    }
    if (d->has_role && d->role == SECTION_ROLE_TRANSITION) {
        const struct {
            bool present;
            const char *name;
        } ROWS[] = {{d->returns[0] != '\0', "returns"},
                    {d->variation != SECTION_RETURN_EXACT, "variation"},
                    {d->has_figure, "figure"},
                    {d->mode != SECTION_MODE_NONE, "mode"},
                    {d->pedal != 0, "pedal"}};
        for (size_t i = 0; i < COUNT_OF(ROWS); i++) {
            if (ROWS[i].present) {
                snprintf(at, sizeof(at), "/%s", ROWS[i].name);
                return fail(error, at, "a transition has no %s", ROWS[i].name);
            }
        }
    }

    /* the function: a form's role implies it; independent and lyric */
    if (d->has_function && use == SECTION_USE_FORM && d->has_role) {
        const SectionRole roles[] = {SECTION_ROLE_TRANSITION, SECTION_ROLE_RETRANSITION, SECTION_ROLE_SUBORDINATE,
                                     SECTION_ROLE_CODA};
        for (size_t i = 0; i < COUNT_OF(roles); i++) {
            if (d->role == roles[i] && (size_t)d->function.function != i) {
                return fail(error, "/function", "the function of a %s is %s", section_role_name(d->role),
                            section_function_kind_name((SectionFunctionKind)i));
            }
        }
    }
    if (d->function.independent && !(d->has_function && d->function.function == SECTION_FUNCTION_TRANSITION)) {
        return fail(error, "/independent", "only a transition is independent");
    }
    if (d->function.lyric && !(d->has_function && d->function.function == SECTION_FUNCTION_SUBORDINATE)) {
        return fail(error, "/lyric", "only a subordinate theme is lyric");
    }
    if (d->function.independent && d->motive[0]) {
        return fail(error, "/motive", "an independent transition has its own motive");
    }
    if (d->function.lyric) {
        if (d->motive[0]) {
            return fail(error, "/motive", "a lyric theme's motive is m0 augmented");
        }
        if (d->returns[0]) {
            return fail(error, "/returns", "a lyric theme returns nothing");
        }
        if (d->has_pairing &&
            (d->pairing.beginning != SECTION_BEGINNING_ANTECEDENT || d->pairing.ending != SECTION_ENDING_CONSEQUENT)) {
            return fail(error, "/beginning", "a lyric theme is a period");
        }
    }

    /* the construction */
    if (!check_construction(d, use, error)) {
        return false;
    }

    /* returns, variation and setting */
    const bool returns = d->returns[0] != '\0';
    if (d->variation < SECTION_RETURN_EXACT || d->variation > SECTION_RETURN_SETTING) {
        return fail(error, "/variation", "unknown variation");
    }
    if (d->variation != SECTION_RETURN_EXACT && !returns) {
        return fail(error, "/variation", "a variation needs returns");
    }
    if (d->setting != SECTION_SETTING_NONE && !returns) {
        return fail(error, "/setting", "a setting needs returns");
    }
    if (d->setting != SECTION_SETTING_NONE && d->variation != SECTION_RETURN_SETTING) {
        return fail(error, "/setting", "only a setting return has a setting");
    }
    if (returns && d->variation == SECTION_RETURN_SETTING && d->setting == SECTION_SETTING_NONE) {
        return fail(error, "/variation", "a setting return names its setting");
    }

    /* the mode */
    const bool figurate = returns && d->variation == SECTION_RETURN_FIGURATE;
    switch (d->mode) {
    case SECTION_MODE_NONE:
        break;
    case SECTION_MODE_HOME:
        if (has_keys(d)) {
            return fail(error, "/mode", "a section in the home mode has no keys");
        }
        if (!returns) {
            return fail(error, "/mode", "the home mode is for a returning section");
        }
        if (d->variation != SECTION_RETURN_REHARMONIZE) {
            return fail(error, "/mode", "a return in the home mode is reharmonized");
        }
        break;
    case SECTION_MODE_PARALLEL:
        if (has_keys(d)) {
            return fail(error, "/mode", "a section in the parallel mode has no keys");
        }
        if (!figurate) {
            return fail(error, "/mode", "the parallel mode needs a figurate variation");
        }
        break;
    case SECTION_MODE_TONIC_MAJOR:
        if (has_keys(d)) {
            return fail(error, "/mode", "a section in the tonic major has no keys");
        }
        if (!returns) {
            return fail(error, "/mode", "the tonic major is for a returning section");
        }
        if (figurate) {
            return fail(error, "/mode", "a figurate variation takes the parallel mode, not the tonic major");
        }
        break;
    default:
        return fail(error, "/mode", "unknown mode");
    }

    /* the figure */
    if (d->has_figure) {
        if (!figurate) {
            return fail(error, "/figure", "a figure needs the variation figurate");
        }
        if (use == SECTION_USE_FORM && d->bars == 0) {
            return fail(error, "/figure", "a figure needs bars");
        }
        if (d->mode == SECTION_MODE_PARALLEL && !d->figure.reharmonize) {
            return fail(error, "/figure/harmony", "a variation in the other mode is reharmonized");
        }
        if (d->figure.change_at != 0) {
            if (!d->figure.reharmonize) {
                return fail(error, "/figure/change_at", "a root change needs the harmony reharmonized");
            }
            if (d->bars != 0 && d->figure.change_at >= d->bars) {
                return fail(error, "/figure/change_at", "change_at must be a bar of the section before its last");
            }
        }
    } else if (figurate) {
        return fail(error, "/variation", "a figurate variation needs a figure");
    }

    /* the pedal, the lead-in and the connective */
    if (d->pedal != 0 && d->pedal != 5) {
        return fail(error, "/pedal", "a pedal is dominant (5)");
    }
    if (d->pedal == 5 && !(d->has_pairing && d->pairing.beginning == SECTION_BEGINNING_PRESENTATION)) {
        return fail(error, "/pedal", "a pedal needs a presentation beginning");
    }
    if (d->leads_in && returns) {
        return fail(error, "/leads_in", "a section that returns another cannot lead in");
    }
    if (d->connective != SECTION_CONNECTIVE_NONE && !d->leads_in) {
        return fail(error, "/connective", "a connective needs leads_in");
    }
    return true;
}
