#include "piece.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cJSON.h>

#include "cli_options.h"
#include "fill.h"
#include "grow.h"
#include "theme.h"

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_structure.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/variations.h>
#include "util/util.h"

#define SCHEMA_NAME "ursatz-piece-request"
#define NUMERALS_MAX 32
#define POOL_MAX 64
#define OUTPUT_MAX 8
/* The ending version is composed from the loop's seed plus this (R-172). */
#define ENDING_SEED_OFFSET 20UL

/* ------------------------------------------------------------- the request */

typedef struct {
    cJSON *root;
    PieceStructure structure;
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 1];
    const cJSON *motive_json[PIECE_STRUCTURE_MAX_SECTIONS + 1]; /* a motif fill's own motive */
    const cJSON *forms_json[PIECE_STRUCTURE_MAX_SECTIONS + 1];  /* a motif fill's saved forms */
    Motive *section_motive[PIECE_STRUCTURE_MAX_SECTIONS + 1];   /* owned */
    PiecePlan *theme_plan[PIECE_STRUCTURE_MAX_SECTIONS + 1];    /* owned */
    char *numeral_text[PIECE_STRUCTURE_MAX_SECTIONS + 1];       /* owned; the numerals point into it */
    const char *numerals[PIECE_STRUCTURE_MAX_SECTIONS + 1][NUMERALS_MAX];
    char source[PIECE_STRUCTURE_MAX_SECTIONS + 1][PLAN_STYLE_PATH_MAX]; /* where a section's material came from */
    const cJSON *piece_motive_json;
    Motive *piece_motive; /* owned */
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *style_path;
    const char *tempo_text;
    char tempo_buffer[32];
    ComposerRatio weight;
    CliStyleWithout without;
    ComposerStyle *style; /* owned */
    ComposerRatio tempo;  /* a denominator of 0 for none */
    const char *tempo_from;
    const char *mode_from;
    GrowPoolForm *pool;   /* owned */
    Motive *pool_motives; /* owned */
    size_t pool_count;
    const Motive *basic;  /* the motive the motives stage compares with, NULL when it is invented */
    PlanExportOptions export_options;
} Request;

static void request_destroy(Request *request) {
    for (size_t i = 0; i <= PIECE_STRUCTURE_MAX_SECTIONS; i++) {
        free(request->section_motive[i]);
        free(request->numeral_text[i]);
        plan_destroy(request->theme_plan[i]);
    }
    free(request->piece_motive);
    free(request->pool);
    free(request->pool_motives);
    composer_style_destroy(request->style);
    cJSON_Delete(request->root);
}

/* "antecedent+continuation": a beginning, a plus sign and an ending (as grow --hybrid). */
static bool parse_hybrid(const char *text, PlanThemePairing *pairing) {
    static const char *const BEGINNINGS[] = {"presentation", "antecedent", "cbi"};
    static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};
    const char *plus = strchr(text, '+');
    if (!plus) {
        return false;
    }
    size_t length = (size_t)(plus - text);
    bool beginning_found = false;
    bool ending_found = false;
    for (int i = 0; i < 3; i++) {
        if (strlen(BEGINNINGS[i]) == length && strncmp(text, BEGINNINGS[i], length) == 0) {
            pairing->beginning = (PlanThemeBeginning)i;
            beginning_found = true;
        }
        if (cli_equals_ignore_case(plus + 1, ENDINGS[i])) {
            pairing->ending = (PlanThemeEnding)i;
            ending_found = true;
        }
    }
    return beginning_found && ending_found;
}

/* A fill's numerals, "I vi IV V", split into the section's own copy. */
static int read_numerals(Request *request, size_t index, const char *id, const char *text) {
    request->numeral_text[index] = strdup(text);
    if (!request->numeral_text[index]) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    size_t count = 0;
    char *save = NULL;
    for (char *word = strtok_r(request->numeral_text[index], " \t,", &save); word; word = strtok_r(NULL, " \t,", &save)) {
        if (count == NUMERALS_MAX) {
            return cli_fail(CLI_EXIT_USAGE, "section '%s': a progression has at most %d chords", id, NUMERALS_MAX);
        }
        request->numerals[index][count++] = word;
    }
    if (count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "section '%s': the progression has no numerals", id);
    }
    request->sections[index].numerals = request->numerals[index];
    request->sections[index].numeral_count = count;
    return 0;
}

static int read_fill(Request *request, size_t index, const cJSON *fill, const PlanMeter *meter) {
    PieceSection *section = &request->sections[index];
    const char *id = section->id;
    const char *kind = "empty";
    if (fill && !cJSON_IsNull(fill)) {
        if (!cJSON_IsObject(fill) || !request_member_string(fill, "kind", true, &kind)) {
            return CLI_EXIT_USAGE;
        }
    } else {
        fill = NULL;
    }
    if (!piece_fill_from_name(kind, &section->fill)) {
        return cli_fail(CLI_EXIT_USAGE, "section '%s': the fill kind must be empty, motif, theme or progression, not '%s'", id,
                        kind);
    }
    snprintf(request->source[index], sizeof(request->source[index]), "basic motive");
    if (section->fill == PIECE_FILL_MOTIF) {
        const char *into = NULL;
        const char *hybrid = NULL;
        if (!request_member_string(fill, "into", false, &into) || !request_member_string(fill, "hybrid", false, &hybrid)) {
            return CLI_EXIT_USAGE;
        }
        request->motive_json[index] = cJSON_GetObjectItemCaseSensitive(fill, "motive");
        request->forms_json[index] = cJSON_GetObjectItemCaseSensitive(fill, "forms");
        if (cJSON_IsNull(request->motive_json[index])) {
            request->motive_json[index] = NULL;
        }
        if (request->motive_json[index]) {
            if (!cJSON_IsObject(request->motive_json[index])) {
                return cli_fail(CLI_EXIT_USAGE, "section '%s': \"motive\" must be a motive", id);
            }
            snprintf(request->source[index], sizeof(request->source[index]), "own motive");
        }
        if (request->forms_json[index] && !cJSON_IsNull(request->forms_json[index]) &&
            !cJSON_IsArray(request->forms_json[index])) {
            return cli_fail(CLI_EXIT_USAGE, "section '%s': \"forms\" must be a list", id);
        }
        if (into) {
            section->has_into = true;
            if (cli_equals_ignore_case(into, "sentence")) {
                section->into = PLAN_ARCHETYPE_SENTENCE;
            } else if (cli_equals_ignore_case(into, "period")) {
                section->into = PLAN_ARCHETYPE_PERIOD;
            } else if (cli_equals_ignore_case(into, "hybrid")) {
                section->into = PLAN_ARCHETYPE_HYBRID;
            } else {
                return cli_fail(CLI_EXIT_USAGE, "section '%s': \"into\" must be sentence, period or hybrid, not '%s'", id, into);
            }
        }
        if (section->has_into && section->into == PLAN_ARCHETYPE_HYBRID) {
            if (!hybrid || !parse_hybrid(hybrid, &section->hybrid)) {
                return cli_fail(CLI_EXIT_USAGE, "section '%s': a hybrid needs \"hybrid\": \"<beginning>+<ending>\", for example "
                                                "antecedent+continuation", id);
            }
            bool sentence = section->hybrid.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
                            section->hybrid.ending == PLAN_THEME_ENDING_CONTINUATION;
            bool period = section->hybrid.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
                          section->hybrid.ending == PLAN_THEME_ENDING_CONSEQUENT;
            if (sentence || period) {
                return cli_fail(CLI_EXIT_USAGE, "section '%s': %s is a %s, not a hybrid", id, hybrid,
                                sentence ? "sentence" : "period");
            }
        } else if (hybrid) {
            return cli_fail(CLI_EXIT_USAGE, "section '%s': \"hybrid\" applies only to \"into\": \"hybrid\"", id);
        }
    } else if (section->fill == PIECE_FILL_THEME) {
        const char *path = NULL;
        PlanJsonError error;
        if (!request_member_string(fill, "plan", true, &path)) {
            return CLI_EXIT_USAGE;
        }
        if (plan_read_json_file(path, &request->theme_plan[index], &error) != PLAN_OK) {
            return cli_fail(CLI_EXIT_USAGE, "section '%s': cannot read the theme plan '%s': %s", id, path, error.message);
        }
        section->theme_plan = request->theme_plan[index];
        snprintf(request->source[index], sizeof(request->source[index]), "%s", path);
    } else if (section->fill == PIECE_FILL_PROGRESSION) {
        const char *numerals = NULL;
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        if (!request_member_string(fill, "numerals", true, &numerals)) {
            return CLI_EXIT_USAGE;
        }
        int status = read_numerals(request, index, id, numerals);
        if (status != 0) {
            return status;
        }
        if (cells && !cJSON_IsNull(cells)) {
            if (!cJSON_IsNumber(cells) || cells->valuedouble < 1 || cells->valuedouble != (double)(size_t)cells->valuedouble) {
                return cli_fail(CLI_EXIT_USAGE, "section '%s': \"cells\" must be a whole number from 1", id);
            }
            section->cells_per_chord = (size_t)cells->valuedouble;
        }
        snprintf(request->source[index], sizeof(request->source[index]), "%s", numerals);
    }
    (void)meter;
    return 0;
}

