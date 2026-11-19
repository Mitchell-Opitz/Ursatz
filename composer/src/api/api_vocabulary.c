#include <stdio.h>
#include <string.h>

#include "theory/style/style_labels.h"
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_random.h>
#include <ursatz_composer/motive_tones.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/preferences.h>
#include <ursatz_composer/variant_registry.h>

#include "api_grow.h"
#include "grow/grow_internal.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "api_vocabulary.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

/* --- the tables ------------------------------------------------------------------------------------------ */

const ApiChoice API_METERS[] = {{"2/4", "2/4"}, {"3/4", "3/4"}, {"4/4", "4/4"}, {"6/8", "6/8"}, {NULL, NULL}};
const ApiChoice API_FUNCTIONS[] = {{"beginning", "Beginning"},
                                   {"continuation", "Continuation"},
                                   {"contrast", "Contrast"},
                                   {"cadential", "Cadential"},
                                   {NULL, NULL}};
const ApiChoice API_DISTANCES[] = {{"close", "Close"}, {"medium", "Medium"}, {"far", "Far"}, {NULL, NULL}};
const ApiChoice API_THEME_POOLS[] = {{"approved-and-unflagged", "Approved and unflagged"},
                                     {"approved", "Approved only"},
                                     {"none", "None"},
                                     {NULL, NULL}};
const ApiChoice API_ACCOMPANIMENT_PATTERNS[] = {{"any", "Any pattern"},       {"block", "Block"},
                                                {"arpeggiated", "Arpeggiated"}, {"alberti", "Alberti"},
                                                {"oom-pah", "Oom-pah"},       {"motive", "From a motive"},
                                                {NULL, NULL}};
const ApiChoice API_PIECE_INTOS[] = {{"sentence", "Sentence"}, {"period", "Period"}, {"hybrid", "Hybrid"}, {NULL, NULL}};
/* The pickups an application offers, in beats of the meter: a pickup is also accepted when it is any other length on the
   meter's grid (a multiple of 1/32 of a whole note, or a triplet of the beat or the division) shorter than a bar. */
const ApiChoice API_PICKUPS[] = {{"0", "0"},   {"1/4", "1/4"}, {"1/2", "1/2"}, {"1", "1"},
                                 {"3/2", "3/2"}, {"2", "2"},   {"3", "3"},     {NULL, NULL}};

const ApiChoice API_THEME_BARS[] = {{"3", "3 bars"},   {"4", "4 bars"},   {"5", "5 bars"},   {"6", "6 bars"},   {"7", "7 bars"},
                                   {"8", "8 bars"},   {"9", "9 bars"},   {"10", "10 bars"}, {"11", "11 bars"}, {"12", "12 bars"},
                                   {"13", "13 bars"}, {"14", "14 bars"}, {"15", "15 bars"}, {"16", "16 bars"}, {NULL, NULL}};
const ApiChoice API_STYLE_WEIGHTS[] = {{"1/4", "Light"}, {"1/2", "Normal"}, {"1", "Strong"}, {NULL, NULL}};

/* The beginning and ending words of a hybrid ("antecedent+continuation") with the ideas each is made of (grow_idea_place). */
const ApiThemePart API_THEME_BEGINNINGS[] = {{"presentation", "Presentation", {GROW_IDEA_BASIC, GROW_IDEA_REPETITION}, 2},
                                             {"antecedent", "Antecedent", {GROW_IDEA_BASIC, GROW_IDEA_CONTRASTING}, 2},
                                             {"cbi", "Compound basic idea", {GROW_IDEA_BASIC, GROW_IDEA_CONTRASTING}, 2},
                                             {NULL, NULL, {0}, 0}};
const ApiThemePart API_THEME_ENDINGS[] = {
    {"continuation", "Continuation", {GROW_IDEA_FRAGMENT_1, GROW_IDEA_FRAGMENT_2, GROW_IDEA_CADENTIAL}, 3},
    {"consequent", "Consequent", {GROW_IDEA_CONSEQUENT_BASIC, GROW_IDEA_CONSEQUENT_CONTRASTING}, 2},
    {"cadential", "Cadential", {GROW_IDEA_CADENTIAL_1, GROW_IDEA_CADENTIAL_2}, 2},
    {NULL, NULL, {0}, 0}};

bool api_theme_cadence_ok(int cadence_type) {
    return cadence_type == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence_type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
           cadence_type == CADENCE_TYPE_HALF;
}

