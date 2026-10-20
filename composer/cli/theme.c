#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "grow.h"

#include <harmony_chord.h>

#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>

/* At most this many motives in a request, and arguments for grow. */
#define THEME_MOTIVES 16
#define THEME_ARGS 80
#define THEME_ROWS 16

#define SCHEMA_NAME "ursatz-theme-request"

typedef struct {
    const cJSON *json;
    const char *id;
} RequestMotive;

typedef struct {
    GrowExt base; /* first: run_grow_ext gets a pointer to it */
    cJSON *root;
    RequestMotive motives[THEME_MOTIVES];
    size_t motive_count;
    const cJSON *slot_motive[GROW_SLOT_COUNT]; /* the request motive of each slot, NULL for none */
    const char *slot_motive_id[GROW_SLOT_COUNT];
    const cJSON *idea_motive[GROW_IDEA_COUNT]; /* the request motive of each idea with its own, NULL for none */
    const char *idea_motive_id[GROW_IDEA_COUNT];
    const cJSON *accompaniment_motive; /* the "motive" member of an accompaniment of pattern "motive" */
} ThemeExt;

static const char *const SLOT_KEYS[GROW_SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};

/* --- the request --------------------------------------------------------------- */

char *request_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    if (fseek(file, 0, SEEK_END) == 0) {
        long length = ftell(file);
        if (length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
            text = malloc((size_t)length + 1);
            if (text && fread(text, 1, (size_t)length, file) == (size_t)length) {
                text[length] = '\0';
            } else {
                free(text);
                text = NULL;
            }
        }
    }
    fclose(file);
    return text;
}

/* A member that is a string (or null / absent when optional). Returns false, with the error printed, for any other type. */
bool request_member_string(const cJSON *object, const char *name, bool required, const char **out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        if (required) {
            cli_fail(CLI_EXIT_USAGE, "the request needs \"%s\"", name);
            return false;
        }
        return true;
    }
    if (!cJSON_IsString(item)) {
        cli_fail(CLI_EXIT_USAGE, "\"%s\" in the request must be a string", name);
        return false;
    }
    *out = item->valuestring;
    return true;
}

/* A pickup written as a string ("0", "1/2") or a number of beats; formats a number into text. */
bool request_member_pickup(const cJSON *object, char *text, size_t size, const char **out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "pickup");
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (cJSON_IsString(item)) {
        *out = item->valuestring;
        return true;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(text, size, "%g", item->valuedouble);
        *out = text;
        return true;
    }
    cli_fail(CLI_EXIT_USAGE, "\"pickup\" in the request must be a string or a number of beats");
    return false;
}

/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
bool request_parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

/*
 * Reads a motive of the request, a degree list in the key (R-135), with its pickup, harmony, features and role. `role` is
 * the role to force (NULL for the motive's own member). Returns 0 or the status of the error printed.
 */
int request_read_motive(const char *what, const cJSON *object, const char *role, const PlanMeter *meter,
                       const PlanKey *key, Motive *out) {
    const char *degrees;
    const char *harmony;
    const char *form;
    const char *role_text;
    const char *pickup;
    char pickup_text[32];
    if (!request_member_string(object, "degrees", true, &degrees) || !request_member_string(object, "harmony", false, &harmony) ||
        !request_member_string(object, "form", false, &form) || !request_member_string(object, "role", false, &role_text) ||
        !request_member_pickup(object, pickup_text, sizeof(pickup_text), &pickup)) {
        return CLI_EXIT_USAGE;
    }
    if (form && strcmp(form, "full") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "%s: form must be \"full\", not \"%s\"", what, form);
    }
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    options.has_key = true;
    options.key = *key;
    if (pickup) {
        if (!request_parse_pickup(pickup, meter->beat_unit, &options.pickup)) {
            return cli_fail(CLI_EXIT_USAGE, "%s: pickup '%s' is not a number of beats; write 0, 1 or 1/2", what, pickup);
        }
        options.has_pickup = true;
    }
    MotiveError error;
    if (harmony && harmony[0]) {
        if (motive_harmony_parse(harmony, meter, options.harmony, &options.harmony_count, &error) != MOTIVE_OK) {
            return cli_fail(CLI_EXIT_USAGE, "%s: harmony '%s' at %s: %s", what, harmony, error.location, error.message);
        }
        options.has_harmony = true;
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(object, "features");
    if (features && !cJSON_IsNull(features)) {
        if (!cJSON_IsArray(features) || cJSON_GetArraySize(features) > MOTIVE_MAX_FEATURES) {
            return cli_fail(CLI_EXIT_USAGE, "%s: \"features\" must be a list of at most %d features", what,
                            MOTIVE_MAX_FEATURES);
        }
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, features) {
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
            const cJSON *first = cJSON_GetObjectItemCaseSensitive(item, "first");
            const cJSON *last = cJSON_GetObjectItemCaseSensitive(item, "last");
            if (!cJSON_IsString(label) || strlen(label->valuestring) > MOTIVE_FEATURE_LABEL_MAX || !cJSON_IsNumber(first) ||
                !cJSON_IsNumber(last) || first->valuedouble < 0 || last->valuedouble < first->valuedouble) {
                return cli_fail(CLI_EXIT_USAGE, "%s: a feature is {\"label\", \"first\", \"last\"} with 0 <= first <= last", what);
            }
            MotiveFeature *feature = &options.features[options.feature_count++];
            snprintf(feature->label, sizeof(feature->label), "%s", label->valuestring);
            feature->first = (size_t)first->valuedouble;
            feature->last = (size_t)last->valuedouble;
        }
        options.has_features = options.feature_count > 0;
    }
    const char *wanted_role = role ? role : role_text;
    if (wanted_role) {
        if (!motive_role_parse(wanted_role, &options.role)) {
            return cli_fail(CLI_EXIT_USAGE, "%s: role must be melodic or accompaniment, not '%s'", what, wanted_role);
        }
        options.has_role = true;
    }
    if (motive_from_degrees_text(degrees, &options, out, &error) != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_USAGE, "%s: cannot read the degrees '%s'%s%s: %s", what, degrees,
                        error.location[0] ? " at " : "", error.location, error.message);
    }
    return 0;
}