static int read_section(Request *request, size_t index, const cJSON *item, const PlanMeter *meter) {
    PieceSection *section = &request->sections[index];
    const char *id = NULL;
    const char *part = NULL;
    const char *letter = NULL;
    const char *cadence = NULL;
    const char *pickup = NULL;
    char pickup_text[32];
    if (!cJSON_IsObject(item)) {
        return cli_fail(CLI_EXIT_USAGE, "every section of the request must be an object");
    }
    if (!request_member_string(item, "id", true, &id) || !request_member_string(item, "part", true, &part) ||
        !request_member_string(item, "letter", false, &letter) || !request_member_string(item, "cadence", false, &cadence) ||
        !request_member_pickup(item, pickup_text, sizeof(pickup_text), &pickup)) {
        return CLI_EXIT_USAGE;
    }
    if (strlen(id) == 0 || strlen(id) >= PIECE_SECTION_ID_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "a section id is 1 to %d characters, not '%s'", PIECE_SECTION_ID_MAX - 1, id);
    }
    snprintf(section->id, sizeof(section->id), "%s", id);
    if (!piece_part_from_name(part, &section->part)) {
        return cli_fail(CLI_EXIT_USAGE, "section '%s': the part must be intro, body, turnaround or ending, not '%s'", id, part);
    }
    if (letter) {
        snprintf(section->letter, sizeof(section->letter), "%s", letter);
    } else {
        snprintf(section->letter, sizeof(section->letter), "%c", toupper((unsigned char)id[0]));
    }
    const cJSON *beats = cJSON_GetObjectItemCaseSensitive(item, "beats");
    if (!cJSON_IsNumber(beats) || beats->valuedouble < 1 || beats->valuedouble > 4096 ||
        beats->valuedouble != (double)(size_t)beats->valuedouble) {
        return cli_fail(CLI_EXIT_USAGE, "section '%s': \"beats\" must be a whole number of beats, 1 or more", id);
    }
    section->beats = (size_t)beats->valuedouble;
    section->pickup = (Rational){0, 1};
    if (pickup && !request_parse_pickup(pickup, meter->beat_unit, &section->pickup)) {
        return cli_fail(CLI_EXIT_USAGE, "section '%s': pickup '%s' is not a number of beats; write 0, 1 or 1/2", id, pickup);
    }
    if (cadence) {
        if (!cli_parse_cadence(cadence, &section->cadence)) {
            return cli_fail(CLI_EXIT_USAGE, "section '%s': the cadence must be PAC, IAC, HC, DC or PC, not '%s'", id, cadence);
        }
        section->has_cadence = true;
    }
    return read_fill(request, index, cJSON_GetObjectItemCaseSensitive(item, "fill"), meter);
}

/* The brief: key, mode, meter, the style and its weight, the tempo, the accompaniment or voicing. */
static int read_brief(Request *request) {
    const cJSON *root = request->root;
    const char *style_weight = NULL;
    const char *voicing = NULL;
    if (!request_member_string(root, "key", true, &request->key_text) ||
        !request_member_string(root, "mode", true, &request->mode_text) ||
        !request_member_string(root, "meter", true, &request->meter_text) ||
        !request_member_string(root, "style", false, &request->style_path) ||
        !request_member_string(root, "style_weight", false, &style_weight) ||
        !request_member_string(root, "voicing", false, &voicing)) {
        return CLI_EXIT_USAGE;
    }
    PieceStructure *structure = &request->structure;
    const char *mode = request->mode_text;
    if (!cli_equals_ignore_case(mode, "major") && !cli_equals_ignore_case(mode, "minor") &&
        !cli_equals_ignore_case(mode, "style")) {
        return cli_fail(CLI_EXIT_USAGE, "\"mode\" must be major, minor or style, not '%s'", mode);
    }
    if (cli_equals_ignore_case(mode, "style") && !request->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "\"mode\": \"style\" needs a \"style\"");
    }
    /* The key may be a tonic alone: the mode is the one "mode" gives. */
    char tonic_key[64];
    const char *key_text = request->key_text;
    if (strlen(key_text) + 7 < sizeof(tonic_key) && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof(tonic_key), "%s major", key_text);
        key_text = tonic_key;
    }
    if (!cli_parse_key(key_text, &structure->key)) {
        return cli_fail(CLI_EXIT_USAGE, "unknown key '%s'; write a tonic letter A-G with an optional # or b, for example \"D\"",
                        request->key_text);
    }
    if (cli_equals_ignore_case(mode, "major")) {
        structure->key.mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (cli_equals_ignore_case(mode, "minor")) {
        structure->key.mode = KEY_STRUCTURE_MODE_MINOR;
    }
    if (!cli_parse_meter(request->meter_text, &structure->meter) || !harmony_meter_supported(&structure->meter)) {
        return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8", request->meter_text);
    }
    request->weight = (ComposerRatio){1, 1};
    if (style_weight && !cli_parse_ratio(style_weight, &request->weight)) {
        return cli_fail(CLI_EXIT_USAGE, "\"style_weight\" must be a fraction like 1/2 or a whole number, not '%s'", style_weight);
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(root, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        if (!request->style_path) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            int added = cJSON_IsString(category) ? cli_style_without_add(&request->without, category->valuestring)
                                                 : cli_fail(CLI_EXIT_USAGE, "\"without\" must be a list of categories");
            if (added != 0) {
                return added;
            }
        }
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(root, "tempo");
    if (tempo && !cJSON_IsNull(tempo)) {
        if (cJSON_IsString(tempo)) {
            request->tempo_text = tempo->valuestring;
        } else if (cJSON_IsNumber(tempo)) {
            snprintf(request->tempo_buffer, sizeof(request->tempo_buffer), "%g", tempo->valuedouble);
            request->tempo_text = request->tempo_buffer;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "\"tempo\" in the request must be a number of quarter notes per minute");
        }
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(root, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern = NULL;
        if (!cJSON_IsObject(accompaniment) || !request_member_string(accompaniment, "pattern", true, &pattern)) {
            return CLI_EXIT_USAGE;
        }
        if (strcmp(pattern, "motive") == 0) {
            return cli_fail(CLI_EXIT_USAGE, "an accompaniment of pattern \"motive\" is not available in a piece");
        }
        int status = cli_read_accompaniment(pattern, NULL, request->meter_text, &structure->meter, &structure->accompaniment);
        if (status != 0) {
            return status;
        }
        structure->has_accompaniment = true;
    }
    if (voicing) {
        if (structure->has_accompaniment) {
            return cli_fail(CLI_EXIT_USAGE, "\"voicing\" writes the inner voices itself; it cannot be combined with an "
                                            "\"accompaniment\"");
        }
        if (strcmp(voicing, "satb") != 0 && strcmp(voicing, "chorale") != 0) {
            return cli_fail(CLI_EXIT_USAGE, "\"voicing\" in the request must be satb or chorale, not '%s'", voicing);
        }
        structure->has_voicing = true;
        structure->voicing.write_soprano = strcmp(voicing, "chorale") == 0;
    }
    return 0;
}

/* The style, then the tempo and the mode it may give, as grow does. */
static int load_style(Request *request, bool plain_midi) {
    if (request->style_path) {
        int status = cli_load_style(request->style_path, request->without.names, request->without.count, &request->style);
        if (status != 0) {
            return status;
        }
    }
    int status = cli_read_export_options(request->tempo_text, plain_midi, &request->export_options);
    if (status != 0) {
        return status;
    }
    request->tempo = (ComposerRatio){0, 0};
    request->tempo_from = "none";
    if (request->tempo_text) {
        request->tempo.numerator = request->export_options.tempo_numerator;
        request->tempo.denominator = request->export_options.tempo_denominator;
        request->tempo_from = "option";
    } else if (composer_style_tempo(request->style, &request->tempo)) {
        request->export_options.tempo_numerator = request->tempo.numerator;
        request->export_options.tempo_denominator = request->tempo.denominator;
        request->tempo_from = "style";
    }
    request->mode_from = "request";
    if (cli_equals_ignore_case(request->mode_text, "style")) {
        bool minor = false;
        if (!composer_style_mode(request->style, &minor)) {
            return cli_fail(CLI_EXIT_USAGE, "\"mode\": \"style\" needs a style with a mode category");
        }
        request->structure.key.mode = minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
        request->mode_from = "style";
    }
    if (request->style) {
        PlanStyle *plan_style = &request->structure.style;
        memset(plan_style, 0, sizeof(*plan_style));
        snprintf(plan_style->path, sizeof(plan_style->path), "%s", request->style_path);
        plan_style->weight_numerator = request->weight.numerator;
        plan_style->weight_denominator = request->weight.denominator;
        for (size_t i = 0; i < request->without.count && i < PLAN_STYLE_WITHOUT_MAX; i++) {
            snprintf(plan_style->without[plan_style->without_count++], PLAN_STYLE_WITHOUT_NAME_MAX, "%s",
                     request->without.names[i]);
        }
        request->structure.has_style = true;
    }
    return 0;
}

/* A melodic motive of the request. */
static int read_melodic(const char *what, const cJSON *json, const PlanMeter *meter, const AnalysisKey *key, Motive **out) {
    *out = calloc(1, sizeof(Motive));
    if (!*out) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    int status = request_read_motive(what, json, NULL, meter, key, *out);
    if (status == 0 && (*out)->accompaniment) {
        return cli_fail(CLI_EXIT_USAGE, "%s is of the accompaniment role; a piece takes a melodic motive", what);
    }
    return status;
}

/* The motives, and the saved forms the motives stage may use; the key and the meter are known now. */
static int load_motives(Request *request) {
    PieceStructure *structure = &request->structure;
    char what[96];
    int status = 0;
    if (request->piece_motive_json) {
        status = read_melodic("the motive of the piece", request->piece_motive_json, &structure->meter, &structure->key,
                              &request->piece_motive);
        structure->motive = request->piece_motive;
    }
    size_t forms = 0;
    for (size_t i = 0; status == 0 && i < structure->section_count; i++) {
        if (request->motive_json[i]) {
            snprintf(what, sizeof(what), "the motive of section '%s'", request->sections[i].id);
            status = read_melodic(what, request->motive_json[i], &structure->meter, &structure->key, &request->section_motive[i]);
            request->sections[i].motive = request->section_motive[i];
        }
        const cJSON *list = request->forms_json[i];
        const cJSON *form = NULL;
        cJSON_ArrayForEach(form, list) {
            forms++;
        }
    }
    if (status != 0 || forms == 0) {
        return status;
    }
    if (forms > POOL_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "the request has more than %d saved forms", POOL_MAX);
    }
    request->pool = calloc(forms, sizeof(*request->pool));
    request->pool_motives = calloc(forms, sizeof(*request->pool_motives));
    if (!request->pool || !request->pool_motives) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    for (size_t i = 0; i < structure->section_count; i++) {
        const cJSON *form = NULL;
        cJSON_ArrayForEach(form, request->forms_json[i]) {
            const char *item_id = NULL;
            const char *flag = NULL;
            if (!cJSON_IsObject(form) || !request_member_string(form, "item", true, &item_id) ||
                !request_member_string(form, "flag", true, &flag)) {
                return CLI_EXIT_USAGE;
            }
            if (strcmp(flag, "rejected") == 0) {
                return cli_fail(CLI_EXIT_USAGE, "the form '%s' is rejected; leave it out of the request", item_id);
            }
            if (strcmp(flag, "approved") != 0 && strcmp(flag, "unflagged") != 0) {
                return cli_fail(CLI_EXIT_USAGE, "the flag of form '%s' must be approved or unflagged, not '%s'", item_id, flag);
            }
            snprintf(what, sizeof(what), "form '%s'", item_id);
            status = request_read_motive(what, form, NULL, &structure->meter, &structure->key,
                                         &request->pool_motives[request->pool_count]);
            if (status != 0) {
                return status;
            }
            GrowPoolForm *entry = &request->pool[request->pool_count];
            const cJSON *chain = cJSON_GetObjectItemCaseSensitive(form, "chain");
            entry->motive = &request->pool_motives[request->pool_count];
            entry->slot_motive_of = GROW_SLOT_BASIC;
            entry->approved = strcmp(flag, "approved") == 0;
            snprintf(entry->item, sizeof(entry->item), "%s", item_id);
            snprintf(entry->chain, sizeof(entry->chain), "%s", cJSON_IsString(chain) ? chain->valuestring : "");
            if (!request_read_functions(what, cJSON_GetObjectItemCaseSensitive(form, "functions"), &entry->functions)) {
                return CLI_EXIT_USAGE;
            }
            request->pool_count++;
        }
    }
    return 0;
}