size_t api_choice_count(const ApiChoice *table) {
    size_t count = 0;
    while (table[count].value) {
        count++;
    }
    return count;
}

int api_choice_find(const ApiChoice *table, const char *text) {
    for (size_t i = 0; text && table[i].value; i++) {
        if (strcmp(table[i].value, text) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void api_choice_list(const ApiChoice *table, char *out, size_t size) {
    api_choice_join(table, ", ", " or ", out, size);
}

void api_choice_join(const ApiChoice *table, const char *separator, const char *last_separator, char *out, size_t size) {
    size_t count = api_choice_count(table);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count && used < size; i++) {
        const char *between = i == 0 ? "" : i + 1 == count ? last_separator : separator;
        int written = snprintf(out + used, size - used, "%s%s", between, table[i].value);
        used += written > 0 ? (size_t)written : 0;
    }
}

ComposerStatus api_fail_meter(ComposerError *error, const char *path, const char *text) {
    char list[64];
    api_choice_list(API_METERS, list, sizeof list);
    return api_fail(error, COMPOSER_ERR_REQUEST, path, "meter '%s' is not supported; use %s", text, list);
}

/* --- labels of the lists the library holds ----------------------------------------------------------------- */

/* In the order of plan_cadence_names (PAC IAC HC DC PC). */
static const char *const CADENCE_LABELS[PLAN_CADENCE_COUNT] = {"Perfect authentic cadence", "Imperfect authentic cadence",
                                                               "Half cadence", "Deceptive cadence", "Plagal cadence"};
static const char *const MOTIVE_FORM_LABELS[] = {"Full", "Rhythm only", "Contour only"};
static const char *const ROLE_LABELS[] = {"Melodic", "Accompaniment"};
static const char *const RANDOM_STRATEGY_LABELS[] = {"Segments", "Drawn"};
static const char *const PART_LABELS[PIECE_PART_COUNT] = {"Intro", "Loop body", "Turnaround", "Ending"};
static const char *const FILL_LABELS[] = {"Empty", "Motif", "Theme"};

typedef struct {
    NonChordTone type;
    const char *label;
    const char *abbreviation;
} ToneLabel;

static const ToneLabel TONE_LABELS[NON_CHORD_TONE_COUNT] = {
    {NON_CHORD_TONE_PASSING, "Passing tone", "PT"},       {NON_CHORD_TONE_NEIGHBOR, "Neighbor tone", "NT"},
    {NON_CHORD_TONE_APPOGGIATURA, "Appoggiatura", "APP"}, {NON_CHORD_TONE_SUSPENSION, "Suspension", "SUS"},
    {NON_CHORD_TONE_RETARDATION, "Retardation", "RET"},   {NON_CHORD_TONE_ESCAPE_TONE, "Escape tone", "ET"},
    {NON_CHORD_TONE_ANTICIPATION, "Anticipation", "ANT"}, {NON_CHORD_TONE_DOUBLE_NEIGHBOR, "Double neighbor", "DN"},
    {NON_CHORD_TONE_CHANGING_TONE, "Changing tone", "CT2"}, {NON_CHORD_TONE_PEDAL, "Pedal", "PED"}};

typedef struct {
    const char *value;
    const char *label;
} MotiveCategory;

static const MotiveCategory MOTIVE_CATEGORIES[] = {{STYLE_CATEGORY_MOTIVE_DEGREES, "degrees"},
                                                   {STYLE_CATEGORY_MOTIVE_INTERVALS, "intervals"},
                                                   {STYLE_CATEGORY_MOTIVE_RHYTHM, "rhythm"},
                                                   {STYLE_CATEGORY_MOTIVE_NCT_FIGURE, "non-chord figures"}};

/* --- the document ---------------------------------------------------------------------------------------- */

static cJSON *add_list(cJSON *root, const char *name) {
    return cJSON_AddArrayToObject(root, name);
}

static cJSON *add_choice(cJSON *list, const char *value, const char *label) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry || !cJSON_AddStringToObject(entry, "value", value) || !cJSON_AddStringToObject(entry, "label", label) ||
        !cJSON_AddItemToArray(list, entry)) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

static bool add_choices(cJSON *root, const char *name, const ApiChoice *table) {
    cJSON *list = add_list(root, name);
    for (size_t i = 0; list && table[i].value; i++) {
        if (!add_choice(list, table[i].value, table[i].label)) {
            return false;
        }
    }
    return list != NULL;
}

/* The materials the Composer composes in, as {value: the material's id, label: its key word}; a piece's list adds "style". */
static bool add_materials(cJSON *root, const CommonPracticeMaterials *materials, const char *name, bool with_style) {
    cJSON *list = add_list(root, name);
    return list && add_choice(list, materials->major->id, materials->major->key_word) &&
           add_choice(list, materials->natural_minor->id, materials->natural_minor->key_word) &&
           (!with_style || add_choice(list, "style", "From the brief"));
}

/* The meters: the value, the beats of a bar and where a second chord may start (theme chords start on a cell of the meter). */
static bool add_meters(cJSON *root) {
    cJSON *list = add_list(root, "meters");
    for (size_t i = 0; list && API_METERS[i].value; i++) {
        PlanMeter meter;
        if (!request_parse_meter(API_METERS[i].value, &meter) || !harmony_meter_supported(&meter)) {
            continue;
        }
        cJSON *entry = add_choice(list, API_METERS[i].value, API_METERS[i].label);
        Rational cell;
        if (!entry || !cJSON_AddNumberToObject(entry, "beats", meter.beats_per_bar) ||
            !cJSON_AddStringToObject(entry, "chord_onset_step", "1/2")) {
            return false;
        }
        if (given_cell_length(&meter, &cell) && cell.num * (long long)meter.beat_unit / cell.den < meter.beats_per_bar &&
            !cJSON_AddNumberToObject(entry, "second_chord_beat", (double)(cell.num * (long long)meter.beat_unit / cell.den))) {
            return false;
        }
    }
    return list != NULL;
}

static bool add_keys(cJSON *root, const CommonPracticeMaterials *materials) {
    cJSON *keys = add_list(root, "keys");
    cJSON *names = add_list(root, "pitch_class_names");
    for (int minor = 0; keys && minor < 2; minor++) {
        for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
            char name[32];
            util_key_name(pitch_class, minor ? materials->natural_minor : materials->major, name, sizeof name);
            if (!add_choice(keys, name, name)) {
                return false;
            }
        }
    }
    for (int pitch_class = 0; names && pitch_class < 12; pitch_class++) {
        char major[32];
        char minor[32];
        util_key_name(pitch_class, materials->major, major, sizeof major);
        util_key_name(pitch_class, materials->natural_minor, minor, sizeof minor);
        *strchr(major, ' ') = '\0';
        *strchr(minor, ' ') = '\0';
        cJSON *entry = cJSON_CreateObject();
        cJSON *spellings = entry ? cJSON_CreateObject() : NULL;
        if (!entry || !spellings || !cJSON_AddNumberToObject(entry, "value", pitch_class) ||
            !cJSON_AddStringToObject(spellings, materials->major->id, major) ||
            !cJSON_AddStringToObject(spellings, materials->natural_minor->id, minor) ||
            !cJSON_AddItemToObject(entry, "names", spellings)) {
            cJSON_Delete(spellings);
            cJSON_Delete(entry);
            return false;
        }
        if (!cJSON_AddItemToArray(names, entry)) {
            cJSON_Delete(entry);
            return false;
        }
    }
    return keys && names;
}

