#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_json.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "util/plan_util.h"
#include "util/request_parse.h"

/*
 * The piece request (docs/piece-request.md, schema "ursatz-piece-request" version 1) as the library reads it: the brief, the
 * sections with their fills, the motives, the saved forms, the style and the tempo. A theme fill carries its plan as an
 * object (the caller reads the file); the other members are the request file's.
 */

#define SCHEMA_NAME "ursatz-piece-request"

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

void api_piece_request_destroy(PieceSpec *request) {
    for (size_t i = 0; i <= PIECE_STRUCTURE_MAX_SECTIONS; i++) {
        free(request->section_motive[i]);
        free(request->numeral_text[i]);
        plan_destroy(request->theme_plan[i]);
    }
    free(request->piece_motive);
    free(request->pool);
    free(request->pool_motives);
    composer_style_destroy(request->style);
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
        if (request_equals_ignore_case(plus + 1, ENDINGS[i])) {
            pairing->ending = (PlanThemeEnding)i;
            ending_found = true;
        }
    }
    return beginning_found && ending_found;
}

/* A fill's numerals, "I vi IV V", split into the section's own copy. */
static ComposerStatus read_numerals(PieceSpec *request, size_t index, const char *id, const char *path, const char *text,
                                    ComposerError *error) {
    request->numeral_text[index] = strdup(text);
    if (!request->numeral_text[index]) {
        return fail_alloc(error);
    }
    size_t count = 0;
    char *save = NULL;
    for (char *word = strtok_r(request->numeral_text[index], " \t,", &save); word; word = strtok_r(NULL, " \t,", &save)) {
        if (count == PIECE_NUMERALS_MAX) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': a progression has at most %d chords", id,
                            PIECE_NUMERALS_MAX);
        }
        request->numerals[index][count++] = word;
    }
    if (count == 0) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': the progression has no numerals", id);
    }
    request->sections[index].numerals = request->numerals[index];
    request->sections[index].numeral_count = count;
    return COMPOSER_OK;
}