/* The bits of the functions a form lists; false, with the error printed, for a name that is not one. */
bool request_read_functions(const char *what, const cJSON *functions, unsigned *out) {
    *out = 0;
    if (!functions || cJSON_IsNull(functions)) {
        return true;
    }
    if (!cJSON_IsArray(functions)) {
        cli_fail(CLI_EXIT_USAGE, "%s: \"functions\" must be a list", what);
        return false;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, functions) {
        const char *name = cJSON_IsString(item) ? item->valuestring : "";
        unsigned bit = strcmp(name, "beginning") == 0      ? GROW_FUNCTION_BEGINNING
                       : strcmp(name, "continuation") == 0 ? GROW_FUNCTION_CONTINUATION
                       : strcmp(name, "contrast") == 0     ? GROW_FUNCTION_CONTRAST
                       : strcmp(name, "cadential") == 0    ? GROW_FUNCTION_CADENTIAL
                                                           : 0;
        if (bit == 0) {
            cli_fail(CLI_EXIT_USAGE, "%s: function must be beginning, continuation, contrast or cadential, not '%s'", what, name);
            return false;
        }
        *out |= bit;
    }
    return true;
}

/* The slots: each names a motive of the request (a motive may be in several slots), "basic" required. */
static int read_slots(ThemeExt *ext, const cJSON *request) {
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
    if (!cJSON_IsObject(slots)) {
        return cli_fail(CLI_EXIT_USAGE, "the request needs \"slots\": {\"basic\": <motive id>, ...}");
    }
    for (const cJSON *entry = slots->child; entry; entry = entry->next) {
        bool known = false;
        for (int s = 0; s < GROW_SLOT_COUNT; s++) {
            known = known || strcmp(entry->string, SLOT_KEYS[s]) == 0;
        }
        if (!known) {
            return cli_fail(CLI_EXIT_USAGE, "unknown slot '%s'; the slots are basic, contrasting, continuation and cadential",
                            entry->string);
        }
    }
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        const cJSON *named = cJSON_GetObjectItemCaseSensitive(slots, SLOT_KEYS[s]);
        if (!named || cJSON_IsNull(named)) {
            if (s == GROW_SLOT_BASIC) {
                return cli_fail(CLI_EXIT_USAGE, "the request needs a basic slot");
            }
            continue;
        }
        if (!cJSON_IsString(named)) {
            return cli_fail(CLI_EXIT_USAGE, "slot '%s' must name a motive by its id", SLOT_KEYS[s]);
        }
        RequestMotive *found = NULL;
        for (size_t m = 0; m < ext->motive_count; m++) {
            if (strcmp(ext->motives[m].id, named->valuestring) == 0) {
                found = &ext->motives[m];
            }
        }
        if (!found) {
            return cli_fail(CLI_EXIT_USAGE, "slot '%s' names the motive '%s', which the request does not have", SLOT_KEYS[s],
                            named->valuestring);
        }
        ext->slot_motive[s] = found->json;
        ext->slot_motive_id[s] = found->id;
    }
    return 0;
}

/* The motive of the request with this id, else NULL. */
static const RequestMotive *find_motive(const ThemeExt *ext, const char *id) {
    for (size_t m = 0; m < ext->motive_count; m++) {
        if (strcmp(ext->motives[m].id, id) == 0) {
            return &ext->motives[m];
        }
    }
    return NULL;
}