static bool add_cadences(cJSON *root) {
    cJSON *all = add_list(root, "cadences");
    cJSON *theme = add_list(root, "theme_cadences");
    for (size_t i = 0; all && theme && i < PLAN_CADENCE_COUNT; i++) {
        if (!add_choice(all, plan_cadence_names[i], CADENCE_LABELS[i])) {
            return false;
        }
        if (api_theme_cadence_ok(plan_cadence_types[i]) && !add_choice(theme, plan_cadence_names[i], CADENCE_LABELS[i])) {
            return false;
        }
    }
    return all && theme;
}

/* The pickups of each meter: the offered values the readers accept (a number of beats shorter than a bar, on the meter's grid). */
static bool add_pickups(cJSON *root) {
    cJSON *pickups = cJSON_AddObjectToObject(root, "pickups");
    for (size_t m = 0; pickups && API_METERS[m].value; m++) {
        PlanMeter meter;
        if (!request_parse_meter(API_METERS[m].value, &meter) || !harmony_meter_supported(&meter)) {
            continue;
        }
        cJSON *list = cJSON_AddArrayToObject(pickups, API_METERS[m].value);
        for (size_t i = 0; list && API_PICKUPS[i].value; i++) {
            Rational pickup;
            if (!request_parse_pickup(API_PICKUPS[i].value, meter.beat_unit, &pickup) ||
                rational_cmp(pickup, (Rational){meter.beats_per_bar, meter.beat_unit}) >= 0 ||
                !plan_meter_pickup_on_grid(meter, pickup)) {
                continue;
            }
            cJSON *text = cJSON_CreateString(API_PICKUPS[i].value);
            if (!text || !cJSON_AddItemToArray(list, text)) {
                cJSON_Delete(text);
                return false;
            }
        }
        if (!list) {
            return false;
        }
    }
    return pickups != NULL;
}