/* The request, whole: the structure of sections as PieceStructure, with every motive read. */
static int read_request(Request *request, const char *path, bool plain_midi) {
    char *text;
    if (!util_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the request '%s'", path);
    }
    request->root = cJSON_Parse(text);
    free(text);
    if (!request->root || !cJSON_IsObject(request->root)) {
        return cli_fail(CLI_EXIT_USAGE, "the request '%s' is not a JSON object", path);
    }
    const char *schema = NULL;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request->root, "schema_version");
    if (!request_member_string(request->root, "schema", true, &schema) || strcmp(schema, SCHEMA_NAME) != 0 ||
        !cJSON_IsNumber(version) || version->valuedouble != 1) {
        return cli_fail(CLI_EXIT_USAGE, "the request must have \"schema\": \"%s\" and \"schema_version\": 1", SCHEMA_NAME);
    }
    int status = read_brief(request);
    if (status != 0) {
        return status;
    }
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(request->root, "sections");
    if (!cJSON_IsArray(sections) || cJSON_GetArraySize(sections) < 1) {
        return cli_fail(CLI_EXIT_USAGE, "the request needs \"sections\": a list with a body");
    }
    /* One more than the most, room for the default ending. */
    if (cJSON_GetArraySize(sections) > PIECE_STRUCTURE_MAX_SECTIONS) {
        return cli_fail(CLI_EXIT_USAGE, "a piece has at most %d sections", PIECE_STRUCTURE_MAX_SECTIONS);
    }
    PieceStructure *structure = &request->structure;
    structure->sections = request->sections;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, sections) {
        status = read_section(request, structure->section_count, item, &structure->meter);
        if (status != 0) {
            return status;
        }
        structure->section_count++;
    }
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(request->root, "motive");
    if (motive && !cJSON_IsNull(motive)) {
        if (!cJSON_IsObject(motive)) {
            return cli_fail(CLI_EXIT_USAGE, "\"motive\" in the request must be a motive");
        }
        request->piece_motive_json = motive;
    }
    status = load_style(request, plain_midi);
    if (status == 0) {
        status = load_motives(request);
    }
    if (status != 0) {
        return status;
    }
    request->basic = structure->motive;
    for (size_t i = 0; !request->basic && i < structure->section_count; i++) {
        request->basic = request->sections[i].motive;
    }
    /* The structure checks before the default ending joins it: a request without a body is a bad one. */
    if (piece_structure_check(structure) != PIECE_OK) {
        return cli_fail(CLI_EXIT_USAGE, "the sections are not a piece: they need a body, in the order intro, body, turnaround, "
                                        "ending, with different ids, whole beats and a pickup shorter than a bar");
    }
    size_t given = structure->section_count;
    if (piece_structure_default_ending(structure, PIECE_STRUCTURE_MAX_SECTIONS + 1) && structure->section_count > given) {
        /* The default ending is made from the basic motive, like an empty section. */
        snprintf(request->source[given], sizeof(request->source[0]), "basic motive");
    }
    return 0;
}

/* ------------------------------------------------------------- composing */

typedef struct {
    PiecePlan *plan;
    ComposeOutcome outcome;
    bool composed;
} Version;

typedef struct {
    Version loop;
    Version ending;
    unsigned long seed;
} Piece;

static void version_destroy(Version *version) {
    if (version->composed) {
        compose_outcome_destroy(&version->outcome);
    }
    plan_destroy(version->plan);
    memset(version, 0, sizeof(*version));
}

static void piece_destroy(Piece *piece) {
    version_destroy(&piece->loop);
    version_destroy(&piece->ending);
}


typedef struct {
    bool help;
    bool check;
    bool plain_midi;
    const char *input;
    const char *from;       /* the loop plan to work on (regenerate) */
    const char *request;    /* the request that plan was made from */
    const char *regenerate; /* a section id, or "none" */
    const char *lane;       /* harmony, bass, melody, inner or all */
    const char *variations; /* a section id: make variations of it instead of regenerating */
    const char *count;
    const char *out_dir;
    const char *seed;
    const char *out;
    const char *ending_out;
    const char *midi;
    const char *musicxml;
    const char *ending_midi;
    const char *ending_musicxml;
    const char *explain;
    const char *format;
} Options;