/* The ideas: each names, by its idea name, a motive of the request (or null for none). */
static int read_ideas(ThemeExt *ext, const cJSON *request) {
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(request, "ideas");
    if (!ideas || cJSON_IsNull(ideas)) {
        return 0;
    }
    if (!cJSON_IsObject(ideas)) {
        return cli_fail(CLI_EXIT_USAGE, "\"ideas\" must be an object: {\"<idea name>\": <motive id>, ...}");
    }
    for (const cJSON *entry = ideas->child; entry; entry = entry->next) {
        int idea = -1;
        for (int k = 0; k < GROW_IDEA_COUNT; k++) {
            if (strcmp(entry->string, grow_idea_name((GrowIdea)k)) == 0) {
                idea = k;
            }
        }
        if (idea < 0) {
            return cli_fail(CLI_EXIT_USAGE, "unknown idea '%s' in \"ideas\"", entry->string);
        }
        if (cJSON_IsNull(entry)) {
            continue;
        }
        if (!cJSON_IsString(entry)) {
            return cli_fail(CLI_EXIT_USAGE, "idea '%s' must name a motive by its id", entry->string);
        }
        const RequestMotive *found = find_motive(ext, entry->valuestring);
        if (!found) {
            return cli_fail(CLI_EXIT_USAGE, "idea '%s' names the motive '%s', which the request does not have", entry->string,
                            entry->valuestring);
        }
        if (idea == GROW_IDEA_BASIC) {
            if (!ext->slot_motive[GROW_SLOT_BASIC] || ext->slot_motive[GROW_SLOT_BASIC] != found->json) {
                return cli_fail(CLI_EXIT_USAGE, "the basic idea is the basic slot; leave it out of \"ideas\" or name the "
                                                "motive of the basic slot");
            }
            continue;
        }
        ext->idea_motive[idea] = found->json;
        ext->idea_motive_id[idea] = found->id;
    }
    return 0;
}

/* Reads the motives, the slots and the ideas, and refuses saved forms, before anything is grown. */
static int read_motives(ThemeExt *ext, const cJSON *request) {
    const cJSON *motives = cJSON_GetObjectItemCaseSensitive(request, "motives");
    if (!cJSON_IsArray(motives) || cJSON_GetArraySize(motives) < 1 || cJSON_GetArraySize(motives) > THEME_MOTIVES) {
        return cli_fail(CLI_EXIT_USAGE, "the request needs \"motives\": 1 to %d motives", THEME_MOTIVES);
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, motives) {
        const char *id;
        if (!cJSON_IsObject(item) || !request_member_string(item, "id", true, &id)) {
            return CLI_EXIT_USAGE;
        }
        for (size_t m = 0; m < ext->motive_count; m++) {
            if (strcmp(ext->motives[m].id, id) == 0) {
                return cli_fail(CLI_EXIT_USAGE, "the motive id '%s' is used twice", id);
            }
        }
        ext->motives[ext->motive_count].json = item;
        ext->motives[ext->motive_count].id = id;
        ext->motive_count++;
    }
    int status = read_slots(ext, request);
    if (status == 0) {
        status = read_ideas(ext, request);
    }
    if (status != 0) {
        return status;
    }
    /* Saved forms are not read (R-207): the motives stage invents the forms. */
    for (size_t m = 0; m < ext->motive_count; m++) {
        const cJSON *forms = cJSON_GetObjectItemCaseSensitive(ext->motives[m].json, "forms");
        if (forms && !cJSON_IsNull(forms)) {
            return cli_fail(CLI_EXIT_USAGE, "motive '%s': saved forms are no longer read; leave \"forms\" out",
                            ext->motives[m].id);
        }
    }
    return 0;
}

/*
 * The chords: a list of {"bar", "text"}, at most one a bar, the text in the own-harmony syntax relative to the bar. They are
 * written into ext->base.chords as Given chords from the theme's start, each lasting to the next chord of its bar or the
 * end of the bar.
 */