/* The functions a game loop section can be (R-510): the name a request gives, its label and the parts it can be in. */
typedef struct {
    PieceFunction function;
    const char *label;
    const PiecePart *parts;
    size_t part_count;
} FunctionChoice;

static const PiecePart TRANSITION_PARTS[] = {PIECE_PART_INTRO, PIECE_PART_BODY};
static const PiecePart CODA_PARTS[] = {PIECE_PART_ENDING};
static const FunctionChoice FUNCTION_CHOICES[] = {
    {PIECE_FUNCTION_TRANSITION, "Transition", TRANSITION_PARTS, 2},
    {PIECE_FUNCTION_CODA, "Coda", CODA_PARTS, 1},
};

static bool add_piece_functions(cJSON *root) {
    cJSON *list = add_list(root, "piece_functions");
    for (size_t i = 0; list && i < sizeof FUNCTION_CHOICES / sizeof *FUNCTION_CHOICES; i++) {
        const FunctionChoice *choice = &FUNCTION_CHOICES[i];
        cJSON *entry = cJSON_CreateObject();
        cJSON *parts = cJSON_CreateArray();
        bool ok = entry && parts && cJSON_AddStringToObject(entry, "name", piece_function_name(choice->function)) &&
                  cJSON_AddStringToObject(entry, "label", choice->label);
        for (size_t p = 0; ok && p < choice->part_count; p++) {
            cJSON *name = cJSON_CreateString(piece_part_name(choice->parts[p]));
            ok = name && cJSON_AddItemToArray(parts, name);
            if (!ok) {
                cJSON_Delete(name);
            }
        }
        ok = ok && cJSON_AddItemToObject(entry, "parts", parts);
        if (ok) {
            parts = NULL;
            ok = cJSON_AddItemToArray(list, entry);
        }
        if (!ok) {
            cJSON_Delete(parts);
            cJSON_Delete(entry);
            return false;
        }
    }
    return list != NULL;
}

static bool add_library_lists(cJSON *root) {
    cJSON *forms = add_list(root, "motive_forms");
    cJSON *strategies = add_list(root, "random_motive_strategies");
    cJSON *roles = add_list(root, "roles");
    cJSON *parts = add_list(root, "piece_parts");
    if (!forms || !strategies || !roles || !parts || !add_piece_functions(root)) {
        return false;
    }
    cJSON *fills = add_list(root, "fill_kinds");
    if (!fills) {
        return false;
    }
    for (int form = MOTIVE_SKETCH_FORM_FULL; form <= MOTIVE_SKETCH_FORM_CONTOUR; form++) {
        if (!add_choice(forms, motive_form_name((MotiveSketchForm)form), MOTIVE_FORM_LABELS[form - MOTIVE_SKETCH_FORM_FULL])) {
            return false;
        }
    }
    for (int strategy = MOTIVE_RANDOM_STRATEGY_SEGMENTS; strategy <= MOTIVE_RANDOM_STRATEGY_DRAWN; strategy++) {
        if (!add_choice(strategies, motive_random_strategy_name((MotiveRandomStrategy)strategy),
                        RANDOM_STRATEGY_LABELS[strategy - MOTIVE_RANDOM_STRATEGY_SEGMENTS])) {
            return false;
        }
    }
    for (int accompaniment = 0; accompaniment < 2; accompaniment++) {
        if (!add_choice(roles, motive_role_name(accompaniment != 0), ROLE_LABELS[accompaniment])) {
            return false;
        }
    }
    for (int part = 0; part < PIECE_PART_COUNT; part++) {
        if (!add_choice(parts, piece_part_name((PiecePart)part), PART_LABELS[part])) {
            return false;
        }
    }
    for (int fill = PIECE_FILL_EMPTY; fill <= PIECE_FILL_THEME; fill++) {
        if (!add_choice(fills, piece_fill_name((PieceFillKind)fill), FILL_LABELS[fill])) {
            return false;
        }
    }
    return true;
}

