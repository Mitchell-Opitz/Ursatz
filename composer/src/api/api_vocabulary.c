#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/style_labels.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_tones.h>
#include <ursatz_composer/piece_structure.h>
#include <ursatz_composer/variant_registry.h>

#include "api_internal.h"
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
const ApiChoice API_THEME_SLOTS[] = {{"basic", "Basic idea"},
                                     {"contrasting", "Contrasting idea"},
                                     {"continuation", "Continuation"},
                                     {"cadential", "Cadential"},
                                     {NULL, NULL}};
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

const ApiThemeIdea API_THEME_IDEAS[] = {{"basic idea", 0},
                                        {"repetition", 0},
                                        {"contrasting idea", 1},
                                        {"consequent basic idea", 0},
                                        {"consequent contrasting idea", 1},
                                        {"fragment 1", 2},
                                        {"fragment 2", 2},
                                        {"cadential", 3},
                                        {"cadential 1", 3},
                                        {"cadential 2", 3},
                                        {NULL, 0}};

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

/* --- labels of the lists the library holds ----------------------------------------------------------------- */

/* In the order of plan_cadence_names (PAC IAC HC DC PC). */
static const char *const CADENCE_LABELS[PLAN_CADENCE_COUNT] = {"Perfect authentic cadence", "Imperfect authentic cadence",
                                                               "Half cadence", "Deceptive cadence", "Plagal cadence"};
static const char *const MOTIVE_FORM_LABELS[] = {"Full", "Rhythm only", "Contour only"};
static const char *const ROLE_LABELS[] = {"Melodic", "Accompaniment"};
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
    cJSON *roles = add_list(root, "roles");
    cJSON *parts = add_list(root, "piece_parts");
    cJSON *fills = add_list(root, "fill_kinds");
    if (!forms || !roles || !parts || !fills) {
        return false;
    }
    for (int form = MOTIVE_SKETCH_FORM_FULL; form <= MOTIVE_SKETCH_FORM_CONTOUR; form++) {
        if (!add_choice(forms, motive_form_name((MotiveSketchForm)form), MOTIVE_FORM_LABELS[form - MOTIVE_SKETCH_FORM_FULL])) {
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

static bool add_theme_lists(cJSON *root) {
    cJSON *ideas = add_list(root, "theme_ideas");
    cJSON *categories = add_list(root, "theme_motive_categories");
    for (size_t i = 0; ideas && API_THEME_IDEAS[i].name; i++) {
        cJSON *entry = add_choice(ideas, API_THEME_IDEAS[i].name, API_THEME_IDEAS[i].name);
        if (!entry || !cJSON_AddStringToObject(entry, "slot", API_THEME_SLOTS[API_THEME_IDEAS[i].slot].value)) {
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

bool api_vocabulary_build(cJSON *root) {
    return add_meters(root) && add_choices(root, "modes", API_MODES) && add_choices(root, "piece_modes", API_PIECE_MODES) &&
           add_keys(root) && add_cadences(root) && add_pickups(root) && add_library_lists(root) &&
           add_choices(root, "functions", API_FUNCTIONS) && add_choices(root, "distances", API_DISTANCES) &&
           add_choices(root, "theme_slots", API_THEME_SLOTS) && add_theme_lists(root) &&
           add_choices(root, "theme_pools", API_THEME_POOLS) &&
           add_choices(root, "accompaniment_patterns", API_ACCOMPANIMENT_PATTERNS) &&
           add_choices(root, "piece_intos", API_PIECE_INTOS) && add_tone_labels(root) && add_operations(root);
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