static int read_chords(ThemeExt *ext, const PlanMeter *meter, const PlanKey *key) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(ext->root, "chords");
    if (!list || cJSON_IsNull(list)) {
        return 0;
    }
    if (!cJSON_IsArray(list)) {
        return cli_fail(CLI_EXIT_USAGE, "\"chords\" must be a list of {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
    }
    const cJSON *bars_item = cJSON_GetObjectItemCaseSensitive(ext->root, "bars");
    long bars = cJSON_IsNumber(bars_item) ? (long)bars_item->valuedouble : 8;
    PlanTime bar_length;
    if (plan_time_make(meter->beats_per_bar, (unsigned long)meter->beat_unit, &bar_length) != PLAN_OK) {
        return cli_fail(CLI_EXIT_USAGE, "the meter has no bar length");
    }
    bool seen[GROW_EXT_CHORDS / 2 + 1] = {false};
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        const cJSON *bar_item = cJSON_IsObject(entry) ? cJSON_GetObjectItemCaseSensitive(entry, "bar") : NULL;
        const char *text;
        if (!cJSON_IsObject(entry) || !cJSON_IsNumber(bar_item) || !request_member_string(entry, "text", true, &text)) {
            return cli_fail(CLI_EXIT_USAGE, "each of \"chords\" must be {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
        }
        long bar = (long)bar_item->valuedouble;
        if ((double)bar != bar_item->valuedouble || bar < 0 || bar >= bars) {
            return cli_fail(CLI_EXIT_USAGE, "chords bar %g: the theme has bars 0 to %ld", bar_item->valuedouble, bars - 1);
        }
        if (seen[bar]) {
            return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: given twice", bar);
        }
        seen[bar] = true;
        MotiveChord parsed[MOTIVE_MAX_CHORDS];
        size_t count = 0;
        MotiveError error;
        if (motive_harmony_parse(text, meter, parsed, &count, &error) != MOTIVE_OK) {
            return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: '%s' at %s: %s", bar, text, error.location, error.message);
        }
        if (count == 0 || count > 2) {
            return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: write 1 or 2 chords, not %zu", bar, count);
        }
        if (ext->base.chord_count + count > GROW_EXT_CHORDS) {
            return cli_fail(CLI_EXIT_USAGE, "chords: too many chords");
        }
        PlanTime bar_start;
        if (plan_time_make(bar_length.num * bar, bar_length.den, &bar_start) != PLAN_OK) {
            return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: the bar is out of range", bar);
        }
        for (size_t c = 0; c < count; c++) {
            const MotiveChord *chord = &parsed[c];
            PlanTime half_beats;
            PlanTime end = bar_length;
            /* An onset is a multiple of half a beat of the beat unit, inside the bar. */
            if (plan_time_make(chord->onset.num * 2 * meter->beat_unit, chord->onset.den, &half_beats) != PLAN_OK ||
                half_beats.den != 1) {
                return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: a chord starts on a beat or a half beat", bar);
            }
            PlanTime cell;
            PlanTime in_cells;
            if (!given_cell_length(meter, &cell) ||
                plan_time_make(chord->onset.num * (long)cell.den, chord->onset.den * (unsigned long)cell.num, &in_cells) !=
                    PLAN_OK ||
                in_cells.den != 1) {
                /* The harmony and bass operators write chords on the cells of the meter only (see docs/theme-request.md). */
                long beats = cell.num * (long)meter->beat_unit / (long)cell.den;
                return plan_time_compare(cell, bar_length) == 0
                           ? cli_fail(CLI_EXIT_USAGE, "chords bar %ld: a bar of %d/%d takes one chord", bar,
                                      meter->beats_per_bar, meter->beat_unit)
                           : cli_fail(CLI_EXIT_USAGE, "chords bar %ld: in %d/%d a chord starts only every %ld beats (@%ld)",
                                      bar, meter->beats_per_bar, meter->beat_unit, beats, beats);
            }
            if (plan_time_compare(chord->onset, bar_length) >= 0) {
                return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: a chord starts inside the bar", bar);
            }
            if (chord->alteration != 0) {
                return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: altered chords are not given; write a diatonic numeral", bar);
            }
            HarmonyChord harmony;
            memset(&harmony, 0, sizeof(harmony));
            harmony.degree = chord->degree;
            harmony.inversion = chord->inversion;
            harmony.seventh = chord->seventh;
            size_t at = ext->base.chord_count;
            if (!harmony_roman_format(key->mode, &harmony, ext->base.chord_numerals[at])) {
                return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: that chord does not exist in the key", bar);
            }
            if (c + 1 < count) {
                end = parsed[c + 1].onset;
            }
            GivenChord *given = &ext->base.chords[at];
            given->numeral = ext->base.chord_numerals[at];
            if (plan_time_add(bar_start, chord->onset, &given->onset) != PLAN_OK ||
                plan_time_make(end.num * (long)chord->onset.den - chord->onset.num * (long)end.den,
                               end.den * chord->onset.den, &given->length) != PLAN_OK) {
                return cli_fail(CLI_EXIT_USAGE, "chords bar %ld: the chord has no length", bar);
            }
            ext->base.chord_count++;
        }
    }
    return 0;
}

static int theme_load(GrowExt *base, const PlanMeter *meter, const PlanKey *key, const PlanThemePairing *pairing) {
    ThemeExt *ext = (ThemeExt *)base;
    char what[96];
    int status;
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        if (!ext->slot_motive[s]) {
            continue;
        }
        snprintf(what, sizeof(what), "motive '%s'", ext->slot_motive_id[s]);
        Motive *target = s == GROW_SLOT_BASIC ? &ext->base.basic : &ext->base.slot_motives[s];
        status = request_read_motive(what, ext->slot_motive[s], NULL, meter, key, target);
        if (status != 0) {
            return status;
        }
        if (target->role != MOTIVE_ROLE_MELODIC) {
            return cli_fail(CLI_EXIT_USAGE, "%s is of the accompaniment role; a slot takes a melodic motive", what);
        }
        ext->base.has_slot[s] = s != GROW_SLOT_BASIC;
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        if (!ext->idea_motive[idea]) {
            continue;
        }
        if (!grow_idea_in_pairing(pairing, (GrowIdea)idea)) {
            return cli_fail(CLI_EXIT_USAGE, "idea '%s' is not in this theme; the pairing has no %s", grow_idea_name((GrowIdea)idea),
                            grow_idea_name((GrowIdea)idea));
        }
        snprintf(what, sizeof(what), "motive '%s'", ext->idea_motive_id[idea]);
        status = request_read_motive(what, ext->idea_motive[idea], NULL, meter, key, &ext->base.idea_motives[idea]);
        if (status != 0) {
            return status;
        }
        if (ext->base.idea_motives[idea].role != MOTIVE_ROLE_MELODIC) {
            return cli_fail(CLI_EXIT_USAGE, "%s is of the accompaniment role; an idea takes a melodic motive", what);
        }
        ext->base.has_idea[idea] = true;
    }
    if (ext->accompaniment_motive) {
        status = request_read_motive("the accompaniment motive", ext->accompaniment_motive, "accompaniment", meter, key,
                             &ext->base.accompaniment_motive);
        if (status != 0) {
            return status;
        }
        ext->base.has_accompaniment_motive = true;
    }
    return read_chords(ext, meter, key);
}

/* --- the output ------------------------------------------------------------------ */