static void make_config(const Request *request, bool check, unsigned long seed, ComposeConfig *config) {
    memset(config, 0, sizeof(*config));
    config->seed = seed;
    config->analyzer_check = check;
    config->style = request->style;
    config->style_weight = request->weight;
    config->basic = request->basic;
    config->tempo = request->tempo;
    config->pool = request->pool;
    config->pool_count = request->pool_count;
}

static PieceResult compose_version(const Request *request, bool check, unsigned long seed, Version *version) {
    ComposeConfig config;
    make_config(request, check, seed, &config);
    PieceResult result = compose_run(version->plan, &config, &version->outcome);
    version->composed = true;
    return result;
}

/* One try from a seed: the loop version, then the ending version built from it with seed + 20. */
static PieceResult compose_piece(const Request *request, bool check, unsigned long seed, Piece *piece) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    PieceResult result = piece_structure_build_plan(&request->structure, &piece->loop.plan);
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed, &piece->loop);
    }
    if (result == PIECE_OK) {
        result = piece_ending_plan(piece->loop.plan, &request->structure, &piece->ending.plan);
    }
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed + ENDING_SEED_OFFSET, &piece->ending);
    }
    return result;
}

/* ------------------------------------------------------------- regenerating a section */

typedef struct {
    const char *text;    /* the loop plan, normalized (cli_load_plan_text) */
    const char *section; /* the request's section id, or NULL for none: only the ending version is rebuilt */
    const char *lane;    /* harmony, bass, melody or inner; NULL for the whole section */
} Edit;

/* A section's fill in the request, or PIECE_FILL_EMPTY for an id the request does not have. */
static PieceFillKind request_fill(const Request *request, const char *id) {
    for (size_t s = 0; s < request->structure.section_count; s++) {
        if (strcmp(request->sections[s].id, id) == 0) {
            return request->sections[s].fill;
        }
    }
    return PIECE_FILL_EMPTY;
}

static bool is_node_target(const FillTarget *targets, size_t count, NodeId node) {
    for (size_t i = 0; i < count; i++) {
        if (targets[i].kind == FILL_TARGET_NODE && targets[i].id == node) {
            return true;
        }
    }
    return false;
}

/*
 * The loop plan read again, with what regenerating the section reopens: its lane (a lane the section was given, by a
 * theme or a progression, is reopened for the lane asked and no other), and, when the section holds the body's first
 * bar, the seam's copy of that bar dropped and the sections the loop's end holds back reopened, so the seam stage
 * copies the new bar and writes the end against it. Returns 0 or the status of the error printed.
 */
static int open_edit(const Request *request, const Edit *edit, PiecePlan **out, FillTarget **targets, size_t *count,
                     char *what, size_t what_size) {
    PlanJsonError error;
    PiecePlan *plan = NULL;
    *out = NULL;
    *targets = NULL;
    *count = 0;
    if (plan_from_json(edit->text, &plan, &error) != PLAN_OK) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the loop plan: %s", error.message);
    }
    NodeId node = piece_section_find(plan, edit->section);
    if (node == PLAN_NODE_NONE) {
        plan_destroy(plan);
        return cli_fail(CLI_EXIT_USAGE, "the loop plan has no section '%s'", edit->section);
    }
    if (request_fill(request, edit->section) == PIECE_FILL_THEME && !edit->lane) {
        plan_destroy(plan);
        return cli_fail(CLI_EXIT_USAGE,
                        "section '%s' is filled with a theme: regenerate it with --lane harmony, bass, melody or inner", edit->section);
    }
    CliTargetOptions target;
    char node_text[24];
    memset(&target, 0, sizeof(target));
    snprintf(node_text, sizeof(node_text), "%zu", (size_t)node);
    target.node_text = node_text;
    target.lane_text = edit->lane;
    int status = cli_check_target(&target, "piece", PIECE_USAGE_LINE);
    if (status != 0) {
        plan_destroy(plan);
        return status;
    }
    if (target.has_lane && request_fill(request, edit->section) == PIECE_FILL_THEME &&
        (target.lane == PLAN_LANE_MELODY || target.lane == PLAN_LANE_INNER_VOICE)) {
        plan_destroy(plan);
        return cli_fail(CLI_EXIT_USAGE,
                        "section '%s' is filled with a theme: its melody and inner voices are the theme's own; regenerate its harmony "
                        "or bass, or fill the section another way",
                        edit->section);
    }
    if (target.has_lane) {
        /* New chords need a bass and inner voices written from them (the melody stays), so these are reopened first. */
        if (target.lane == PLAN_LANE_HARMONY) {
            piece_reopen_lane(plan, node, PLAN_LANE_BASS);
            piece_reopen_lane(plan, node, PLAN_LANE_INNER_VOICE);
        }
        piece_reopen_lane(plan, node, target.lane);
    }
    FillTarget *found = NULL;
    size_t found_count = 0;
    status = cli_find_targets(plan, &target, &found, &found_count);
    if (status != 0) {
        plan_destroy(plan);
        return status;
    }
    snprintf(what, what_size, "%s%s section %s", target.has_lane ? "the " : "", target.has_lane ? edit->lane : "", edit->section);
    NodeId held[PIECE_STRUCTURE_MAX_SECTIONS];
    size_t held_count = 0;
    bool refresh = piece_section_holds_body_start(plan, node);
    if (refresh) {
        held_count = piece_held_sections(plan, held, PIECE_STRUCTURE_MAX_SECTIONS);
        held_count = held_count > PIECE_STRUCTURE_MAX_SECTIONS ? PIECE_STRUCTURE_MAX_SECTIONS : held_count;
    }
    FillTarget *all = calloc(found_count + held_count + 1, sizeof(FillTarget));
    if (!all || (refresh && piece_seam_reset(plan) != PIECE_OK)) {
        free(all);
        free(found);
        plan_destroy(plan);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot reopen the seam");
    }
    memcpy(all, found, found_count * sizeof(FillTarget));
    size_t total = found_count;
    for (size_t h = 0; h < held_count; h++) {
        if (!is_node_target(all, total, held[h])) {
            all[total++] = (FillTarget){FILL_TARGET_NODE, held[h]};
        }
    }
    free(found);
    *out = plan;
    *targets = all;
    *count = total;
    return 0;
}

/*
 * The section written again in the loop plan, with seed. With no section the loop plan is kept as it is. A usage error or a
 * refused target is a status in *status; PIECE_ERR_STUCK is the result of a stuck run, which the next seed may mend.
 */
static PieceResult regenerate_loop(const Request *request, const Edit *edit, bool check, unsigned long seed, Version *loop,
                                   int *status) {
    memset(loop, 0, sizeof(*loop));
    *status = 0;
    if (!edit->section) {
        /* The plan is complete: nothing is composed, so the loop version has no outcome to report. */
        PlanJsonError error;
        if (plan_from_json(edit->text, &loop->plan, &error) != PLAN_OK) {
            *status = cli_fail(CLI_EXIT_USAGE, "cannot read the loop plan: %s", error.message);
            return PIECE_ERR_BAD_REQUEST;
        }
        return PIECE_OK;
    }
    PiecePlan *plan = NULL;
    FillTarget *targets = NULL;
    size_t count = 0;
    char what[96];
    ComposeConfig config;
    make_config(request, check, seed, &config);
    *status = open_edit(request, edit, &plan, &targets, &count, what, sizeof(what));
    if (*status != 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    RegenerateResult ran = plan_regenerate_all(plan, targets, count, &config, &loop->outcome);
    free(targets);
    loop->plan = plan;
    loop->composed = true;
    if (ran == REGENERATE_ERR_STUCK) {
        return PIECE_ERR_STUCK;
    }
    if (ran != REGENERATE_OK) {
        *status = cli_report_refused(ran, what);
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

/* One try from a seed: the loop plan written again, then the ending version built from it with seed + 20. */
static PieceResult regenerate_piece(const Request *request, const Edit *edit, bool check, unsigned long seed, Piece *piece,
                                    int *status) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    PieceResult result = regenerate_loop(request, edit, check, seed, &piece->loop, status);
    if (result == PIECE_OK) {
        result = piece_ending_plan(piece->loop.plan, &request->structure, &piece->ending.plan);
    }
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed + ENDING_SEED_OFFSET, &piece->ending);
    }
    return result;
}

/* ------------------------------------------------------------- reading the plans */