static ComposerStatus read_fill(PieceSpec *request, size_t index, const cJSON *fill, const char *base, ComposerError *error) {
    PieceSection *section = &request->sections[index];
    const char *id = section->id;
    const char *kind = "empty";
    char path[128];
    if (fill && !cJSON_IsNull(fill)) {
        if (!cJSON_IsObject(fill)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base, "\"fill\" in the request must be an object");
        }
        if (!api_request_string(fill, base, "kind", true, &kind, error)) {
            return COMPOSER_ERR_REQUEST;
        }
    } else {
        fill = NULL;
    }
    if (!piece_fill_from_name(kind, &section->fill)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base,
                        "section '%s': the fill kind must be empty, motif, theme or progression, not '%s'", id, kind);
    }
    snprintf(request->source[index], sizeof(request->source[index]), "basic motive");
    if (section->fill == PIECE_FILL_MOTIF) {
        const char *into = NULL;
        const char *hybrid = NULL;
        if (!api_request_string(fill, base, "into", false, &into, error) ||
            !api_request_string(fill, base, "hybrid", false, &hybrid, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        request->motive_json[index] = cJSON_GetObjectItemCaseSensitive(fill, "motive");
        request->forms_json[index] = cJSON_GetObjectItemCaseSensitive(fill, "forms");
        if (cJSON_IsNull(request->motive_json[index])) {
            request->motive_json[index] = NULL;
        }
        if (request->motive_json[index]) {
            if (!cJSON_IsObject(request->motive_json[index])) {
                snprintf(path, sizeof path, "%s/motive", base);
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"motive\" must be a motive", id);
            }
            snprintf(request->source[index], sizeof(request->source[index]), "own motive");
        }
        if (request->forms_json[index] && !cJSON_IsNull(request->forms_json[index]) &&
            !cJSON_IsArray(request->forms_json[index])) {
            snprintf(path, sizeof path, "%s/forms", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"forms\" must be a list", id);
        }
        snprintf(path, sizeof path, "%s/into", base);
        if (into) {
            section->has_into = true;
            if (request_equals_ignore_case(into, "sentence")) {
                section->into = PLAN_ARCHETYPE_SENTENCE;
            } else if (request_equals_ignore_case(into, "period")) {
                section->into = PLAN_ARCHETYPE_PERIOD;
            } else if (request_equals_ignore_case(into, "hybrid")) {
                section->into = PLAN_ARCHETYPE_HYBRID;
            } else {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"into\" must be sentence, period or hybrid, not '%s'",
                                id, into);
            }
        }
        if (section->has_into && section->into == PLAN_ARCHETYPE_HYBRID) {
            snprintf(path, sizeof path, "%s/hybrid", base);
            if (!hybrid || !parse_hybrid(hybrid, &section->hybrid)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path,
                                "section '%s': a hybrid needs \"hybrid\": \"<beginning>+<ending>\", for example "
                                "antecedent+continuation",
                                id);
            }
            bool sentence = section->hybrid.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
                            section->hybrid.ending == PLAN_THEME_ENDING_CONTINUATION;
            bool period = section->hybrid.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
                          section->hybrid.ending == PLAN_THEME_ENDING_CONSEQUENT;
            if (sentence || period) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': %s is a %s, not a hybrid", id, hybrid,
                                sentence ? "sentence" : "period");
            }
        } else if (hybrid) {
            snprintf(path, sizeof path, "%s/hybrid", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"hybrid\" applies only to \"into\": \"hybrid\"", id);
        }
    } else if (section->fill == PIECE_FILL_THEME) {
        /* The caller reads the plan file; the request carries the plan. */
        const char *label = NULL;
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(fill, "plan");
        snprintf(path, sizeof path, "%s/plan", base);
        if (!cJSON_IsObject(plan)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            cJSON_IsString(plan) ? "section '%s': a theme fill carries its plan (\"plan\": <plan object>), not a path; "
                                                   "the caller reads the file"
                                                 : "section '%s': a theme fill needs \"plan\": <plan object>",
                            id);
        }
        if (!api_request_string(fill, base, "source", false, &label, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        char *text = cJSON_PrintUnformatted(plan);
        PlanJsonError detail;
        memset(&detail, 0, sizeof detail);
        PlanResult read = text ? plan_from_json(text, &request->theme_plan[index], &detail) : PLAN_ERR_ALLOC;
        cJSON_free(text);
        if (read == PLAN_ERR_ALLOC) {
            return fail_alloc(error);
        }
        if (read != PLAN_OK) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': cannot read the theme plan '%s': %s", id,
                            label ? label : "plan", detail.message);
        }
        section->theme_plan = request->theme_plan[index];
        snprintf(request->source[index], sizeof(request->source[index]), "%s", label ? label : "theme plan");
    } else if (section->fill == PIECE_FILL_PROGRESSION) {
        const char *numerals = NULL;
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        if (!api_request_string(fill, base, "numerals", true, &numerals, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        snprintf(path, sizeof path, "%s/numerals", base);
        ComposerStatus status = read_numerals(request, index, id, path, numerals, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        if (cells && !cJSON_IsNull(cells)) {
            if (!cJSON_IsNumber(cells) || cells->valuedouble < 1 || cells->valuedouble != (double)(size_t)cells->valuedouble) {
                snprintf(path, sizeof path, "%s/cells", base);
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"cells\" must be a whole number from 1", id);
            }
            section->cells_per_chord = (size_t)cells->valuedouble;
        }
        snprintf(request->source[index], sizeof(request->source[index]), "%s", numerals);
    }
    return COMPOSER_OK;
}

static ComposerStatus read_section(PieceSpec *request, size_t index, const cJSON *item, const char *base,
                                   const PlanMeter *meter, ComposerError *error) {
    PieceSection *section = &request->sections[index];
    const char *id = NULL;
    const char *part = NULL;
    const char *letter = NULL;
    const char *cadence = NULL;
    const char *pickup = NULL;
    char pickup_text[32];
    char path[128];
    if (!cJSON_IsObject(item)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "every section of the request must be an object");
    }
    if (!api_request_string(item, base, "id", true, &id, error) || !api_request_string(item, base, "part", true, &part, error) ||
        !api_request_string(item, base, "letter", false, &letter, error) ||
        !api_request_string(item, base, "cadence", false, &cadence, error) ||
        !api_request_pickup(item, base, pickup_text, sizeof(pickup_text), &pickup, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (strlen(id) == 0 || strlen(id) >= PIECE_SECTION_ID_MAX) {
        snprintf(path, sizeof path, "%s/id", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "a section id is 1 to %d characters, not '%s'", PIECE_SECTION_ID_MAX - 1,
                        id);
    }
    snprintf(section->id, sizeof(section->id), "%s", id);
    if (!piece_part_from_name(part, &section->part)) {
        snprintf(path, sizeof path, "%s/part", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': the part must be intro, body, turnaround or ending, not '%s'",
                        id, part);
    }
    if (letter) {
        snprintf(section->letter, sizeof(section->letter), "%s", letter);
    } else {
        snprintf(section->letter, sizeof(section->letter), "%c", toupper((unsigned char)id[0]));
    }
    const cJSON *beats = cJSON_GetObjectItemCaseSensitive(item, "beats");
    if (!cJSON_IsNumber(beats) || beats->valuedouble < 1 || beats->valuedouble > 4096 ||
        beats->valuedouble != (double)(size_t)beats->valuedouble) {
        snprintf(path, sizeof path, "%s/beats", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"beats\" must be a whole number of beats, 1 or more", id);
    }
    section->beats = (size_t)beats->valuedouble;
    section->pickup = (Rational){0, 1};
    if (pickup && !request_parse_pickup(pickup, meter->beat_unit, &section->pickup)) {
        snprintf(path, sizeof path, "%s/pickup", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': pickup '%s' is not a number of beats; write 0, 1 or 1/2", id,
                        pickup);
    }
    if (cadence) {
        if (!request_parse_cadence(cadence, &section->cadence)) {
            snprintf(path, sizeof path, "%s/cadence", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': the cadence must be PAC, IAC, HC, DC or PC, not '%s'",
                            id, cadence);
        }
        section->has_cadence = true;
    }
    snprintf(path, sizeof path, "%s/fill", base);
    return read_fill(request, index, cJSON_GetObjectItemCaseSensitive(item, "fill"), path, error);
}

/* The brief: key, mode, meter, the style and its weight, the tempo, the accompaniment or voicing. */
static ComposerStatus read_brief(PieceSpec *request, const cJSON *root, const char *base, ComposerError *error) {
    const char *style_weight = NULL;
    const char *voicing = NULL;
    char path[128];
    if (!api_request_string(root, base, "key", true, &request->key_text, error) ||
        !api_request_string(root, base, "mode", true, &request->mode_text, error) ||
        !api_request_string(root, base, "meter", true, &request->meter_text, error) ||
        (!request->style_path && !api_request_string(root, base, "style", false, &request->style_path, error)) ||
        !api_request_string(root, base, "style_weight", false, &style_weight, error) ||
        !api_request_string(root, base, "voicing", false, &voicing, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    PieceStructure *structure = &request->structure;
    const char *mode = request->mode_text;
    snprintf(path, sizeof path, "%s/mode", base);
    if (!request_equals_ignore_case(mode, "major") && !request_equals_ignore_case(mode, "minor") &&
        !request_equals_ignore_case(mode, "style")) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"mode\" must be major, minor or style, not '%s'", mode);
    }
    if (request_equals_ignore_case(mode, "style") && !request->style_path) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"mode\": \"style\" needs a \"style\"");
    }
    /* The key may be a tonic alone: the mode is the one "mode" gives. */
    char tonic_key[64];
    const char *key_text = request->key_text;
    if (strlen(key_text) + 7 < sizeof(tonic_key) && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof(tonic_key), "%s major", key_text);
        key_text = tonic_key;
    }
    if (!request_parse_key(key_text, &structure->key)) {
        snprintf(path, sizeof path, "%s/key", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, for example \"D\"", request->key_text);
    }
    if (request_equals_ignore_case(mode, "major")) {
        structure->key.mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (request_equals_ignore_case(mode, "minor")) {
        structure->key.mode = KEY_STRUCTURE_MODE_MINOR;
    }
    if (!request_parse_meter(request->meter_text, &structure->meter) || !harmony_meter_supported(&structure->meter)) {
        snprintf(path, sizeof path, "%s/meter", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8",
                        request->meter_text);
    }
    request->weight = (ComposerRatio){1, 1};
    if (style_weight && !request_parse_ratio(style_weight, &request->weight)) {
        snprintf(path, sizeof path, "%s/style_weight", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "\"style_weight\" must be a fraction like 1/2 or a whole number, not '%s'", style_weight);
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(root, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        snprintf(path, sizeof path, "%s/without", base);
        if (!request->style_path) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"without\" must be a list of categories");
            }
            bool known = false;
            for (size_t i = 0; i < request->without_count; i++) {
                known = known || strcmp(request->without[i], category->valuestring) == 0;
            }
            if (known) {
                continue;
            }
            if (request->without_count >= PIECE_WITHOUT_MAX) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"without\" names at most %d style categories", PIECE_WITHOUT_MAX);
            }
            request->without[request->without_count++] = category->valuestring;
        }
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(root, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern = NULL;
        snprintf(path, sizeof path, "%s/accompaniment", base);
        if (!cJSON_IsObject(accompaniment)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"accompaniment\" must be null or {\"pattern\": ...}");
        }
        if (!api_request_string(accompaniment, path, "pattern", true, &pattern, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        if (strcmp(pattern, "motive") == 0) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "an accompaniment of pattern \"motive\" is not available in a piece");
        }
        ComposerStatus status =
            api_read_accompaniment(pattern, NULL, request->meter_text, &structure->meter, &structure->accompaniment, error);
        if (status != COMPOSER_OK) {
            snprintf(error->path, sizeof error->path, "%s", path);
            return status;
        }
        structure->has_accompaniment = true;
    }
    if (voicing) {
        snprintf(path, sizeof path, "%s/voicing", base);
        if (structure->has_accompaniment) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            "\"voicing\" writes the inner voices itself; it cannot be combined with an \"accompaniment\"");
        }
        if (strcmp(voicing, "satb") != 0 && strcmp(voicing, "chorale") != 0) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"voicing\" in the request must be satb or chorale, not '%s'",
                            voicing);
        }
        structure->has_voicing = true;
        structure->voicing.write_soprano = strcmp(voicing, "chorale") == 0;
    }
    return COMPOSER_OK;
}