static bool add_tone_labels(cJSON *root) {
    cJSON *list = add_list(root, "tone_labels");
    cJSON *chord = list ? add_choice(list, "chord", "Chord tone") : NULL;
    if (!chord || !cJSON_AddStringToObject(chord, "abbreviation", "CT")) {
        return false;
    }
    for (size_t i = 0; i < NON_CHORD_TONE_COUNT; i++) {
        cJSON *entry = add_choice(list, motive_tone_type_name(TONE_LABELS[i].type), TONE_LABELS[i].label);
        if (!entry || !cJSON_AddStringToObject(entry, "abbreviation", TONE_LABELS[i].abbreviation)) {
            return false;
        }
    }
    return true;
}

/* The labels of the slots, in GrowSlot order; the slot names are the theme shape table's. The basic idea is the one slot every
   theme needs (R-430): the frontend reads it from here instead of naming it. */
static bool add_theme_slots(cJSON *root) {
    static const char *const LABELS[GROW_SLOT_COUNT] = {"Basic idea", "Contrasting idea", "Continuation", "Cadential"};
    cJSON *list = add_list(root, "theme_slots");
    for (int slot = 0; list && slot < GROW_SLOT_COUNT; slot++) {
        cJSON *entry = add_choice(list, grow_slot_name((GrowSlot)slot), LABELS[slot]);
        if (!entry || !cJSON_AddBoolToObject(entry, "required", slot == GROW_SLOT_BASIC)) {
            return false;
        }
    }
    return list != NULL;
}

/*
 * The piece_intos: each archetype with the beginning and ending it is made of when it is made of exactly one pairing, found by
 * trying every pairing (R-430). A sentence is a presentation and a continuation, a period an antecedent and a consequent; a
 * hybrid is several pairings and gets neither.
 */
static bool add_piece_intos(cJSON *root) {
    cJSON *list = add_list(root, "piece_intos");
    for (int into = 0; list && API_PIECE_INTOS[into].value; into++) {
        cJSON *entry = add_choice(list, API_PIECE_INTOS[into].value, API_PIECE_INTOS[into].label);
        int found = 0;
        SectionPairing pairing = {0};
        for (int b = 0; b < 3; b++) {
            for (int e = 0; e < 3; e++) {
                SectionPairing candidate = {(SectionBeginning)b, (SectionEnding)e};
                if (grow_pairing_archetype(&candidate) == into) {
                    pairing = candidate;
                    found++;
                }
            }
        }
        if (!entry || (found == 1 && (!cJSON_AddStringToObject(entry, "beginning", API_THEME_BEGINNINGS[pairing.beginning].value) ||
                                      !cJSON_AddStringToObject(entry, "ending", API_THEME_ENDINGS[pairing.ending].value)))) {
            return false;
        }
    }
    return list != NULL;
}

static bool add_theme_lists(cJSON *root) {
    cJSON *ideas = add_list(root, "theme_ideas");
    cJSON *categories = add_list(root, "theme_motive_categories");
    for (int idea = 0; ideas && idea < GROW_IDEA_COUNT; idea++) {
        const char *name = grow_idea_name((GrowIdea)idea);
        cJSON *entry = add_choice(ideas, name, name);
        if (!entry || !cJSON_AddStringToObject(entry, "slot", grow_slot_name(theme_shape_idea_slot((GrowIdea)idea)))) {
            return false;
        }
    }
    for (size_t i = 0; categories && i < sizeof MOTIVE_CATEGORIES / sizeof MOTIVE_CATEGORIES[0]; i++) {
        if (!add_choice(categories, MOTIVE_CATEGORIES[i].value, MOTIVE_CATEGORIES[i].label)) {
            return false;
        }
    }
    return ideas && categories;
}

static bool add_theme_parts(cJSON *root, const char *name, const ApiThemePart *table) {
    cJSON *list = add_list(root, name);
    for (size_t i = 0; list && table[i].value; i++) {
        cJSON *entry = add_choice(list, table[i].value, table[i].label);
        cJSON *ideas = entry ? cJSON_AddArrayToObject(entry, "ideas") : NULL;
        for (size_t k = 0; ideas && k < table[i].idea_count; k++) {
            cJSON *idea = cJSON_CreateString(grow_idea_name(table[i].ideas[k]));
            if (!idea || !cJSON_AddItemToArray(ideas, idea)) {
                cJSON_Delete(idea);
                return false;
            }
        }
        if (!ideas) {
            return false;
        }
    }
    return list != NULL;
}

