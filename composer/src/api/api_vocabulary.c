#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/style_labels.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
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

const ApiChoice API_MODES[] = {{"major", "Major"}, {"minor", "Minor"}, {NULL, NULL}};
const ApiChoice API_PIECE_MODES[] = {{"major", "Major"}, {"minor", "Minor"}, {"style", "From the brief"}, {NULL, NULL}};
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
/* The pickups an application offers, in beats of the meter: a pickup is also accepted when it is any other multiple of 1/32
   of a whole note shorter than a bar. */
const ApiChoice API_PICKUPS[] = {{"0", "0"},   {"1/4", "1/4"}, {"1/2", "1/2"}, {"1", "1"},
                                 {"3/2", "3/2"}, {"2", "2"},   {"3", "3"},     {NULL, NULL}};

const ApiChoice API_THEME_BARS[] = {{"4", "4 bars"}, {"8", "8 bars"}, {NULL, NULL}};
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

int api_choice_find_ignore_case(const ApiChoice *table, const char *text) {
    for (size_t i = 0; text && table[i].value; i++) {
        if (request_equals_ignore_case(text, table[i].value)) {
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
static const char *const FILL_LABELS[] = {"Empty", "Motif", "Theme", "Progression"};

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

static bool add_keys(cJSON *root) {
    cJSON *keys = add_list(root, "keys");
    cJSON *names = add_list(root, "pitch_class_names");
    for (int minor = 0; keys && minor < 2; minor++) {
        for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
            char name[32];
            util_key_name(pitch_class, minor != 0, name, sizeof name);
            if (!add_choice(keys, name, name)) {
                return false;
            }
        }
    }
    for (int pitch_class = 0; names && pitch_class < 12; pitch_class++) {
        char major[32];
        char minor[32];
        util_key_name(pitch_class, false, major, sizeof major);
        util_key_name(pitch_class, true, minor, sizeof minor);
        *strchr(major, ' ') = '\0';
        *strchr(minor, ' ') = '\0';
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !cJSON_AddNumberToObject(entry, "value", pitch_class) || !cJSON_AddStringToObject(entry, "major", major) ||
            !cJSON_AddStringToObject(entry, "minor", minor) || !cJSON_AddItemToArray(names, entry)) {
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

/* The pickups of each meter: the offered values the readers accept (a number of beats shorter than a bar, on the 1/32 grid). */
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
            Rational grid;
            if (!request_parse_pickup(API_PICKUPS[i].value, meter.beat_unit, &pickup) ||
                rational_cmp(pickup, (Rational){meter.beats_per_bar, meter.beat_unit}) >= 0 ||
                rational_make(pickup.num * PIECE_PICKUP_GRID, pickup.den, &grid) != RATIONAL_OK || grid.den != 1) {
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

static bool add_library_lists(cJSON *root) {
    cJSON *forms = add_list(root, "motive_forms");
    cJSON *strategies = add_list(root, "random_motive_strategies");
    cJSON *roles = add_list(root, "roles");
    cJSON *parts = add_list(root, "piece_parts");
    cJSON *fills = add_list(root, "fill_kinds");
    if (!forms || !strategies || !roles || !parts || !fills) {
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
    for (int fill = PIECE_FILL_EMPTY; fill <= PIECE_FILL_PROGRESSION; fill++) {
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
        PlanThemePairing pairing = {0};
        for (int b = 0; b < 3; b++) {
            for (int e = 0; e < 3; e++) {
                PlanThemePairing candidate = {(PlanThemeBeginning)b, (PlanThemeEnding)e};
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
           cJSON_AddNumberToObject(item, "keep_max", MELODY_KEEP_MAX);
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

bool api_vocabulary_build(cJSON *root) {
    return add_meters(root) && add_choices(root, "modes", API_MODES) && add_choices(root, "piece_modes", API_PIECE_MODES) &&
           add_keys(root) && add_cadences(root) && add_pickups(root) && add_library_lists(root) &&
           add_choices(root, "functions", API_FUNCTIONS) && add_choices(root, "distances", API_DISTANCES) &&
           add_theme_slots(root) && add_theme_lists(root) &&
           add_choices(root, "theme_pools", API_THEME_POOLS) && add_choices(root, "theme_bars", API_THEME_BARS) &&
           add_first_cadences(root) && add_theme_parts(root, "theme_beginnings", API_THEME_BEGINNINGS) &&
           add_theme_parts(root, "theme_endings", API_THEME_ENDINGS) && add_choices(root, "style_weights", API_STYLE_WEIGHTS) &&
           add_choices(root, "accompaniment_patterns", API_ACCOMPANIMENT_PATTERNS) &&
           add_piece_intos(root) && add_tone_labels(root) && add_operations(root) &&
           add_preferences(root) && add_limits(root);
}

ComposerStatus composer_api_vocabulary(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_CreateObject();
    if (!root || !api_vocabulary_build(root)) {
        cJSON_Delete(root);
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, root);
}