/* The style, then the tempo and the mode it may give, as grow does. */
static ComposerStatus load_style(PieceSpec *request, const cJSON *root, const char *base, bool plain_midi,
                                 ComposerError *error) {
    char path[128];
    if (request->style_path) {
        snprintf(path, sizeof path, "%s/style", base);
        ComposerStatus status =
            api_load_style(request->style_path, request->without, request->without_count, path, path, &request->style, error);
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    request->export_options = plan_export_options_default();
    if (plain_midi) {
        request->export_options.track_names = false;
        request->export_options.section_markers = false;
    }
    ComposerStatus status = api_read_tempo(root, &request->export_options, error);
    if (status != COMPOSER_OK) {
        snprintf(path, sizeof path, "%s%s", base, error->path);
        snprintf(error->path, sizeof error->path, "%s", path);
        return status;
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(root, "tempo");
    request->tempo = (ComposerRatio){0, 0};
    request->tempo_from = "none";
    if (tempo && !cJSON_IsNull(tempo)) {
        request->tempo.numerator = request->export_options.tempo_numerator;
        request->tempo.denominator = request->export_options.tempo_denominator;
        request->tempo_from = "option";
    } else if (composer_style_tempo(request->style, &request->tempo)) {
        request->export_options.tempo_numerator = request->tempo.numerator;
        request->export_options.tempo_denominator = request->tempo.denominator;
        request->tempo_from = "style";
    }
    request->mode_from = "request";
    if (request_equals_ignore_case(request->mode_text, "style")) {
        bool minor = false;
        if (!composer_style_mode(request->style, &minor)) {
            snprintf(path, sizeof path, "%s/mode", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"mode\": \"style\" needs a style with a mode category");
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
        for (size_t i = 0; i < request->without_count && i < PLAN_STYLE_WITHOUT_MAX; i++) {
            snprintf(plan_style->without[plan_style->without_count++], PLAN_STYLE_WITHOUT_NAME_MAX, "%s", request->without[i]);
        }
        request->structure.has_style = true;
    }
    return COMPOSER_OK;
}

/* A melodic motive of the request. */
static ComposerStatus read_melodic(const char *what, const char *path, const cJSON *json, const PlanMeter *meter,
                                   const AnalysisKey *key, Motive **out, ComposerError *error) {
    *out = calloc(1, sizeof(Motive));
    if (!*out) {
        return fail_alloc(error);
    }
    ComposerStatus status = api_read_degrees_motive(what, path, json, NULL, meter, key, *out, error);
    if (status == COMPOSER_OK && (*out)->accompaniment) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s is of the accompaniment role; a piece takes a melodic motive", what);
    }
    return status;
}

/* The motives, and the saved forms the motives stage may use; the key and the meter are known now. */
static ComposerStatus load_motives(PieceSpec *request, const char *base, ComposerError *error) {
    PieceStructure *structure = &request->structure;
    char what[96];
    char path[128];
    ComposerStatus status = COMPOSER_OK;
    if (request->piece_motive_json) {
        snprintf(path, sizeof path, "%s/motive", base);
        status = read_melodic("the motive of the piece", path, request->piece_motive_json, &structure->meter, &structure->key,
                              &request->piece_motive, error);
        structure->motive = request->piece_motive;
    }
    size_t forms = 0;
    for (size_t i = 0; status == COMPOSER_OK && i < structure->section_count; i++) {
        if (request->motive_json[i]) {
            snprintf(what, sizeof(what), "the motive of section '%s'", request->sections[i].id);
            snprintf(path, sizeof path, "%s/sections/%zu/fill/motive", base, i);
            status = read_melodic(what, path, request->motive_json[i], &structure->meter, &structure->key,
                                  &request->section_motive[i], error);
            request->sections[i].motive = request->section_motive[i];
        }
        const cJSON *list = request->forms_json[i];
        const cJSON *form = NULL;
        cJSON_ArrayForEach(form, list) {
            forms++;
        }
    }
    if (status != COMPOSER_OK || forms == 0) {
        return status;
    }
    if (forms > PIECE_POOL_MAX) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/request/sections", "the request has more than %d saved forms", PIECE_POOL_MAX);
    }
    request->pool = calloc(forms, sizeof(*request->pool));
    request->pool_motives = calloc(forms, sizeof(*request->pool_motives));
    if (!request->pool || !request->pool_motives) {
        return fail_alloc(error);
    }
    for (size_t i = 0; i < structure->section_count; i++) {
        const cJSON *form = NULL;
        size_t at = 0;
        cJSON_ArrayForEach(form, request->forms_json[i]) {
            const char *item_id = NULL;
            const char *flag = NULL;
            snprintf(path, sizeof path, "%s/sections/%zu/fill/forms/%zu", base, i, at++);
            if (!cJSON_IsObject(form)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "every saved form must be an object");
            }
            if (!api_request_string(form, path, "item", true, &item_id, error) ||
                !api_request_string(form, path, "flag", true, &flag, error)) {
                return COMPOSER_ERR_REQUEST;
            }
            if (strcmp(flag, "rejected") == 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "the form '%s' is rejected; leave it out of the request", item_id);
            }
            if (strcmp(flag, "approved") != 0 && strcmp(flag, "unflagged") != 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "the flag of form '%s' must be approved or unflagged, not '%s'",
                                item_id, flag);
            }
            snprintf(what, sizeof(what), "form '%s'", item_id);
            status = api_read_degrees_motive(what, path, form, NULL, &structure->meter, &structure->key,
                                             &request->pool_motives[request->pool_count], error);
            if (status != COMPOSER_OK) {
                return status;
            }
            GrowPoolForm *entry = &request->pool[request->pool_count];
            const cJSON *chain = cJSON_GetObjectItemCaseSensitive(form, "chain");
            entry->motive = &request->pool_motives[request->pool_count];
            entry->slot_motive_of = GROW_SLOT_BASIC;
            entry->approved = strcmp(flag, "approved") == 0;
            snprintf(entry->item, sizeof(entry->item), "%s", item_id);
            snprintf(entry->chain, sizeof(entry->chain), "%s", cJSON_IsString(chain) ? chain->valuestring : "");
            status = api_read_functions(what, path, cJSON_GetObjectItemCaseSensitive(form, "functions"), &entry->functions, error);
            if (status != COMPOSER_OK) {
                return status;
            }
            request->pool_count++;
        }
    }
    return COMPOSER_OK;
}