static void print_number(const ComposerRatio *ratio) {
    if (ratio->denominator == 1) {
        printf("%lu", ratio->numerator);
    } else {
        printf("%g", (double)ratio->numerator / (double)ratio->denominator);
    }
}

static void theme_print_slots(const GrowExt *base, const PiecePlan *plan, const GrowOutcome *outcome, bool json);

/* Whether the extension is the theme command's: the piece command prints its brief with this function too. */
static const ThemeExt *theme_ext_of(const GrowExt *base) {
    return base && base->print_theme == theme_print_slots ? (const ThemeExt *)base : NULL;
}

/* The chords the request gave, as read: [{"bar", "text"}]. */
static void print_chords_json(const ThemeExt *ext) {
    const cJSON *found = cJSON_GetObjectItemCaseSensitive(ext->root, "chords");
    const cJSON *list = cJSON_IsArray(found) ? found : NULL;
    const cJSON *entry = NULL;
    size_t at = 0;
    printf("\"chords\":[");
    cJSON_ArrayForEach(entry, list) {
        printf("%s{\"bar\":%ld,\"text\":", at++ ? "," : "", (long)cJSON_GetObjectItemCaseSensitive(entry, "bar")->valuedouble);
        cli_print_json_string(cJSON_GetObjectItemCaseSensitive(entry, "text")->valuestring);
        printf("}");
    }
    printf("]");
}

void theme_print_brief(const GrowExt *base, const GrowBrief *brief, bool json) {
    const ThemeExt *ext = theme_ext_of(base);
    if (json) {
        printf("\"brief\":{\"tempo\":{\"qpm\":");
        if (brief->tempo.denominator > 0) {
            print_number(&brief->tempo);
        } else {
            printf("null");
        }
        printf(",\"from\":\"%s\"},\"mode\":{\"mode\":\"%s\",\"from\":\"%s\"},\"density\":", brief->tempo_from,
               brief->minor ? "minor" : "major", brief->mode_from);
        if (brief->has_density) {
            print_number(&brief->density);
        } else {
            printf("null");
        }
        printf(",\"without\":[");
        for (size_t i = 0; i < brief->without_count; i++) {
            printf("%s", i ? "," : "");
            cli_print_json_string(brief->without[i]);
        }
        printf("]}");
        if (ext) {
            printf(",");
            print_chords_json(ext);
        }
        return;
    }
    printf("brief: mode %s (%s)", brief->minor ? "minor" : "major", brief->mode_from);
    if (brief->tempo.denominator > 0) {
        printf(", tempo ");
        print_number(&brief->tempo);
        printf(" (%s)", brief->tempo_from);
    }
    if (brief->has_density) {
        printf(", density ");
        print_number(&brief->density);
        printf(" attacks per beat");
    }
    if (brief->without_count > 0) {
        printf(", without");
        for (size_t i = 0; i < brief->without_count; i++) {
            printf("%s %s", i ? "," : "", brief->without[i]);
        }
    }
    printf("\n");
    const cJSON *found = ext ? cJSON_GetObjectItemCaseSensitive(ext->root, "chords") : NULL;
    const cJSON *list = cJSON_IsArray(found) ? found : NULL;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        printf("chords bar %ld: %s\n", (long)cJSON_GetObjectItemCaseSensitive(entry, "bar")->valuedouble,
               cJSON_GetObjectItemCaseSensitive(entry, "text")->valuestring);
    }
}

/* The slot an idea has the function of, by the idea's name (grow_form.c has the same table). */
static GrowSlot idea_slot(const char *name) {
    static const struct {
        const char *name;
        GrowSlot slot;
    } NAMES[] = {{"basic idea", GROW_SLOT_BASIC},
                 {"repetition", GROW_SLOT_BASIC},
                 {"consequent basic idea", GROW_SLOT_BASIC},
                 {"contrasting idea", GROW_SLOT_CONTRASTING},
                 {"consequent contrasting idea", GROW_SLOT_CONTRASTING},
                 {"fragment 1", GROW_SLOT_CONTINUATION},
                 {"fragment 2", GROW_SLOT_CONTINUATION},
                 {"cadential", GROW_SLOT_CADENTIAL},
                 {"cadential 1", GROW_SLOT_CADENTIAL},
                 {"cadential 2", GROW_SLOT_CADENTIAL}};
    for (size_t i = 0; name && i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (strcmp(name, NAMES[i].name) == 0) {
            return NAMES[i].slot;
        }
    }
    return GROW_SLOT_BASIC;
}

typedef struct {
    const char *idea;
    GrowSlot slot;
    GrowSlot origin; /* the slot whose motive the idea is made from */
    const char *source;
    int own_idea; /* the GrowIdea whose own motive the idea is, else -1 */
    char item[64];
    char chain[256];
    long connection; /* percent, -1 for none */
} Row;

/* The percent in a form's label ("... 87%: ..."), else -1; and where the text before it ends. */
static long label_percent(const char *label, size_t *before) {
    const char *mark = strstr(label, "%:");
    if (!mark) {
        return -1;
    }
    const char *start = mark;
    while (start > label && start[-1] >= '0' && start[-1] <= '9') {
        start--;
    }
    if (start == mark) {
        return -1;
    }
    *before = (size_t)(start - label);
    while (*before > 0 && label[*before - 1] == ' ') {
        (*before)--;
    }
    return strtol(start, NULL, 10);
}