/* The loop's region: from the start of the first body section to the end of the last section of the loop. */
static bool loop_region(const PiecePlan *plan, Rational *start, Rational *end) {
    bool found = false;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            if (constraint->kind != PLAN_CONSTRAINT_PIECE_PART) {
                continue;
            }
            int part = constraint->params.piece_part.part;
            PlanSpan span = plan_node_span(plan, id);
            Rational span_end;
            if ((part != (int)PIECE_PART_BODY && part != (int)PIECE_PART_TURNAROUND) ||
                plan_span_end(span, &span_end) != PLAN_OK) {
                continue;
            }
            if (!found || (part == (int)PIECE_PART_BODY && rational_cmp(span.start, *start) < 0)) {
                *start = span.start;
            }
            if (!found || rational_cmp(span_end, *end) > 0) {
                *end = span_end;
            }
            found = true;
        }
    }
    return found;
}

#define ROWS_MAX (PIECE_STRUCTURE_MAX_SECTIONS + 1)

typedef struct {
    const PieceSection *section;
    const char *source;
    Rational start;
    bool has_cadence;
    CadenceType cadence;
    const char *heard; /* the cadence the Analyzer heard, NULL without --check */
    bool kept;         /* a regenerate left the section as it was: it was neither heard nor checked */
} Row;

/* The sections of a version in time order, with the cadence each planned and, with a check, the one heard. */
static size_t collect_rows(const Request *request, const Version *version, Row *rows) {
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    size_t next_item = 0;
    size_t count = 0;
    const PiecePlan *plan = version->plan;
    for (NodeId id = 1; id <= plan_node_count(plan) && count < ROWS_MAX; id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION ||
            plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
            continue;
        }
        const PlanConstraint *part = NULL;
        const PlanConstraint *cadence = NULL;
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            part = constraint->kind == PLAN_CONSTRAINT_PIECE_PART ? constraint : part;
            cadence = constraint->kind == PLAN_CONSTRAINT_CADENCE && !cadence ? constraint : cadence;
        }
        if (!part) {
            continue;
        }
        Row *row = &rows[count];
        memset(row, 0, sizeof(*row));
        for (size_t s = 0; s < request->structure.section_count; s++) {
            if (strcmp(request->sections[s].id, part->params.piece_part.id) == 0) {
                row->section = &request->sections[s];
                row->source = request->source[s];
            }
        }
        if (!row->section) {
            continue;
        }
        row->start = plan_node_span(plan, id).start;
        for (size_t h = 0; h < version->outcome.section_count; h++) {
            if (version->outcome.sections[h].section == id && !version->outcome.sections[h].in_scope) {
                row->kept = true;
            }
        }
        row->has_cadence = cadence != NULL;
        row->cadence = cadence ? cadence->params.cadence : CADENCE_TYPE_PERFECT_AUTHENTIC;
        if (cadence && check) {
            size_t seen = 0;
            for (size_t i = 0; i < check->item_count; i++) {
                if (check->items[i].kind == ANALYZER_CHECK_CADENCE && seen++ == next_item) {
                    row->heard = check->items[i].heard;
                }
            }
            next_item++;
        }
        count++;
    }
    return count;
}

/* ------------------------------------------------------------- the output */

static void print_time(Rational time) {
    if (time.den == 1) {
        printf("%lld", time.num);
    } else {
        printf("%lld/%lld", time.num, time.den);
    }
}

static void print_time_json(Rational time) {
    printf("\"%lld/%lld\"", time.num, time.den);
}

static GrowBrief make_brief(const Request *request) {
    GrowBrief brief;
    memset(&brief, 0, sizeof(brief));
    brief.tempo = request->tempo;
    brief.tempo_from = request->tempo_from;
    brief.minor = request->structure.key.mode == KEY_STRUCTURE_MODE_MINOR;
    brief.mode_from = request->mode_from;
    brief.has_density = composer_style_density(request->style, &brief.density);
    brief.without = request->without.names;
    brief.without_count = request->without.count;
    return brief;
}

static void print_version_text(const char *label, const Request *request, const Version *version) {
    Row rows[ROWS_MAX];
    size_t count = collect_rows(request, version, rows);
    const ComposeOutcome *outcome = &version->outcome;
    printf("\n%s version\n", label);
    for (size_t i = 0; i < count; i++) {
        printf("  %-10s %-2s %-3s start ", rows[i].section->id, piece_part_name(rows[i].section->part),
               rows[i].section->letter);
        print_time(rows[i].start);
        printf(", %zu beats", rows[i].section->beats);
        if (rows[i].has_cadence) {
            printf(", cadence %s", cli_cadence_name(rows[i].cadence));
        }
        if (rows[i].kept) {
            printf(" (kept)");
        } else if (rows[i].heard) {
            printf(" (heard %s)", rows[i].heard);
        }
        printf(", %s fill (%s)\n", piece_fill_name(rows[i].section->fill), rows[i].source);
    }
    printf("  score %lu/%lu\n", outcome->score.numerator, outcome->score.denominator);
    if (outcome->seam_planned) {
        printf("  seam: %zu of 4 rules held (progression %s, no parallels %s, step %s, leading tone %s)\n", outcome->seam.held,
               outcome->seam.progression ? "yes" : "no", outcome->seam.no_parallels ? "yes" : "no",
               outcome->seam.step ? "yes" : "no", outcome->seam.leading_tone ? "yes" : "no");
    }
    if (outcome->analyzer_check_count > 0) {
        const AnalyzerCheck *check = &outcome->analyzer_checks[0];
        printf("  analyzer check: %zu of %zu agreed\n", check->agreed, check->planned);
    }
    if (outcome->style_measured) {
        cli_print_style_distance(&outcome->style_distance);
    }
}

static void print_version_json(const char *label, const Request *request, const Version *version, const char *plan_path) {
    Row rows[ROWS_MAX];
    size_t count = collect_rows(request, version, rows);
    const ComposeOutcome *outcome = &version->outcome;
    printf("\"%s\":{\"plan\":", label);
    if (plan_path) {
        cli_print_json_string(plan_path);
    } else {
        printf("null");
    }
    printf(",\"sections\":[");
    for (size_t i = 0; i < count; i++) {
        printf("%s{\"id\":", i ? "," : "");
        cli_print_json_string(rows[i].section->id);
        printf(",\"part\":\"%s\",\"letter\":", piece_part_name(rows[i].section->part));
        cli_print_json_string(rows[i].section->letter);
        printf(",\"start\":");
        print_time_json(rows[i].start);
        printf(",\"beats\":%zu,\"cadence\":", rows[i].section->beats);
        if (rows[i].has_cadence) {
            printf("\"%s\"", cli_cadence_name(rows[i].cadence));
        } else {
            printf("null");
        }
        printf(",\"heard\":");
        if (rows[i].heard) {
            cli_print_json_string(rows[i].heard);
        } else {
            printf("null");
        }
        if (rows[i].kept) {
            printf(",\"in_scope\":false");
        }
        printf(",\"fill\":\"%s\",\"source\":", piece_fill_name(rows[i].section->fill));
        cli_print_json_string(rows[i].source);
        printf("}");
    }
    printf("],\"score\":{\"numerator\":%lu,\"denominator\":%lu},\"seam\":", outcome->score.numerator,
           outcome->score.denominator);
    if (outcome->seam_planned) {
        printf("{\"progression\":%s,\"no_parallels\":%s,\"step\":%s,\"leading_tone\":%s,\"held\":%zu}",
               outcome->seam.progression ? "true" : "false", outcome->seam.no_parallels ? "true" : "false",
               outcome->seam.step ? "true" : "false", outcome->seam.leading_tone ? "true" : "false", outcome->seam.held);
    } else {
        printf("null");
    }
    printf(",\"analyzer_check\":");
    if (outcome->analyzer_check_count > 0) {
        const AnalyzerCheck *check = &outcome->analyzer_checks[0];
        printf("{\"planned\":%zu,\"agreed\":%zu,\"contradicts\":%s,\"items\":[", check->planned, check->agreed,
               check->contradicts ? "true" : "false");
        for (size_t i = 0; i < check->item_count; i++) {
            const AnalyzerCheckItem *item = &check->items[i];
            printf("%s{\"kind\":\"%s\",\"bar\":%zu,\"planned\":", i ? "," : "",
                   item->kind == ANALYZER_CHECK_CADENCE ? "cadence" : item->kind == ANALYZER_CHECK_KEY ? "key" : "violation",
                   item->bar);
            cli_print_json_string(item->planned);
            printf(",\"heard\":");
            cli_print_json_string(item->heard);
            printf(",\"agreed\":%s}", item->agreed ? "true" : "false");
        }
        printf("]}");
    } else {
        printf("null");
    }
    printf(",");
    if (outcome->style_measured) {
        cli_print_style_distance_json(&outcome->style_distance);
    } else {
        printf("\"style_distance\":null");
    }
    printf("}");
}