/* The lengths a theme has with no construction given: the others get one chosen (R-469). */
static bool add_regular_bars(cJSON *root) {
    cJSON *list = add_list(root, "theme_bars_regular");
    return list && cJSON_AddItemToArray(list, cJSON_CreateNumber(4)) && cJSON_AddItemToArray(list, cJSON_CreateNumber(8));
}

/* Whether some pairing has the idea and takes an extension of the kind on it. */
static bool extension_on_idea(SectionExtensionKind kind, GrowIdea idea) {
    for (int beginning = SECTION_BEGINNING_PRESENTATION; beginning <= SECTION_BEGINNING_COMPOUND_BASIC_IDEA; beginning++) {
        for (int ending = SECTION_ENDING_CONTINUATION; ending <= SECTION_ENDING_CADENTIAL; ending++) {
            SectionPairing pairing;
            memset(&pairing, 0, sizeof(pairing));
            pairing.beginning = (SectionBeginning)beginning;
            pairing.ending = (SectionEnding)ending;
            if (grow_extension_allowed(&pairing, kind, idea)) {
                return true;
            }
        }
    }
    return false;
}

/* The extensions of a construction (R-468) and, for a repeat and a stretch, the ideas each can follow. */
static bool add_theme_extensions(cJSON *root) {
    static const struct {
        const char *value;
        const char *label;
        SectionExtensionKind kind;
        bool names_idea;
    } KINDS[] = {{"repeat", "Internal repetition", SECTION_EXTENSION_REPEAT, true},
                 {"stretch", "Stretched cadence", SECTION_EXTENSION_STRETCH, true},
                 {"addition", "Addition after the cadence", SECTION_EXTENSION_ADDITION, false},
                 {"evaded", "Evaded cadence", SECTION_EXTENSION_EVADED, false}};
    cJSON *list = add_list(root, "theme_extensions");
    for (size_t i = 0; list && i < sizeof(KINDS) / sizeof(KINDS[0]); i++) {
        cJSON *entry = add_choice(list, KINDS[i].value, KINDS[i].label);
        cJSON *ideas = entry && KINDS[i].names_idea ? cJSON_AddArrayToObject(entry, "ideas") : NULL;
        if (!entry || (KINDS[i].names_idea && !ideas)) {
            return false;
        }
        for (int idea = 0; ideas && idea < GROW_IDEA_COUNT; idea++) {
            if (extension_on_idea(KINDS[i].kind, (GrowIdea)idea) &&
                !cJSON_AddItemToArray(ideas, cJSON_CreateString(grow_idea_name((GrowIdea)idea)))) {
                return false;
            }
        }
        if (KINDS[i].kind == SECTION_EXTENSION_REPEAT && !cJSON_AddBoolToObject(entry, "varied", true)) {
            return false;
        }
    }
    return list != NULL;
}

/* The first cadences of a period: the half and imperfect authentic cadences, in the order of the cadences list. */
static bool add_first_cadences(cJSON *root) {
    cJSON *list = add_list(root, "theme_first_cadences");
    for (size_t i = 0; list && i < PLAN_CADENCE_COUNT; i++) {
        if ((plan_cadence_types[i] == CADENCE_TYPE_HALF || plan_cadence_types[i] == CADENCE_TYPE_IMPERFECT_AUTHENTIC) &&
            !add_choice(list, plan_cadence_names[i], CADENCE_LABELS[i])) {
            return false;
        }
    }
    return list != NULL;
}

static bool add_operations(cJSON *root) {
    char *text = variant_registry_to_json();
    cJSON *operations = text ? cJSON_Parse(text) : NULL;
    cJSON_free(text);
    if (!operations) {
        return false;
    }
    if (!cJSON_AddItemToObject(root, "variant_operations", operations)) {
        cJSON_Delete(operations);
        return false;
    }
    return true;
}

/* The largest seed of each request that reads one: the variants entry keeps room for the seeds its chains draw. */
static bool add_seed_max(cJSON *limits) {
    cJSON *seed = cJSON_AddObjectToObject(limits, "seed_max");
    return seed && cJSON_AddNumberToObject(seed, "variants", (double)API_VARIANTS_SEED_MAX) &&
           cJSON_AddNumberToObject(seed, "random_motive", (double)API_SEED_MAX) &&
           cJSON_AddNumberToObject(seed, "theme", (double)GROW_SEED_MAX) &&
           cJSON_AddNumberToObject(seed, "piece", (double)PIECE_SEED_MAX);
}