/* The step of the motives stage that wrote the motive node, else NULL. */
static const FillConsidered *chosen_for(const GrowOutcome *outcome, NodeId node) {
    const FillRecord *record = outcome->records[GROW_STAGE_MOTIVES];
    for (size_t i = 0; record && i < record->step_count; i++) {
        const FillStep *step = &record->steps[i];
        if (step->target.kind == FILL_TARGET_NODE && step->target.id == node && step->chosen >= 0) {
            return &step->considered[step->chosen];
        }
    }
    return NULL;
}

static void describe_row(const GrowOutcome *outcome, NodeId motive_node, Row *row) {
    const FillConsidered *chosen = chosen_for(outcome, motive_node);
    row->source = "motive";
    row->connection = -1;
    if (!chosen) {
        return;
    }
    size_t before = 0;
    row->connection = label_percent(chosen->label, &before);
    if (strcmp(chosen->operator_name, GROW_FORM_OPERATOR_NAME) != 0) {
        row->source = "operator";
        snprintf(row->chain, sizeof(row->chain), "%s", chosen->label);
    } else if (strncmp(chosen->label, "exact ", 6) == 0) {
        row->source = "motive";
    } else {
        row->source = "registry";
        snprintf(row->chain, sizeof(row->chain), "%.*s", (int)(before < sizeof(row->chain) - 1 ? before : sizeof(row->chain) - 1),
                 chosen->label);
    }
}

/* The ideas of the theme, in order, with the motive each is made from and how its motive was made. */
static size_t collect_rows(const PiecePlan *plan, const GrowOutcome *outcome, Row *rows) {
    NodeId theme = cli_find_node(plan, "theme");
    size_t count = 0;
    if (theme == PLAN_NODE_NONE) {
        return 0;
    }
    PlanSpan span = plan_node_span(plan, theme);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < THEME_ROWS; id++) {
        if (plan_node_kind(plan, id) != PLAN_NODE_IDEA || !plan_span_contains(span, plan_node_span(plan, id))) {
            continue;
        }
        LaneSpanId lanes[16];
        size_t found = plan_node_lane_spans(plan, id, lanes, 16);
        for (size_t i = 0; i < found && i < 16; i++) {
            if (plan_lane_span_lane(plan, lanes[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, lanes[i]) != 0) {
                continue;
            }
            for (size_t k = 0; k < plan_lane_span_constraint_count(plan, lanes[i]); k++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, lanes[i], k);
                if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF) {
                    continue;
                }
                Row *row = &rows[count];
                memset(row, 0, sizeof(*row));
                row->idea = plan_node_name(plan, id);
                row->own_idea = -1;
                row->slot = idea_slot(row->idea);
                row->origin = GROW_SLOT_BASIC;
                /* The idea's melody asks for its motive node (m1 to m4); that node is made from m0 or a slot's node. */
                NodeId made_from = constraint->params.motive.motive;
                for (size_t c = 0; c < plan_node_constraint_count(plan, made_from); c++) {
                    const PlanConstraint *ref = plan_node_constraint_at(plan, made_from, c);
                    if (ref->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
                        made_from = ref->params.motive.motive;
                    }
                }
                const char *node_name = plan_node_name(plan, made_from);
                /* The slot motives are the material nodes s1 (contrasting), s2 (continuation) and s3 (cadential). */
                for (GrowSlot own = GROW_SLOT_CONTRASTING; node_name && own < GROW_SLOT_COUNT; own++) {
                    if (node_name[0] == 's' && node_name[1] == (char)('0' + own) && node_name[2] == '\0') {
                        row->origin = own;
                    }
                }
                const char *direct = plan_node_name(plan, constraint->params.motive.motive);
                if (direct && direct[0] == 'i' && direct[1] >= '0' && direct[1] <= '9' && direct[2] == '\0') {
                    /* The idea has a motive of its own: the node "i<k>" of the material. */
                    row->own_idea = direct[1] - '0';
                    row->source = "idea";
                    row->connection = -1;
                } else if (direct && strcmp(direct, "m0") == 0) {
                    row->source = "motive"; /* the basic idea is the basic motive */
                    row->connection = -1;
                } else {
                    describe_row(outcome, constraint->params.motive.motive, row);
                }
                count++;
                break;
            }
            break;
        }
    }
    return count;
}