/* ------------------------------------------------------------- the files */

typedef enum {
    OUT_LOOP_PLAN,
    OUT_ENDING_PLAN,
    OUT_LOOP_MIDI,
    OUT_LOOP_MUSICXML,
    OUT_ENDING_MIDI,
    OUT_ENDING_MUSICXML,
    OUT_EXPLAIN
} OutputKind;

typedef struct {
    OutputKind kind;
    const char *path;
    char temporary[1032];
} Output;

static bool ends_with_json(const char *path) {
    size_t length = strlen(path);
    return length >= 5 && cli_equals_ignore_case(path + length - 5, ".json");
}

/* The explanation of both versions: text with a heading for each, or JSON {"loop": ..., "ending": ...}. */
static int write_explanation(const Piece *piece, const char *final_path, const char *write_path) {
    Explanation loop;
    Explanation ending;
    bool json = ends_with_json(final_path);
    if (explain_build(piece->loop.plan, &piece->loop.outcome, &loop) != EXPLAIN_OK) {
        return -1;
    }
    if (explain_build(piece->ending.plan, &piece->ending.outcome, &ending) != EXPLAIN_OK) {
        explain_destroy(&loop);
        return -1;
    }
    char *loop_text = json ? explain_to_json(&loop) : explain_to_text(&loop);
    char *ending_text = json ? explain_to_json(&ending) : explain_to_text(&ending);
    int status = -1;
    if (loop_text && ending_text) {
        FILE *file = fopen(write_path, "wb");
        if (file) {
            int printed = json ? fprintf(file, "{\"loop\":%s,\"ending\":%s}\n", loop_text, ending_text)
                               : fprintf(file, "== loop version ==\n%s\n== ending version ==\n%s\n", loop_text, ending_text);
            status = (fclose(file) == 0 && printed > 0) ? 0 : -1;
        }
    }
    free(loop_text);
    free(ending_text);
    explain_destroy(&loop);
    explain_destroy(&ending);
    return status;
}

/*
 * Writes every output to a temporary file next to its final name and renames them all only when every one was written,
 * so that an error leaves no output file behind.
 */
static int write_files(const Options *options, const Request *request, const Piece *piece, Output *outputs, size_t count) {
    RenderedPlan rendered[2];
    bool have[2] = {false, false};
    const Version *versions[2] = {&piece->loop, &piece->ending};
    PlanExportOptions loop_options = request->export_options;
    Rational start;
    Rational end;
    if (loop_region(piece->loop.plan, &start, &end)) {
        loop_options.loop_markers = true;
        loop_options.loop_start = start;
        loop_options.loop_end = end;
    }
    int status = 0;
    (void)options;
    for (size_t i = 0; i < count && status == 0; i++) {
        Output *output = &outputs[i];
        bool is_ending = output->kind == OUT_ENDING_PLAN || output->kind == OUT_ENDING_MIDI ||
                         output->kind == OUT_ENDING_MUSICXML;
        if (snprintf(output->temporary, sizeof(output->temporary), "%s.tmp", output->path) >= (int)sizeof(output->temporary)) {
            status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", output->path);
            break;
        }
        if (output->kind == OUT_LOOP_PLAN || output->kind == OUT_ENDING_PLAN) {
            PlanJsonError error;
            if (plan_write_json_file(versions[is_ending]->plan, output->temporary, &error) != PLAN_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", output->path, error.message);
            }
        } else if (output->kind == OUT_EXPLAIN) {
            if (write_explanation(piece, output->path, output->temporary) != 0) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", output->path);
            }
        } else {
            PlanExportError error;
            if (!have[is_ending]) {
                if (plan_export_render(versions[is_ending]->plan, &rendered[is_ending], NULL) != PLAN_EXPORT_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have[is_ending] = true;
            }
            bool midi = output->kind == OUT_LOOP_MIDI || output->kind == OUT_ENDING_MIDI;
            PlanExportResult result =
                midi ? rendered_plan_export_midi(&rendered[is_ending], is_ending ? &request->export_options : &loop_options,
                                                 output->temporary, &error)
                     : rendered_plan_export_musicxml(&rendered[is_ending], &request->export_options, output->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", output->path, error.message);
            }
        }
    }
    for (int v = 0; v < 2; v++) {
        if (have[v]) {
            rendered_plan_destroy(&rendered[v]);
        }
    }
    for (size_t i = 0; status == 0 && i < count; i++) {
        if (rename(outputs[i].temporary, outputs[i].path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", outputs[i].path);
        }
    }
    if (status != 0) {
        for (size_t i = 0; i < count; i++) {
            remove(outputs[i].temporary);
        }
    }
    return status;
}

static const char *output_path(const Output *outputs, size_t count, OutputKind kind) {
    for (size_t i = 0; i < count; i++) {
        if (outputs[i].kind == kind) {
            return outputs[i].path;
        }
    }
    return NULL;
}

static void print_result(const Options *options, const Request *request, const Piece *piece, const Output *outputs,
                         size_t count, bool json) {
    Rational start = {0, 1};
    Rational end = {0, 1};
    bool has_loop = loop_region(piece->loop.plan, &start, &end);
    char key[32];
    GrowBrief brief = make_brief(request);
    bool regenerated = options->from && !cli_equals_ignore_case(options->regenerate, "none");
    bool whole = !options->lane || cli_equals_ignore_case(options->lane, "all");
    cli_key_name(&request->structure.key, key, sizeof(key));
    if (json) {
        printf("{\"schema\":\"ursatz-piece\",\"seed\":%lu,\"key\":", piece->seed);
        cli_print_json_string(key);
        printf(",\"meter\":\"%d/%d\",", request->structure.meter.beats_per_bar, request->structure.meter.beat_unit);
        theme_print_brief(NULL, &brief, true);
        printf(",\"loop\":");
        if (has_loop) {
            printf("{\"start\":");
            print_time_json(start);
            printf(",\"end\":");
            print_time_json(end);
            printf("}");
        } else {
            printf("null");
        }
        if (options->from) {
            printf(",\"regenerated\":");
            if (regenerated) {
                printf("{\"section\":");
                cli_print_json_string(options->regenerate);
                printf(",\"lane\":%s%s%s}", whole ? "" : "\"", whole ? "\"all\"" : options->lane, whole ? "" : "\"");
            } else {
                printf("null");
            }
        }
        printf(",\"versions\":{");
        print_version_json("loop", request, &piece->loop, output_path(outputs, count, OUT_LOOP_PLAN));
        printf(",");
        print_version_json("ending", request, &piece->ending, output_path(outputs, count, OUT_ENDING_PLAN));
        printf("},\"files\":[");
        for (size_t i = 0; i < count; i++) {
            printf("%s", i ? "," : "");
            cli_print_json_string(outputs[i].path);
        }
        printf("]}\n");
        return;
    }
    printf("piece in %s, meter %d/%d, seed %lu\n", key, request->structure.meter.beats_per_bar,
           request->structure.meter.beat_unit, piece->seed);
    theme_print_brief(NULL, &brief, false);
    if (regenerated) {
        printf("regenerated %s of section %s\n", whole ? "all lanes" : options->lane, options->regenerate);
    } else if (options->from) {
        printf("regenerated nothing; the ending version is built again\n");
    }
    if (has_loop) {
        printf("loop from ");
        print_time(start);
        printf(" to ");
        print_time(end);
        printf(" (whole notes)\n");
    }
    print_version_text("loop", request, &piece->loop);
    print_version_text("ending", request, &piece->ending);
    for (size_t i = 0; i < count; i++) {
        printf("wrote %s\n", outputs[i].path);
    }
}

/* ------------------------------------------------------------- variations */

#define VARIATIONS_MAX 8
#define PATH_SIZE 1024
/* As many seeds are tried as it takes to find the variations asked for, at most this many for each. */
#define SEEDS_PER_VARIATION 4

typedef struct {
    Version version;
    unsigned long seed;
    uint64_t signature;
    char plan[PATH_SIZE];
    char midi[PATH_SIZE];
    char musicxml[PATH_SIZE];
} Variation;

typedef struct {
    const Options *options;
    const Request *request;
    const Variation *items;
    size_t kept;
    size_t seeds_tried;
    unsigned long requested;
} VariationsResult;

static void variations_destroy(Variation *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        version_destroy(&items[i].version);
    }
}