/* The limits of the preferences document, as numbers (docs/preferences.md). */
static bool add_preference_limits(cJSON *limits) {
    cJSON *item = cJSON_AddObjectToObject(limits, "preferences");
    return item && cJSON_AddNumberToObject(item, "weight_max", COMPOSER_PREFERENCES_WEIGHT_MAX) &&
           cJSON_AddNumberToObject(item, "pickup_one_in_max", COMPOSER_PREFERENCES_PICKUP_ONE_IN_MAX) &&
           cJSON_AddNumberToObject(item, "accented_one_in_max", COMPOSER_PREFERENCES_ACCENTED_ONE_IN_MAX) &&
           cJSON_AddNumberToObject(item, "long_nct_one_in_max", COMPOSER_PREFERENCES_LONG_NCT_ONE_IN_MAX) &&
           cJSON_AddNumberToObject(item, "leaps_per_motif_max", COMPOSER_PREFERENCES_LEAPS_PER_MOTIF_MAX) &&
           cJSON_AddNumberToObject(item, "syncopation_one_in_max", COMPOSER_PREFERENCES_SYNCOPATION_ONE_IN_MAX) &&
           cJSON_AddNumberToObject(item, "skips_per_motif_max", COMPOSER_PREFERENCES_SKIPS_PER_MOTIF_MAX) &&
           cJSON_AddNumberToObject(item, "plan_chords_max", COMPOSER_PREFERENCES_PLAN_MAX) &&
           cJSON_AddNumberToObject(item, "plan_beat_max", COMPOSER_PREFERENCES_PLAN_BEAT_MAX) &&
           cJSON_AddNumberToObject(item, "leap_min", COMPOSER_PREFERENCES_LEAP_MIN) &&
           cJSON_AddNumberToObject(item, "leap_max", COMPOSER_PREFERENCES_LEAP_MAX) &&
           cJSON_AddNumberToObject(item, "range_low_min", COMPOSER_PREFERENCES_RANGE_LOW_MIN) &&
           cJSON_AddNumberToObject(item, "range_low_max", COMPOSER_PREFERENCES_RANGE_LOW_MAX) &&
           cJSON_AddNumberToObject(item, "range_high_max", COMPOSER_PREFERENCES_RANGE_HIGH_MAX) &&
           cJSON_AddNumberToObject(item, "range_span_min", COMPOSER_PREFERENCES_RANGE_SPAN_MIN) &&
           cJSON_AddNumberToObject(item, "density_max", COMPOSER_PREFERENCES_DENSITY_MAX) &&
           cJSON_AddNumberToObject(item, "scale_max", COMPOSER_PREFERENCES_SCALE_MAX) &&
           cJSON_AddNumberToObject(item, "climax_rise_min", COMPOSER_PREFERENCES_CLIMAX_RISE_MIN) &&
           cJSON_AddNumberToObject(item, "climax_rise_max", COMPOSER_PREFERENCES_CLIMAX_RISE_MAX) &&
           cJSON_AddNumberToObject(item, "ratio_denominator_max", COMPOSER_PREFERENCES_RATIO_DENOMINATOR_MAX) &&
           cJSON_AddNumberToObject(item, "fidelity_denominator_max", COMPOSER_PREFERENCES_FIDELITY_DENOMINATOR_MAX) &&
           cJSON_AddNumberToObject(item, "attempts_max", COMPOSER_PREFERENCES_ATTEMPTS_MAX) &&
           cJSON_AddNumberToObject(item, "keep_max", MELODY_KEEP_MAX) &&
           cJSON_AddNumberToObject(item, "rhythm_repeat_bars_max", COMPOSER_PREFERENCES_RHYTHM_REPEAT_BARS_MAX);
}

/* The default preferences, the document a listening session edits. */
static bool add_preferences(cJSON *root) {
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    return cJSON_AddItemToObject(root, "preferences", composer_preferences_write(&defaults));
}