static void theme_print_slots(const GrowExt *base, const PiecePlan *plan, const GrowOutcome *outcome, bool json) {
    const ThemeExt *ext = (const ThemeExt *)base;
    Row rows[THEME_ROWS];
    size_t count = collect_rows(plan, outcome, rows);
    if (json) {
        printf("\"slots\":[");
        for (size_t i = 0; i < count; i++) {
            const char *motive = rows[i].own_idea >= 0 ? ext->idea_motive_id[rows[i].own_idea]
                                                       : ext->slot_motive_id[rows[i].origin];
            printf("%s{\"idea\":", i ? "," : "");
            cli_print_json_string(rows[i].idea ? rows[i].idea : "");
            printf(",\"slot\":\"%s\",\"motive\":", grow_slot_name(rows[i].slot));
            cli_print_json_string(motive ? motive : "");
            printf(",\"source\":\"%s\",\"item\":", rows[i].source);
            if (rows[i].item[0]) {
                cli_print_json_string(rows[i].item);
            } else {
                printf("null");
            }
            printf(",\"chain\":");
            if (rows[i].chain[0]) {
                cli_print_json_string(rows[i].chain);
            } else {
                printf("null");
            }
            printf(",\"connection\":");
            if (rows[i].connection >= 0) {
                printf("%ld", rows[i].connection);
            } else {
                printf("null");
            }
            printf("}");
        }
        printf("]");
        return;
    }
    printf("\nslots (the motive each idea is made from)\n");
    for (size_t i = 0; i < count; i++) {
        const char *motive = rows[i].own_idea >= 0 ? ext->idea_motive_id[rows[i].own_idea]
                                                   : ext->slot_motive_id[rows[i].origin];
        printf("  %s: %s slot, motive %s, %s", rows[i].idea ? rows[i].idea : "?", grow_slot_name(rows[i].slot),
               motive ? motive : "?", rows[i].source);
        if (rows[i].item[0]) {
            printf(" %s", rows[i].item);
        }
        if (rows[i].chain[0]) {
            printf(" (%s)", rows[i].chain);
        }
        if (rows[i].connection >= 0) {
            printf(", connection %ld%%", rows[i].connection);
        }
        printf("\n");
    }
}

/* --- the command ------------------------------------------------------------------ */

typedef struct {
    bool help;
    bool check;
    bool plain_midi;
    const char *input;
    const char *seed;
    const char *suggest;
    const char *out;
    const char *midi;
    const char *musicxml;
    const char *explain;
    const char *format;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", THEME_USAGE_LINE + 7);
    fprintf(out, "Grows a theme from the motives of a request (JSON, schema %s, version 2): up to 16 motives, which the\n", SCHEMA_NAME);
    fprintf(out, "slots of the theme (basic, contrasting, continuation and cadential) and its single ideas name (a motive may be\n");
    fprintf(out, "in several), and the brief: key, mode, meter, cadences, an accompaniment or a voicing, and a style that gives\n");
    fprintf(out, "the tempo, the mode and the note density. The forms of the ideas are invented. See\n");
    fprintf(out, "docs/theme-request.md.\n\n");
    fprintf(out, "Options (as grow's):\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output (default: from the clock)\n");
    fprintf(out, "  --suggest <n>        write n different themes (1 to 16); with files, theme k goes to <name>-k.<extension>\n");
    fprintf(out, "  --check              analyze the theme with the Ursatz Analyzer and pass over a seed whose analysis contradicts\n");
    fprintf(out, "                       the plan\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the melody, the bass and the accompaniment as a MIDI file (the tempo of the brief)\n");
    fprintf(out, "  --musicxml <file>    write them as MusicXML\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected\n");
    fprintf(out, "  --format <text|json> how to print the theme on stdout (default: text), with the slots and the brief\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid request; 2 the request cannot be satisfied; 3 an output file\n");
    fprintf(out, "could not be written.\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed;
        } else if (strcmp(arg, "--suggest") == 0) {
            target = &options->suggest;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer theme --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one request file", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    return 0;
}

typedef struct {
    const char *items[THEME_ARGS];
    size_t count;
} Args;

static bool args_add(Args *args, const char *text) {
    if (args->count >= THEME_ARGS) {
        return false;
    }
    args->items[args->count++] = text;
    return true;
}

static bool args_pair(Args *args, const char *name, const char *value) {
    return args_add(args, name) && args_add(args, value);
}

/* grow's arguments for the request: the brief, and the options passed through. */
static int build_args(ThemeExt *ext, const cJSON *request, const Options *options, Args *args) {
    const char *into;
    const char *hybrid;
    const char *key;
    const char *mode;
    const char *meter;
    const char *cadence;
    const char *first_cadence;
    const char *voicing;
    const char *style;
    const char *tempo;
    const char *weight;
    if (!request_member_string(request, "into", true, &into) || !request_member_string(request, "hybrid", false, &hybrid) ||
        !request_member_string(request, "key", true, &key) || !request_member_string(request, "mode", true, &mode) ||
        !request_member_string(request, "meter", true, &meter) || !request_member_string(request, "cadence", false, &cadence) ||
        !request_member_string(request, "first_cadence", false, &first_cadence) ||
        !request_member_string(request, "voicing", false, &voicing) || !request_member_string(request, "style", false, &style) ||
        !request_member_string(request, "style_weight", false, &weight)) {
        return CLI_EXIT_USAGE;
    }
    const cJSON *tempo_item = cJSON_GetObjectItemCaseSensitive(request, "tempo");
    static char tempo_text[32];
    tempo = NULL;
    if (tempo_item && !cJSON_IsNull(tempo_item)) {
        if (cJSON_IsString(tempo_item)) {
            tempo = tempo_item->valuestring;
        } else if (cJSON_IsNumber(tempo_item)) {
            snprintf(tempo_text, sizeof(tempo_text), "%g", tempo_item->valuedouble);
            tempo = tempo_text;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "\"tempo\" in the request must be a number of quarter notes per minute");
        }
    }
    bool ok = args_add(args, "ursatz-composer") && args_add(args, "grow") && args_pair(args, "--into", into) &&
              args_pair(args, "--key", key) && args_pair(args, "--mode", mode) && args_pair(args, "--meter", meter);
    if (hybrid) {
        ok = ok && args_pair(args, "--hybrid", hybrid);
    }
    static char bars_text[16];
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(request, "bars");
    if (bars && !cJSON_IsNull(bars)) {
        if (!cJSON_IsNumber(bars)) {
            return cli_fail(CLI_EXIT_USAGE, "\"bars\" in the request must be 4 or 8");
        }
        snprintf(bars_text, sizeof(bars_text), "%g", bars->valuedouble);
        ok = ok && args_pair(args, "--bars", bars_text);
    }
    if (cadence) {
        ok = ok && args_pair(args, "--cadence", cadence);
    }
    if (first_cadence) {
        ok = ok && args_pair(args, "--first-cadence", first_cadence);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern;
        if (!cJSON_IsObject(accompaniment) || !request_member_string(accompaniment, "pattern", true, &pattern)) {
            return CLI_EXIT_USAGE;
        }
        if (strcmp(pattern, "motive") == 0) {
            ext->accompaniment_motive = cJSON_GetObjectItemCaseSensitive(accompaniment, "motive");
            if (!cJSON_IsObject(ext->accompaniment_motive)) {
                return cli_fail(CLI_EXIT_USAGE, "an accompaniment of pattern \"motive\" needs a \"motive\" member");
            }
            ext->base.accompaniment_motive_given = true;
        }
        ok = ok && args_pair(args, "--accompaniment", pattern);
    }
    if (voicing) {
        if (strcmp(voicing, "satb") == 0) {
            ok = ok && args_add(args, "--voicing");
        } else if (strcmp(voicing, "chorale") == 0) {
            ok = ok && args_pair(args, "--texture", "chorale");
        } else {
            return cli_fail(CLI_EXIT_USAGE, "\"voicing\" in the request must be satb or chorale, not '%s'", voicing);
        }
    }
    if (style) {
        ok = ok && args_pair(args, "--style", style);
        if (weight) {
            ok = ok && args_pair(args, "--style-weight", weight);
        }
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        if (!style) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request must be a list of categories");
            }
            ok = ok && args_pair(args, "--style-without", category->valuestring);
        }
    }
    if (tempo) {
        ok = ok && args_pair(args, "--tempo", tempo);
    }
    if (options->seed) {
        ok = ok && args_pair(args, "--seed", options->seed);
    }
    if (options->suggest) {
        ok = ok && args_pair(args, "--suggest", options->suggest);
    }
    if (options->check) {
        ok = ok && args_add(args, "--check");
    }
    if (options->plain_midi) {
        ok = ok && args_add(args, "--plain-midi");
    }
    if (options->out) {
        ok = ok && args_pair(args, "--out", options->out);
    }
    if (options->midi) {
        ok = ok && args_pair(args, "--midi", options->midi);
    }
    if (options->musicxml) {
        ok = ok && args_pair(args, "--musicxml", options->musicxml);
    }
    if (options->explain) {
        ok = ok && args_pair(args, "--explain", options->explain);
    }
    if (options->format) {
        ok = ok && args_pair(args, "--format", options->format);
    }
    return ok ? 0 : cli_fail(CLI_EXIT_USAGE, "the request has too many style categories to leave out");
}