static void print_variations_json(const VariationsResult *result) {
    printf("{\"schema\":\"ursatz-piece-variations\",\"section\":");
    cli_print_json_string(result->options->variations);
    printf(",\"lane\":");
    cli_print_json_string(result->options->lane);
    printf(",\"variations\":[");
    for (size_t i = 0; i < result->kept; i++) {
        const Variation *variation = &result->items[i];
        printf("%s{\"index\":%zu,\"seed\":%lu,\"midi\":", i ? "," : "", i + 1, variation->seed);
        cli_print_json_string(variation->midi);
        printf(",\"musicxml\":");
        cli_print_json_string(variation->musicxml);
        printf(",");
        print_version_json("loop", result->request, &variation->version, variation->plan);
        printf("}");
    }
    printf("],\"kept\":%zu,\"requested\":%lu,\"seeds_tried\":%zu}\n", result->kept, result->requested, result->seeds_tried);
}

static void print_variations_text(const VariationsResult *result) {
    printf("%s of section %s\n", strcmp(result->options->lane, "harmony") == 0 ? "harmonizations" : "variations",
           result->options->variations);
    for (size_t i = 0; i < result->kept; i++) {
        const Variation *variation = &result->items[i];
        const ComposeOutcome *outcome = &variation->version.outcome;
        printf("variation %zu  seed %lu  score %lu/%lu", i + 1, variation->seed, outcome->score.numerator, outcome->score.denominator);
        if (outcome->seam_planned) {
            printf("  seam %zu of 4 rules held", outcome->seam.held);
        }
        printf("\n");
    }
    printf("kept %zu of %lu, %zu seeds tried\n", result->kept, result->requested, result->seeds_tried);
    for (size_t i = 0; i < result->kept; i++) {
        printf("wrote %s, %s, %s\n", result->items[i].plan, result->items[i].midi, result->items[i].musicxml);
    }
}

/* Writes what print prints to a file next to its final name, then renames it. */
static int write_result_file(const VariationsResult *result, const char *path) {
    char temporary[PATH_SIZE + 8];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    int file = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (saved < 0 || file < 0 || dup2(file, STDOUT_FILENO) < 0) {
        if (saved >= 0) {
            close(saved);
        }
        if (file >= 0) {
            close(file);
        }
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", path);
    }
    close(file);
    print_variations_json(result);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", path);
    }
    return 0;
}

/* variation-<i>.json, .mid and .musicxml of the loop version, and result.json, in the directory. */
static int write_variations(const Options *options, const Request *request, Variation *items, size_t kept, size_t seeds_tried,
                            unsigned long requested, bool json) {
    if (mkdir(options->out_dir, 0755) != 0 && errno != EEXIST) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", options->out_dir, strerror(errno));
    }
    int status = 0;
    for (size_t i = 0; status == 0 && i < kept; i++) {
        Variation *variation = &items[i];
        if (snprintf(variation->plan, PATH_SIZE, "%s/variation-%zu.json", options->out_dir, i + 1) >= PATH_SIZE ||
            snprintf(variation->midi, PATH_SIZE, "%s/variation-%zu.mid", options->out_dir, i + 1) >= PATH_SIZE ||
            snprintf(variation->musicxml, PATH_SIZE, "%s/variation-%zu.musicxml", options->out_dir, i + 1) >= PATH_SIZE) {
            return cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
        }
        Output outputs[3] = {{OUT_LOOP_PLAN, variation->plan, ""}, {OUT_LOOP_MIDI, variation->midi, ""},
                             {OUT_LOOP_MUSICXML, variation->musicxml, ""}};
        Piece piece;
        memset(&piece, 0, sizeof(piece));
        piece.loop = variation->version;
        status = write_files(options, request, &piece, outputs, 3);
    }
    VariationsResult result = {options, request, items, kept, seeds_tried, requested};
    char path[PATH_SIZE];
    if (status == 0 && snprintf(path, PATH_SIZE, "%s/result.json", options->out_dir) >= PATH_SIZE) {
        status = cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
    }
    if (status == 0) {
        status = write_result_file(&result, path);
    }
    if (status == 0) {
        json ? print_variations_json(&result) : print_variations_text(&result);
    }
    return status;
}

/*
 * Variations of one section: the section is written again from one seed after another (as --regenerate does, the seam and
 * the loop's end included) and a version is kept when it sounds different from the plan and from the ones kept before.
 * Returns the exit status.
 */
static int make_variations(const Options *options, const Request *request, const Edit *edit, unsigned long seed, bool json) {
    unsigned long requested = 0;
    if (!options->count || !cli_parse_whole_number(options->count, VARIATIONS_MAX, &requested) || requested < 1) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'", VARIATIONS_MAX,
                        options->count ? options->count : "");
    }
    PiecePlan *original = NULL;
    PlanJsonError error;
    if (plan_from_json(edit->text, &original, &error) != PLAN_OK) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the loop plan: %s", error.message);
    }
    uint64_t original_signature = plan_signature(original);
    plan_destroy(original);
    Variation items[VARIATIONS_MAX];
    memset(items, 0, sizeof(items));
    size_t kept = 0;
    size_t tried = 0;
    int status = 0;
    for (size_t attempt = 0; status == 0 && kept < requested && attempt < SEEDS_PER_VARIATION * requested; attempt++) {
        Variation *next = &items[kept];
        unsigned long used = (seed + attempt) & CLI_SEED_MAX;
        PieceResult result = regenerate_loop(request, edit, options->check, used, &next->version, &status);
        tried++;
        bool keep = result == PIECE_OK && status == 0;
        if (keep) {
            next->signature = plan_signature(next->version.plan);
            keep = next->signature != original_signature;
            for (size_t i = 0; keep && i < kept; i++) {
                keep = items[i].signature != next->signature;
            }
        }
        if (keep) {
            next->seed = used;
            kept++;
        } else {
            version_destroy(&next->version);
        }
    }
    if (status == 0 && kept == 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "no version of section '%s' that differs from the plan was found in %zu seeds from %lu",
                          options->variations, tried, seed);
    }
    if (status == 0) {
        status = write_variations(options, request, items, kept, tried, requested, json);
    }
    variations_destroy(items, kept);
    return status;
}

/* ------------------------------------------------------------- the command */

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n", PIECE_USAGE_LINE + 7);
    fprintf(out, "%s\n", PIECE_REGENERATE_USAGE_LINE);
    fprintf(out, "%s\n\n", PIECE_VARIATIONS_USAGE_LINE);
    fprintf(out, "Composes a looping game track from a request (JSON, schema %s): an intro, a body, a turnaround and an\n", SCHEMA_NAME);
    fprintf(out, "ending, each a list of sections of any length in beats with a pickup, a cadence and a fill (nothing, a motive,\n");
    fprintf(out, "a composed theme or a progression), in a key and a meter, with an accompaniment or a voicing and a style. Two\n");
    fprintf(out, "versions are made: the loop (intro once, then the body and the turnaround, which lead back to the body's start)\n");
    fprintf(out, "and the ending (the intro and the body as the loop has them, then the ending). See docs/piece-request.md.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --seed <n>               whole number from 0; the same seed gives the same output (default: from the clock)\n");
    fprintf(out, "  --check                  analyze both versions with the Ursatz Analyzer and pass over a seed whose analysis\n");
    fprintf(out, "                           contradicts the plan\n");
    fprintf(out, "  --out <loop-plan.json>   write the loop version's plan as JSON\n");
    fprintf(out, "  --ending-out <file>      write the ending version's plan as JSON\n");
    fprintf(out, "  --midi <file>            write the loop version as a MIDI file, with loopStart and loopEnd markers\n");
    fprintf(out, "  --musicxml <file>        write the loop version as MusicXML\n");
    fprintf(out, "  --ending-midi <file>     write the ending version as a MIDI file\n");
    fprintf(out, "  --ending-musicxml <file> write the ending version as MusicXML\n");
    fprintf(out, "  --plain-midi             MIDI: no track names and no section markers (the loop markers stay)\n");
    fprintf(out, "  --explain <file>         write why each candidate was chosen or rejected, in both versions\n");
    fprintf(out, "  --format <text|json>     how to print the result on stdout (default: text)\n");
    fprintf(out, "  --from <loop-plan.json>  work on a loop plan the command wrote, with --request (the request it was made from)\n");
    fprintf(out, "  --regenerate <id|none>   write the section with that id again (the lane given, or all of it), the rest locked;\n");
    fprintf(out, "                           the seam is written again when the section holds the body's first bar, and the\n");
    fprintf(out, "                           ending version is always built again from the new loop. none: only the ending version.\n");
    fprintf(out, "                           A section filled with a theme needs --lane\n");
    fprintf(out, "  --lane <lane>            harmony, bass, melody, inner or all (default all)\n");
    fprintf(out, "  --variations <id>        make --count different versions of the section (--lane needed; harmony over a given\n");
    fprintf(out, "                           melody is a harmonization), each written to --out-dir as variation-<i>.json, .mid and\n");
    fprintf(out, "                           .musicxml (the loop version) with result.json: the seeds, scores, seam and cadences\n");
    fprintf(out, "  --count <n>              how many variations, 1 to %d\n", VARIATIONS_MAX);
    fprintf(out, "  --out-dir <dir>          where the variations are written\n");
    fprintf(out, "  --help                   this text\n\n");
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
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out;
        } else if (strcmp(arg, "--ending-out") == 0) {
            target = &options->ending_out;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml;
        } else if (strcmp(arg, "--ending-midi") == 0) {
            target = &options->ending_midi;
        } else if (strcmp(arg, "--ending-musicxml") == 0) {
            target = &options->ending_musicxml;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format;
        } else if (strcmp(arg, "--from") == 0) {
            target = &options->from;
        } else if (strcmp(arg, "--request") == 0) {
            target = &options->request;
        } else if (strcmp(arg, "--regenerate") == 0) {
            target = &options->regenerate;
        } else if (strcmp(arg, "--lane") == 0) {
            target = &options->lane;
        } else if (strcmp(arg, "--variations") == 0) {
            target = &options->variations;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count;
        } else if (strcmp(arg, "--out-dir") == 0) {
            target = &options->out_dir;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer piece --help)", arg);
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