/* The sizes the requests are limited to, for the help of a client that shows them. */
static bool add_limits(cJSON *root) {
    cJSON *limits = cJSON_AddObjectToObject(root, "limits");
    return limits && cJSON_AddNumberToObject(limits, "motive_notes", MOTIVE_MAX_NOTES) &&
           cJSON_AddNumberToObject(limits, "motive_chords", MOTIVE_MAX_CHORDS) &&
           cJSON_AddNumberToObject(limits, "motive_features", MOTIVE_MAX_FEATURES) &&
           cJSON_AddNumberToObject(limits, "variant_chain", VARIANT_CHAIN_MAX) &&
           cJSON_AddNumberToObject(limits, "variant_functions", VARIANT_FUNCTIONS) &&
           cJSON_AddNumberToObject(limits, "random_motive_count", MOTIVE_RANDOM_MAX_COUNT) &&
           cJSON_AddNumberToObject(limits, "tempo_min", (double)API_TEMPO_MIN) &&
           cJSON_AddNumberToObject(limits, "tempo_max", (double)API_TEMPO_MAX) &&
           cJSON_AddNumberToObject(limits, "piece_sections", PIECE_STRUCTURE_MAX_SECTIONS) &&
           cJSON_AddNumberToObject(limits, "piece_beats", (double)API_PIECE_BEATS_MAX) && add_seed_max(limits) &&
           add_preference_limits(limits);
}

bool api_vocabulary_build(cJSON *root, const CommonPracticeMaterials *materials) {
    return add_meters(root) && add_materials(root, materials, "materials", false) && add_materials(root, materials, "piece_materials", true) &&
           add_keys(root, materials) && add_cadences(root) && add_pickups(root) && add_library_lists(root) &&
           add_choices(root, "functions", API_FUNCTIONS) && add_choices(root, "distances", API_DISTANCES) &&
           add_theme_slots(root) && add_theme_lists(root) &&
           add_choices(root, "theme_pools", API_THEME_POOLS) && add_choices(root, "theme_bars", API_THEME_BARS) &&
           add_regular_bars(root) && add_theme_extensions(root) &&
           add_first_cadences(root) && add_theme_parts(root, "theme_beginnings", API_THEME_BEGINNINGS) &&
           add_theme_parts(root, "theme_endings", API_THEME_ENDINGS) && add_choices(root, "style_weights", API_STYLE_WEIGHTS) &&
           add_choices(root, "accompaniment_patterns", API_ACCOMPANIMENT_PATTERNS) &&
           add_piece_intos(root) && add_tone_labels(root) && add_operations(root) &&
           add_preferences(root) && add_limits(root);
}

/* The schemas a section's harmony may be (R-610): the galant prototypes that make a chord of each stage in the major and in the natural minor. */
static bool add_schemas(cJSON *root, const CommonPracticeMaterials *materials) {
    cJSON *list = add_list(root, "schemas");
    for (int id = 0; list && id < GALANT_SCHEMA_COUNT; id++) {
        if (!harmony_schema_available((GalantSchemaId)id, materials->major) && !harmony_schema_available((GalantSchemaId)id, materials->natural_minor)) {
            continue;
        }
        const char *name = galant_schema_name((GalantSchemaId)id);
        if (!add_choice(list, name, name)) {
            return false;
        }
    }
    return list != NULL;
}

/* The relations of the snapshot (K6), by id: a section's region may be one, taken from an anchor (R-610). */
static bool add_relations(cJSON *root, const KnowledgeSnapshot *knowledge) {
    static const char *const FAMILIES[] = {"tonal", "modal", "neo-riemannian", "set", "serial"};
    cJSON *list = add_list(root, "relations");
    for (size_t i = 0; list && i < knowledge_snapshot_entry_count(knowledge); i++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry_at(knowledge, i);
        const Relation *relation = entry && strcmp(entry->kind, "relation") == 0 ? knowledge_snapshot_relation(knowledge, entry->id) : NULL;
        if (!relation) {
            continue;
        }
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddStringToObject(item, "id", relation->id) || !cJSON_AddStringToObject(item, "name", relation->name) ||
            !cJSON_AddStringToObject(item, "family", FAMILIES[relation->family]) || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            return false;
        }
    }
    return list != NULL;
}

ComposerStatus composer_api_vocabulary(const ComposerContext *context, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    ComposerStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != COMPOSER_OK) {
        return context_status;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root || !api_vocabulary_build(root, &materials) || !add_relations(root, context->knowledge) || !add_schemas(root, &materials)) {
        cJSON_Delete(root);
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, root);
}