int run_theme(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return cli_fail(CLI_EXIT_USAGE, "theme needs a request file\n%s", THEME_USAGE_LINE);
    }
    char *text = request_read_file(options.input);
    if (!text) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the request '%s'", options.input);
    }
    ThemeExt ext;
    memset(&ext, 0, sizeof(ext));
    ext.root = cJSON_Parse(text);
    free(text);
    if (!ext.root || !cJSON_IsObject(ext.root)) {
        cJSON_Delete(ext.root);
        return cli_fail(CLI_EXIT_USAGE, "the request '%s' is not a JSON object", options.input);
    }
    const char *schema;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(ext.root, "schema_version");
    if (!request_member_string(ext.root, "schema", true, &schema) || strcmp(schema, SCHEMA_NAME) != 0) {
        status = cJSON_GetObjectItemCaseSensitive(ext.root, "schema")
                     ? cli_fail(CLI_EXIT_USAGE, "the request must have \"schema\": \"%s\"", SCHEMA_NAME)
                     : CLI_EXIT_USAGE;
        cJSON_Delete(ext.root);
        return status;
    }
    if (!cJSON_IsNumber(version) || version->valuedouble != 2) {
        cJSON_Delete(ext.root);
        return cli_fail(CLI_EXIT_USAGE, "the request must have \"schema_version\": 2 (see docs/theme-request.md)");
    }
    Args args;
    memset(&args, 0, sizeof(args));
    ext.base.load = theme_load;
    ext.base.print_theme = theme_print_slots;
    ext.base.print_brief = theme_print_brief;
    status = read_motives(&ext, ext.root);
    if (status == 0) {
        status = build_args(&ext, ext.root, &options, &args);
    }
    if (status == 0) {
        status = run_grow_ext((int)args.count, (char **)args.items, &ext.base);
    }
    cJSON_Delete(ext.root);
    return status;
}