/* The status for a piece that no seed composed: what the last try ran into. */
static int fail_compose(PieceResult result, const Piece *last, unsigned long first_seed, unsigned long last_seed) {
    if (result == PIECE_ERR_MOTIVE_TOO_LONG) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "the motive does not fit in one bar of the meter");
    }
    if (result == PIECE_ERR_BAD_REQUEST) {
        return cli_fail(CLI_EXIT_USAGE, "this piece cannot be built as asked: a theme fill must be as long as its section, in "
                                        "the same meter and mode; a progression must be readable and fit its section");
    }
    if (result == PIECE_ERR_STUCK) {
        const Version *stuck = last->ending.composed && last->ending.outcome.stuck ? &last->ending : &last->loop;
        const ComposeRejection *rejection =
            stuck->outcome.rejection_count > 0 ? &stuck->outcome.rejections[stuck->outcome.rejection_count - 1] : NULL;
        return cli_fail(CLI_EXIT_UNSATISFIABLE,
                        "no piece could be composed from seeds %lu to %lu (the last was stuck in the %s version, %s stage%s%s)%s",
                        first_seed, last_seed, stuck == &last->ending ? "ending" : "loop",
                        rejection && rejection->stage[0] ? rejection->stage : "?", rejection ? ", " : "",
                        rejection ? rejection->reason : "",
                        rejection && (strcmp(rejection->stage, "seam") == 0 || strcmp(rejection->stage, "structure") == 0 || !rejection->stage[0])
                            ? "; a section of whole bars needs 2 bars at least to hold its cadence"
                            : "");
    }
    return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", piece_result_name(result));
}

/* The outputs asked for, written, and the result printed. Returns the exit status. */
static int emit(const Options *options, const Request *request, const Piece *piece, bool json) {
    Output outputs[OUTPUT_MAX];
    size_t count = 0;
    const struct {
        OutputKind kind;
        const char *path;
    } wanted[] = {{OUT_LOOP_PLAN, options->out},          {OUT_ENDING_PLAN, options->ending_out},
                  {OUT_LOOP_MIDI, options->midi},         {OUT_LOOP_MUSICXML, options->musicxml},
                  {OUT_ENDING_MIDI, options->ending_midi}, {OUT_ENDING_MUSICXML, options->ending_musicxml},
                  {OUT_EXPLAIN, options->explain}};
    for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]); i++) {
        if (wanted[i].path) {
            outputs[count].kind = wanted[i].kind;
            outputs[count].path = wanted[i].path;
            count++;
        }
    }
    int status = write_files(options, request, piece, outputs, count);
    if (status == 0) {
        print_result(options, request, piece, outputs, count, json);
    }
    return status;
}

/* The options that belong with --from, and the ones that do not. Returns 0 or the status of the usage error printed. */
static int check_edit_options(const Options *options) {
    if (options->from) {
        if (options->input) {
            return cli_fail(CLI_EXIT_USAGE, "with --from the request is given as --request, not as an argument\n%s", PIECE_USAGE_LINE);
        }
        if (!options->request) {
            return cli_fail(CLI_EXIT_USAGE, "--from needs --request <request.json>\n%s", PIECE_USAGE_LINE);
        }
        if (!options->regenerate == !options->variations) {
            return cli_fail(CLI_EXIT_USAGE, "--from needs --regenerate <section id>|none or --variations <section id>, not both\n%s",
                            PIECE_USAGE_LINE);
        }
        if (options->lane && !cli_equals_ignore_case(options->lane, "all") && !cli_equals_ignore_case(options->lane, "harmony") &&
            !cli_equals_ignore_case(options->lane, "bass") && !cli_equals_ignore_case(options->lane, "melody") &&
            !cli_equals_ignore_case(options->lane, "inner")) {
            return cli_fail(CLI_EXIT_USAGE, "--lane must be harmony, bass, melody, inner or all, not '%s'", options->lane);
        }
        if (options->variations) {
            if (!options->lane || !options->count || !options->out_dir) {
                return cli_fail(CLI_EXIT_USAGE, "--variations needs --lane, --count and --out-dir\n%s", PIECE_VARIATIONS_USAGE_LINE);
            }
            if (options->out || options->ending_out || options->midi || options->musicxml || options->ending_midi ||
                options->ending_musicxml || options->explain) {
                return cli_fail(CLI_EXIT_USAGE, "--variations writes its files to --out-dir; the other output options do not apply");
            }
        } else if (options->count || options->out_dir) {
            return cli_fail(CLI_EXIT_USAGE, "--count and --out-dir go with --variations");
        }
        return 0;
    }
    if (options->request || options->regenerate || options->lane || options->variations || options->count || options->out_dir) {
        return cli_fail(CLI_EXIT_USAGE, "--request, --regenerate, --variations, --lane, --count and --out-dir go with --from\n%s",
                        PIECE_USAGE_LINE);
    }
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "piece needs a request file\n%s", PIECE_USAGE_LINE);
    }
    return 0;
}

int run_piece(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    status = check_edit_options(&options);
    if (status != 0) {
        return status;
    }
    unsigned long seed = cli_default_seed();
    if (options.seed && !cli_parse_whole_number(options.seed, CLI_SEED_MAX, &seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'", options.seed);
    }
    bool json = false;
    if (options.format) {
        json = cli_equals_ignore_case(options.format, "json");
        if (!json && !cli_equals_ignore_case(options.format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options.format);
        }
    }
    Request *request = calloc(1, sizeof(*request));
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    status = read_request(request, options.from ? options.request : options.input, options.plain_midi);
    if (status != 0) {
        request_destroy(request);
        free(request);
        return status;
    }
    Edit edit;
    memset(&edit, 0, sizeof(edit));
    char *plan_text = NULL;
    if (options.from) {
        status = cli_load_plan_text(options.from, &plan_text);
        if (status != 0) {
            request_destroy(request);
            free(request);
            return status;
        }
        edit.text = plan_text;
        edit.section = options.variations ? options.variations
                       : cli_equals_ignore_case(options.regenerate, "none") ? NULL : options.regenerate;
        edit.lane = options.lane && !cli_equals_ignore_case(options.lane, "all") ? options.lane : NULL;
    }

    if (options.variations) {
        status = make_variations(&options, request, &edit, seed, json);
        free(plan_text);
        request_destroy(request);
        free(request);
        return status;
    }

    /* The seeds are tried one after another; a stuck run is passed over, and a request no seed can mend ends the search. */
    Piece piece;
    PieceResult result = PIECE_ERR_STUCK;
    unsigned long last_seed = seed;
    memset(&piece, 0, sizeof(piece));
    for (unsigned long attempt = 0; attempt < CLI_MAX_SEED_TRIES && result == PIECE_ERR_STUCK; attempt++) {
        piece_destroy(&piece);
        last_seed = (seed + attempt) & CLI_SEED_MAX;
        result = options.from ? regenerate_piece(request, &edit, options.check, last_seed, &piece, &status)
                              : compose_piece(request, options.check, last_seed, &piece);
    }
    if (status != 0) {
        /* The edit was refused: the error is printed already. */
    } else if (result != PIECE_OK) {
        status = fail_compose(result, &piece, seed, last_seed);
    } else {
        status = emit(&options, request, &piece, json);
    }
    piece_destroy(&piece);
    free(plan_text);
    request_destroy(request);
    free(request);
    return status;
}