ComposerStatus api_piece_read_request(PieceSpec *request, const cJSON *root, const char *base, const char *style_path,
                                      bool plain_midi, const ComposerJob *job, ComposerError *error) {
    char path[128];
    memset(request, 0, sizeof(*request));
    request->job = job;
    request->style_path = style_path;
    if (!cJSON_IsObject(root)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "the piece request must be a JSON object");
    }
    const char *schema = NULL;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!api_request_string(root, base, "schema", true, &schema, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (strcmp(schema, SCHEMA_NAME) != 0 || !cJSON_IsNumber(version) || version->valuedouble != 1) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "the request must have \"schema\": \"%s\" and \"schema_version\": 1",
                        SCHEMA_NAME);
    }
    ComposerStatus status = read_brief(request, root, base, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    snprintf(path, sizeof path, "%s/sections", base);
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(root, "sections");
    if (!cJSON_IsArray(sections) || cJSON_GetArraySize(sections) < 1) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "the request needs \"sections\": a list with a body");
    }
    /* One more than the most, room for the default ending. */
    if (cJSON_GetArraySize(sections) > PIECE_STRUCTURE_MAX_SECTIONS) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "a piece has at most %d sections", PIECE_STRUCTURE_MAX_SECTIONS);
    }
    PieceStructure *structure = &request->structure;
    structure->sections = request->sections;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, sections) {
        char item_path[160];
        snprintf(item_path, sizeof item_path, "%s/%zu", path, structure->section_count);
        status = read_section(request, structure->section_count, item, item_path, &structure->meter, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        structure->section_count++;
    }
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(root, "motive");
    if (motive && !cJSON_IsNull(motive)) {
        if (!cJSON_IsObject(motive)) {
            snprintf(path, sizeof path, "%s/motive", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"motive\" in the request must be a motive");
        }
        request->piece_motive_json = motive;
    }
    status = load_style(request, root, base, plain_midi, error);
    if (status == COMPOSER_OK) {
        status = load_motives(request, base, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    request->basic = structure->motive;
    for (size_t i = 0; !request->basic && i < structure->section_count; i++) {
        request->basic = request->sections[i].motive;
    }
    /* The structure checks before the default ending joins it: a request without a body is a bad one. */
    if (piece_structure_check(structure) != PIECE_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "the sections are not a piece: they need a body, in the order intro, body, turnaround, ending, with "
                        "different ids, whole beats and a pickup shorter than a bar");
    }
    size_t given = structure->section_count;
    if (piece_structure_default_ending(structure, PIECE_STRUCTURE_MAX_SECTIONS + 1) && structure->section_count > given) {
        /* The default ending is made from the basic motive, like an empty section. */
        snprintf(request->source[given], sizeof(request->source[0]), "basic motive");
    }
    return COMPOSER_OK;
}

ComposerStatus api_piece_validate(const cJSON *root, cJSON *filled, ComposerError *error) {
    PieceSpec *request = calloc(1, sizeof(*request));
    if (!request) {
        return fail_alloc(error);
    }
    ComposerStatus status = api_piece_read_request(request, root, "", NULL, false, NULL, error);
    cJSON *sections = cJSON_GetObjectItemCaseSensitive(filled, "sections");
    size_t given = (size_t)cJSON_GetArraySize(sections);
    if (status == COMPOSER_OK) {
        piece_structure_default_cadences(&request->structure);
    }
    for (size_t i = 0; status == COMPOSER_OK && i < request->structure.section_count; i++) {
        const PieceSection *section = &request->structure.sections[i];
        cJSON *item = i < given ? cJSON_GetArrayItem(sections, (int)i) : NULL;
        if (!item) {
            item = cJSON_CreateObject();
            if (!item || !cJSON_AddStringToObject(item, "id", section->id) ||
                !cJSON_AddStringToObject(item, "part", piece_part_name(section->part)) ||
                !cJSON_AddStringToObject(item, "letter", section->letter) ||
                !cJSON_AddNumberToObject(item, "beats", (double)section->beats) || !cJSON_AddItemToArray(sections, item)) {
                cJSON_Delete(item);
                status = fail_alloc(error);
                break;
            }
            cJSON *fill = cJSON_AddObjectToObject(item, "fill");
            if (!fill || !cJSON_AddStringToObject(fill, "kind", piece_fill_name(section->fill))) {
                status = fail_alloc(error);
                break;
            }
        }
        const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(item, "cadence");
        if (!cadence || cJSON_IsNull(cadence)) {
            cJSON_DeleteItemFromObjectCaseSensitive(item, "cadence");
            if (!cJSON_AddStringToObject(item, "cadence", plan_cadence_name(section->cadence))) {
                status = fail_alloc(error);
            }
        }
    }
    api_piece_request_destroy(request);
    free(request);
    return status;
}
