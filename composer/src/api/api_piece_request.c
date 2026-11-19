#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <plan/section_json.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_json.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_vocabulary.h"
#include "api_piece.h"
#include "util/util.h"
#include "api_workflow.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include <ursatz_composer/piece_plan_keys.h>

/*
 * The piece request (docs/piece-request.md, schema "ursatz-piece-request" version 1) as the library reads it: the brief, the
 * sections with their fills, the motives, the saved forms, the style and the tempo. A theme fill carries its plan as an
 * object (the caller reads the file); the other members are the request file's.
 */

#define SCHEMA_NAME "ursatz-piece-request"

void api_piece_request_destroy(PieceSpec *request) {
    for (size_t i = 0; i <= PIECE_STRUCTURE_MAX_SECTIONS; i++) {
        free(request->section_motive[i]);
        plan_destroy(request->theme_plan[i]);
    }
    free(request->piece_motive);
    free(request->pool);
    free(request->pool_motives);
    composer_style_destroy(request->style);
    form_spec_destroy(request->form);
    request->form = NULL;
}

/* "antecedent+continuation": a beginning, a plus sign and an ending (as grow --hybrid), the words of the vocabulary. */
static bool parse_hybrid(const char *text, SectionPairing *pairing) {
    const char *plus = strchr(text, '+');
    if (!plus) {
        return false;
    }
    size_t length = (size_t)(plus - text);
    bool beginning_found = false;
    bool ending_found = false;
    for (int i = 0; API_THEME_BEGINNINGS[i].value; i++) {
        if (strlen(API_THEME_BEGINNINGS[i].value) == length && strncmp(text, API_THEME_BEGINNINGS[i].value, length) == 0) {
            pairing->beginning = (SectionBeginning)i;
            beginning_found = true;
        }
    }
    for (int i = 0; API_THEME_ENDINGS[i].value; i++) {
        if (request_equals_ignore_case(plus + 1, API_THEME_ENDINGS[i].value)) {
            pairing->ending = (SectionEnding)i;
            ending_found = true;
        }
    }
    return beginning_found && ending_found;
}

static ComposerStatus read_fill(PieceSpec *request, size_t index, const cJSON *fill, const char *base, ComposerError *error) {
    PieceSection *section = &request->sections[index];
    const char *id = section->description.id;
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
    /* The old progression fill is the section's harmony, read as `harmony.progression` with an empty fill (R-610). */
    const bool legacy_progression = strcmp(kind, "progression") == 0;
    if (legacy_progression) {
        section->fill = PIECE_FILL_EMPTY;
    } else if (!piece_fill_from_name(kind, &section->fill)) {
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
                char intos[64];
                api_choice_list(API_PIECE_INTOS, intos, sizeof intos);
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"into\" must be %s, not '%s'", id, intos,
                                into);
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
            bool sentence = section->hybrid.beginning == SECTION_BEGINNING_PRESENTATION &&
                            section->hybrid.ending == SECTION_ENDING_CONTINUATION;
            bool period = section->hybrid.beginning == SECTION_BEGINNING_ANTECEDENT &&
                          section->hybrid.ending == SECTION_ENDING_CONSEQUENT;
            if (sentence || period) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': %s is a %s, not a hybrid", id, hybrid,
                                sentence ? "sentence" : "period");
            }
        } else if (hybrid) {
            snprintf(path, sizeof path, "%s/hybrid", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"hybrid\" applies only to \"into\": \"hybrid\"", id);
        }
        /* A construction makes the section a theme of its whole bars, 3 to 16 (R-468): "auto", or the bars of the ideas. */
        const cJSON *construction = cJSON_GetObjectItemCaseSensitive(fill, "construction");
        if (construction && !cJSON_IsNull(construction)) {
            snprintf(path, sizeof path, "%s/construction", base);
            if (cJSON_IsString(construction) && construction->valuestring && strcmp(construction->valuestring, "auto") == 0) {
                section->construction_auto = true;
            } else if (cJSON_IsObject(construction)) {
                ComposerStatus read = api_grow_read_construction_object(construction, path, &section->construction, error);
                if (read != COMPOSER_OK) {
                    return read;
                }
            } else {
                return api_fail(error, COMPOSER_ERR_REQUEST, path,
                                "section '%s': \"construction\" must be \"auto\" or {\"ideas\": {...}, \"extensions\": [...]}", id);
            }
            section->has_construction = true;
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
        PlanResult read = text ? plan_from_json_resolved(request->knowledge, text, composer_plan_read_hooks(), &request->theme_plan[index], &detail) : PLAN_ERR_ALLOC;
        cJSON_free(text);
        if (read == PLAN_ERR_ALLOC) {
            return api_fail_alloc(error);
        }
        if (read != PLAN_OK) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': cannot read the theme plan '%s': %s", id,
                            label ? label : "plan", detail.message);
        }
        section->theme_plan = request->theme_plan[index];
        snprintf(request->source[index], sizeof(request->source[index]), "%s", label ? label : "theme plan");
    } else if (legacy_progression) {
        const char *numerals = NULL;
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        SectionHarmony *harmony = &section->description.harmony;
        if (harmony->kind != SECTION_HARMONY_NONE) {
            snprintf(path, sizeof path, "%s/harmony", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': give harmony or a progression fill, not both", id);
        }
        if (!api_request_string(fill, base, "numerals", true, &numerals, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        snprintf(path, sizeof path, "%s/numerals", base);
        if (strlen(numerals) == 0 || strlen(numerals) >= sizeof(harmony->numerals)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': a progression is 1 to %zu characters of numerals", id,
                            sizeof(harmony->numerals) - 1);
        }
        harmony->kind = SECTION_HARMONY_PROGRESSION;
        snprintf(harmony->numerals, sizeof(harmony->numerals), "%s", numerals);
        if (cells && !cJSON_IsNull(cells)) {
            if (!cJSON_IsNumber(cells) || cells->valuedouble < 1 || cells->valuedouble != (double)(size_t)cells->valuedouble) {
                snprintf(path, sizeof path, "%s/cells", base);
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"cells\" must be a whole number from 1", id);
            }
            harmony->cells = (unsigned)cells->valuedouble;
        }
        snprintf(request->source[index], sizeof(request->source[index]), "%s", numerals);
    }
    return COMPOSER_OK;
}

static ComposerStatus read_section(PieceSpec *request, size_t index, const cJSON *item, const char *base,
                                   const PlanMeter *meter, ComposerError *error) {
    PieceSection *section = &request->sections[index];
    char path[128];
    if (!cJSON_IsObject(item)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "every section of the request must be an object");
    }
    /* The section is the library's description for a request (R-605); `part` is the old name of `role`, and `fill` is read below. */
    const cJSON *part = cJSON_GetObjectItemCaseSensitive(item, "part");
    if (part && !cJSON_IsNull(part) && cJSON_GetObjectItemCaseSensitive(item, "role")) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "give role or part, not both");
    }
    cJSON *named = cJSON_Duplicate(item, true);
    if (!named) {
        return api_fail(error, COMPOSER_ERR_ALLOC, base, "out of memory");
    }
    /* A member given as null is a member not given, as it always was in this request. */
    for (cJSON *member = named->child; member;) {
        cJSON *next = member->next;
        if (cJSON_IsNull(member)) {
            cJSON_Delete(cJSON_DetachItemViaPointer(named, member));
        }
        member = next;
    }
    if (part && !cJSON_IsNull(part)) {
        cJSON *moved = cJSON_DetachItemFromObjectCaseSensitive(named, "part");
        if (!cJSON_AddItemToObject(named, "role", moved)) {
            cJSON_Delete(moved);
            cJSON_Delete(named);
            return api_fail(error, COMPOSER_ERR_ALLOC, base, "out of memory");
        }
    }
    static const char *const SKIPPED[] = {"fill"};
    SectionDescription description;
    SectionError refused;
    bool read = section_from_json(named, SECTION_USE_REQUEST, SKIPPED, 1, &description, &refused);
    cJSON_Delete(named);
    if (!read) {
        const char *member = part && strcmp(refused.path, "/role") == 0 ? "/part" : refused.path;
        snprintf(path, sizeof path, "%s%s", base, member);
        const cJSON *given_id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(given_id) && given_id->valuestring[0] != '\0') {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%.15s': %s", given_id->valuestring, refused.message);
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s", refused.message);
    }
    const char *id = description.id;
    piece_section_init(section);
    section->description = description;
    if (description.letter[0] == '\0') {
        snprintf(section->description.letter, sizeof(section->description.letter), "%c", toupper((unsigned char)id[0]));
    }
    if (description.bars != 0) {
        section->description.beats = description.bars * (size_t)meter->beats_per_bar;
    }
    if (section->description.beats < 1 || section->description.beats > API_PIECE_BEATS_MAX) {
        snprintf(path, sizeof path, "%s/beats", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': \"beats\" must be a whole number of beats, 1 or more", id);
    }
    /* The codec holds a pickup in beats; the request holds it in whole notes. */
    section->description.pickup = (Rational){0, 1};
    if (description.has_pickup &&
        (meter->beat_unit <= 0 || rational_mul(description.pickup, (Rational){1, meter->beat_unit}, &section->description.pickup) != RATIONAL_OK)) {
        snprintf(path, sizeof path, "%s/pickup", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': the pickup is not a number of beats; write 0, 1 or 1/2", id);
    }
    snprintf(path, sizeof path, "%s/fill", base);
    ComposerStatus filled = read_fill(request, index, cJSON_GetObjectItemCaseSensitive(item, "fill"), path, error);
    if (filled == COMPOSER_OK && section->fill == PIECE_FILL_THEME && section->description.harmony.kind != SECTION_HARMONY_NONE) {
        snprintf(path, sizeof path, "%s/harmony", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': a theme brings its harmony", id);
    }
    if (filled == COMPOSER_OK && section->has_construction) {
        size_t per_bar = (size_t)meter->beats_per_bar;
        if (section->description.pickup.num != 0 || per_bar == 0 || section->description.beats % per_bar != 0 || section->description.beats / per_bar < 3 ||
            section->description.beats / per_bar > 16) {
            snprintf(path, sizeof path, "%s/fill/construction", base);
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            "section '%s': a construction needs a theme of 3 to 16 whole bars and no pickup", id);
        }
    }
    return filled;
}

/* The brief: key, mode, meter, the style and its weight, the tempo, the accompaniment or voicing. */
static ComposerStatus read_brief(PieceSpec *request, const cJSON *root, const char *base, ComposerError *error) {
    const char *style_weight = NULL;
    const char *voicing = NULL;
    char path[128];
    if (!api_request_string(root, base, "key", true, &request->key_text, error) ||
        !api_read_material(root, base, true, &request->material_text, &request->material_member, error) ||
        !api_request_string(root, base, "meter", true, &request->meter_text, error) ||
        (!request->style_path && !api_request_string(root, base, "style", false, &request->style_path, error)) ||
        !api_request_string(root, base, "style_weight", false, &style_weight, error) ||
        !api_request_string(root, base, "voicing", false, &voicing, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    PieceRequest *piece = &request->request;
    const PitchMaterial *material = NULL;
    ApiMaterialKind kind;
    snprintf(path, sizeof path, "%s/%s", base, request->material_member);
    ComposerStatus resolved = api_resolve_material(request->knowledge, request->material_member, path, request->material_text,
                                                   API_MATERIAL_ALLOW_STYLE, &material, &kind, error);
    if (resolved != COMPOSER_OK) {
        return resolved;
    }
    if (kind == API_MATERIAL_STYLE && !request->style_path) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"%s\": \"style\" needs a \"style\"", request->material_member);
    }
    /* The key may be a tonic alone: the material is the one "material" gives. */
    char tonic_key[64];
    const char *key_text = request->key_text;
    if (strlen(key_text) + 7 < sizeof(tonic_key) && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof(tonic_key), "%s major", key_text);
        key_text = tonic_key;
    }
    if (!request_parse_key(key_text, request->knowledge, &piece->key)) {
        snprintf(path, sizeof path, "%s/key", base);
        if (api_fail_unsupported_key(error, path, request->knowledge, key_text)) {
            return COMPOSER_ERR_REQUEST;
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, for example \"D\"", request->key_text);
    }
    if (material) {
        composer_key_set_material(&piece->key, material);
    }
    if (!request_parse_meter(request->meter_text, &piece->meter) || !harmony_meter_supported(&piece->meter)) {
        snprintf(path, sizeof path, "%s/meter", base);
        return api_fail_meter(error, path, request->meter_text);
    }
    if (!form_spec_meter_allowed(request->form, &piece->meter)) {
        char listed[96] = "";
        size_t used = 0;
        for (size_t i = 0; i < request->form->meter_count && used < sizeof listed; i++) {
            used += (size_t)snprintf(listed + used, sizeof listed - used, i ? ", %d/%d" : "%d/%d",
                                     request->form->meters[i].beats_per_bar, request->form->meters[i].beat_unit);
        }
        snprintf(path, sizeof path, "%s/meter", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "the %s form is in %s", request->form->label, listed);
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
            api_read_accompaniment(pattern, NULL, request->meter_text, &piece->meter, &piece->accompaniment, error);
        if (status != COMPOSER_OK) {
            snprintf(error->path, sizeof error->path, "%s", path);
            return status;
        }
        piece->has_accompaniment = true;
    }
    if (voicing) {
        snprintf(path, sizeof path, "%s/voicing", base);
        if (piece->has_accompaniment) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            "\"voicing\" writes the inner voices itself; it cannot be combined with an \"accompaniment\"");
        }
        if (strcmp(voicing, "satb") != 0 && strcmp(voicing, "chorale") != 0) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"voicing\" in the request must be satb or chorale, not '%s'",
                            voicing);
        }
        piece->has_voicing = true;
        piece->voicing.write_soprano = strcmp(voicing, "chorale") == 0;
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
    } else if (request->form && request->form->tempo > 0) {
        request->tempo = (ComposerRatio){request->form->tempo, 1};
        request->export_options.tempo_numerator = request->tempo.numerator;
        request->export_options.tempo_denominator = request->tempo.denominator;
        request->tempo_from = "form";
    }
    request->mode_from = "request";
    if (request_equals_ignore_case(request->material_text, "style")) {
        const PitchMaterial *material = NULL;
        if (!composer_style_mode(request->style, request->materials, &material)) {
            snprintf(path, sizeof path, "%s/%s", base, request->material_member);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"%s\": \"style\" needs a style with a mode category",
                            request->material_member);
        }
        composer_key_set_material(&request->request.key, material);
        request->mode_from = "style";
    }
    if (request->style) {
        PlanStyle *plan_style = &request->request.style;
        memset(plan_style, 0, sizeof(*plan_style));
        snprintf(plan_style->path, sizeof(plan_style->path), "%s", request->style_path);
        plan_style->weight_numerator = request->weight.numerator;
        plan_style->weight_denominator = request->weight.denominator;
        for (size_t i = 0; i < request->without_count && i < PLAN_STYLE_WITHOUT_MAX; i++) {
            snprintf(plan_style->without[plan_style->without_count++], PLAN_STYLE_WITHOUT_NAME_MAX, "%s", request->without[i]);
        }
        request->request.has_style = true;
    }
    return COMPOSER_OK;
}

/* A melodic motive of the request. */
static ComposerStatus read_melodic(const KnowledgeSnapshot *knowledge, const char *what, const char *path, const cJSON *json, const PlanMeter *meter,
                                   const SpelledKey *key, Motive **out, ComposerError *error) {
    *out = calloc(1, sizeof(Motive));
    if (!*out) {
        return api_fail_alloc(error);
    }
    ComposerStatus status = api_read_degrees_motive(knowledge, what, path, json, NULL, meter, key, *out, error);
    if (status == COMPOSER_OK && (*out)->accompaniment) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s is of the accompaniment role; a piece takes a melodic motive", what);
    }
    return status;
}

/* The motives, and the saved forms the motives stage may use; the key and the meter are known now. */
static ComposerStatus load_motives(PieceSpec *request, const char *base, ComposerError *error) {
    PieceRequest *piece = &request->request;
    char what[96];
    char path[128];
    ComposerStatus status = COMPOSER_OK;
    if (request->piece_motive_json) {
        snprintf(path, sizeof path, "%s/motive", base);
        status = read_melodic(request->knowledge, "the motive of the piece", path, request->piece_motive_json, &piece->meter, &piece->key,
                              &request->piece_motive, error);
        piece->motive = request->piece_motive;
    }
    size_t forms = 0;
    const size_t read_count = piece->section_count > piece->overlay_count ? piece->section_count : piece->overlay_count;
    for (size_t i = 0; status == COMPOSER_OK && i < read_count; i++) {
        if (request->motive_json[i]) {
            snprintf(what, sizeof(what), "the motive of section '%s'", request->sections[i].description.id);
            snprintf(path, sizeof path, "%s/sections/%zu/fill/motive", base, i);
            status = read_melodic(request->knowledge, what, path, request->motive_json[i], &piece->meter, &piece->key,
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
        return api_fail_alloc(error);
    }
    for (size_t i = 0; i < read_count; i++) {
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
            status = api_read_degrees_motive(request->knowledge, what, path, form, NULL, &piece->meter, &piece->key,
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

/* A whole-number member of the piece request: the error path of api_member_whole starts with base. */
static bool read_whole(const cJSON *root, const char *base, const char *name, unsigned long maximum, unsigned long *out,
                       bool *present, ComposerError *error) {
    if (api_member_whole(root, name, 0, maximum, out, present, error)) {
        return true;
    }
    char path[sizeof error->path];
    snprintf(path, sizeof path, "%s%s", base, error->path);
    snprintf(error->path, sizeof error->path, "%s", path);
    return false;
}

/* A boolean member of the piece request: the error path of api_member_bool starts with base. */
static bool read_bool(const cJSON *root, const char *base, const char *name, bool *out, bool *present, ComposerError *error) {
    if (api_member_bool(root, name, out, present, error)) {
        return true;
    }
    char path[sizeof error->path];
    snprintf(path, sizeof path, "%s%s", base, error->path);
    snprintf(error->path, sizeof error->path, "%s", path);
    return false;
}

/* True when the request has the member and it is not null. */
static bool given_member(const cJSON *root, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    return item && !cJSON_IsNull(item);
}

/* The names of the built-in forms, joined by commas. */
static void builtin_form_names(char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < form_spec_builtin_count() && used < size; i++) {
        used += (size_t)snprintf(out + used, size - used, i ? ", %s" : "%s", form_spec_builtin_name(i));
    }
}

/* The form the request names: schema version 1 is the game loop, version 2 names a built-in form. */
static ComposerStatus read_form(PieceSpec *request, const cJSON *root, const char *base, bool named, ComposerError *error) {
    char path[128];
    const char *name = API_PIECE_FORM;
    snprintf(path, sizeof path, "%s/form", base);
    if (!named && given_member(root, "form")) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "schema_version 1 is the game loop; name a form with schema_version 2");
    }
    char names[256];
    builtin_form_names(names, sizeof names);
    if (named) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "form");
        if (!cJSON_IsString(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "the request needs \"form\": a built-in form (%s)", names);
        }
        name = item->valuestring;
    }
    FormSpecResult found = form_spec_builtin(name, &request->form);
    if (found == FORM_SPEC_ERR_ALLOC) {
        return api_fail_alloc(error);
    }
    if (found != FORM_SPEC_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "unknown form '%s'; the built-in forms are %s", name, names);
    }
    request->request.form = request->form->name;
    return COMPOSER_OK;
}

/*
 * The overlays of a sized form (R-610): `sections` holds objects that each name a section of the size and give it a region, a motif with
 * a motive of its own or a composed theme; the other sections are as the form has them. Each is read through the section codec for an
 * overlay, its fill as a game loop section's.
 */
static ComposerStatus read_overlays(PieceSpec *request, const cJSON *root, const char *base, const FormSpecSize *size, ComposerError *error) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "sections");
    PieceRequest *piece = &request->request;
    const FormSpec *form = request->form;
    char path[1100];
    if (!list || cJSON_IsNull(list)) {
        return COMPOSER_OK;
    }
    char sections_path[400];
    snprintf(sections_path, sizeof sections_path, "%.380s/sections", base);
    snprintf(path, sizeof path, "%s", sections_path);
    if (!cJSON_IsArray(list) || (size_t)cJSON_GetArraySize(list) > size->section_count) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "sections lists the sections of %s to give something of their own, at most %zu",
                        form->label, size->section_count);
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[1060];
        snprintf(at, sizeof at, "%s/%zu", sections_path, index);
        if (!cJSON_IsObject(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, at, "every section of the request must be an object");
        }
        cJSON *named = cJSON_Duplicate(item, true);
        if (!named) {
            return api_fail_alloc(error);
        }
        for (cJSON *member = named->child; member;) {
            cJSON *next = member->next;
            if (cJSON_IsNull(member)) {
                cJSON_Delete(cJSON_DetachItemViaPointer(named, member));
            }
            member = next;
        }
        static const char *const SKIPPED[] = {"fill"};
        PieceSection *section = &request->sections[index];
        piece_section_init(section);
        SectionError refused;
        bool read = section_from_json(named, SECTION_USE_OVERLAY, SKIPPED, 1, &section->description, &refused);
        cJSON_Delete(named);
        if (!read) {
            snprintf(path, sizeof path, "%.600s%s", at, refused.path);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s", refused.message);
        }
        char name[SECTION_NAME_MAX];
        snprintf(name, sizeof(name), "%s", section->description.name);
        snprintf(section->description.id, sizeof(section->description.id), "%.15s", name);
        size_t found = size->section_count;
        for (size_t i = 0; i < size->section_count; i++) {
            if (strcmp(size->sections[i].name, name) == 0) {
                found = i;
            }
        }
        snprintf(path, sizeof path, "%s/name", at);
        if (found == size->section_count) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "no section '%s' in the %s form at %zu bars", name, form->name, size->bars);
        }
        for (size_t earlier = 0; earlier < index; earlier++) {
            if (strcmp(request->sections[earlier].description.name, name) == 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s' is given twice", name);
            }
        }
        snprintf(path, sizeof path, "%s/fill", at);
        ComposerStatus filled = read_fill(request, index, cJSON_GetObjectItemCaseSensitive(item, "fill"), path, error);
        if (filled != COMPOSER_OK) {
            return filled;
        }
        /* What a section of a sized form takes: a motif with a motive, or a theme; its shape and what it returns are the form's. */
        const SectionDescription *spec = &size->sections[found];
        bool fills = section->fill != PIECE_FILL_EMPTY;
        snprintf(path, sizeof path, "%s/fill", at);
        if (section->has_into || section->has_construction || request->forms_json[index]) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': the %s form gives it its shape; give a motive or a theme", name,
                            form->name);
        }
        bool harmony = section->description.harmony.kind != SECTION_HARMONY_NONE;
        if (harmony && section->fill == PIECE_FILL_THEME) {
            snprintf(path, sizeof path, "%s/harmony", at);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': a theme brings its harmony", name);
        }
        if ((fills || harmony) && spec->returns[0] != '\0') {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "'%s' returns '%s': give it to '%s'", name, spec->returns, spec->returns);
        }
        if ((fills || harmony) && (spec->role == SECTION_ROLE_LIQUIDATION || spec->role == SECTION_ROLE_RETRANSITION || spec->role == SECTION_ROLE_CODA)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': a %s is grown by its operator", name, section_role_name(spec->role));
        }
        index++;
    }
    piece->section_count = 0;
    piece->overlays = request->sections;
    piece->overlay_count = index;
    return COMPOSER_OK;
}

/* The regions the overlays ask for, once the key is read: refused at the overlay's region. */
static ComposerStatus check_overlay_regions(PieceSpec *request, const char *base, ComposerError *error) {
    PieceRequest *piece = &request->request;
    const FormSpecSize *size = form_spec_size(request->form, piece->bars, piece->count);
    if (!size || piece->overlay_count == 0) {
        return COMPOSER_OK;
    }
    PieceRegion regions[FORM_SPEC_MAX_SECTIONS];
    bool regional = false;
    size_t refused_section = 0;
    char why[256];
    if (piece_resolve_overlay_regions(piece, size, request->knowledge, regions, &regional, &refused_section, why, sizeof why) == PIECE_OK) {
        return COMPOSER_OK;
    }
    size_t overlay = 0;
    for (size_t o = 0; o < piece->overlay_count; o++) {
        if (strcmp(piece->overlays[o].description.name, size->sections[refused_section].name) == 0) {
            overlay = o;
        }
    }
    char path[200];
    snprintf(path, sizeof path, "%.150s/sections/%zu/region", base, overlay);
    return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s", why);
}

/* The harmony each section gives can be given to it (R-610), tried on a scratch plan of its length, key and meter. */
static ComposerStatus check_harmony(PieceSpec *request, const char *base, ComposerError *error) {
    PieceRequest *piece = &request->request;
    const FormSpecSize *size = request->form->request_sections ? NULL : form_spec_size(request->form, piece->bars, piece->count);
    const PieceSection *items = request->form->request_sections ? piece->sections : piece->overlays;
    size_t count = request->form->request_sections ? piece->section_count : piece->overlay_count;
    PieceRegion regions[PIECE_STRUCTURE_MAX_SECTIONS];
    memset(regions, 0, sizeof(regions));
    size_t refused;
    char why[256];
    bool regional = false;
    if (request->form->request_sections) {
        (void)piece_resolve_regions(piece, request->knowledge, regions, &refused, why, sizeof why);
    } else if (size) {
        (void)piece_resolve_overlay_regions(piece, size, request->knowledge, regions, &regional, &refused, why, sizeof why);
    }
    Rational bar = {0, 1};
    if (rational_make(piece->meter.beats_per_bar, piece->meter.beat_unit, &bar) != RATIONAL_OK) {
        return COMPOSER_OK;
    }
    for (size_t i = 0; i < count; i++) {
        const SectionHarmony *harmony = &items[i].description.harmony;
        if (harmony->kind == SECTION_HARMONY_NONE) {
            continue;
        }
        Rational length = {0, 1};
        SpelledKey key = piece->key;
        char path[200];
        if (request->form->request_sections) {
            if (rational_make((long long)items[i].description.beats, piece->meter.beat_unit, &length) != RATIONAL_OK) {
                continue;
            }
            key = regions[i].key.material ? regions[i].key : piece->key;
        } else {
            size_t found = size ? size->section_count : 0;
            for (size_t j = 0; size && j < size->section_count; j++) {
                if (strcmp(size->sections[j].name, items[i].description.name) == 0) {
                    found = j;
                }
            }
            if (found == size->section_count || rational_mul_int(bar, (long long)size->sections[found].bars, &length) != RATIONAL_OK) {
                continue;
            }
            key = regional && regions[found].has_region ? regions[found].key : piece->key;
        }
        if (!piece_check_harmony(&piece->meter, &key, length, harmony, why, sizeof why)) {
            snprintf(path, sizeof path, "%.150s/sections/%zu/harmony", base, i);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "section '%s': %s", items[i].description.id, why);
        }
    }
    return COMPOSER_OK;
}

/*
 * The members of a form that takes its sections from its size: bars, count, modulate and the overlays in sections; voicing is refused.
 * A size the form does not have is refused with the ones it has.
 */
static ComposerStatus read_sized(PieceSpec *request, const cJSON *root, const char *base, ComposerError *error) {
    char path[128];
    PieceRequest *piece = &request->request;
    const FormSpec *form = request->form;
    unsigned long bars = 0;
    unsigned long count = 0;
    bool has_bars = false;
    bool has_count = false;
    bool modulate = false;
    bool present = false;
    if (given_member(root, "voicing")) {
        snprintf(path, sizeof path, "%s/voicing", base);
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s has no voicing to give; leave out voicing", form->label);
    }
    if (!read_whole(root, base, "bars", 512, &bars, &has_bars, error) || !read_whole(root, base, "count", 64, &count, &has_count, error) ||
        !read_bool(root, base, "modulate", &modulate, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (!form_spec_size(form, (size_t)bars, (size_t)count)) {
        char sizes[160];
        char asked[64] = "";
        api_form_sizes_text(form, sizes, sizeof sizes);
        if (has_bars) {
            snprintf(asked, sizeof asked, "%lu bars%s", bars, has_count ? " with " : "");
        }
        if (has_count) {
            size_t used = strlen(asked);
            snprintf(asked + used, sizeof asked - used, "count %lu", count);
        }
        snprintf(path, sizeof path, "%s/%s", base, has_bars ? "bars" : "count");
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s has no size of %s; it has %s", form->label, asked, sizes);
    }
    piece->bars = (size_t)bars;
    piece->count = (size_t)count;
    piece->modulate = modulate;
    return read_overlays(request, root, base, form_spec_size(form, (size_t)bars, (size_t)count), error);
}

ComposerStatus api_piece_read_request(PieceSpec *request, const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials,
                                      const cJSON *root, const char *base, const char *style_path, bool plain_midi,
                                      const ComposerJob *job, ComposerError *error) {
    char path[128];
    memset(request, 0, sizeof(*request));
    request->job = job;
    request->knowledge = knowledge;
    request->materials = materials;
    request->request.knowledge = knowledge;
    request->style_path = style_path;
    if (!cJSON_IsObject(root)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base, "the piece request must be a JSON object");
    }
    const char *schema = NULL;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!api_request_string(root, base, "schema", true, &schema, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (strcmp(schema, SCHEMA_NAME) != 0 || !cJSON_IsNumber(version) || (version->valuedouble != 1 && version->valuedouble != 2)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, base,
                        "the request must have \"schema\": \"%s\" and \"schema_version\": 1 or 2", SCHEMA_NAME);
    }
    ComposerStatus status = read_form(request, root, base, version->valuedouble == 2, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    PieceRequest *piece = &request->request;
    piece->sections = request->sections;
    bool given_sections = request->form->request_sections;
    if (!given_sections) {
        status = read_sized(request, root, base, error);
    } else {
        static const char *const SIZED[] = {"bars", "count", "modulate"};
        for (size_t i = 0; i < sizeof(SIZED) / sizeof(SIZED[0]); i++) {
            if (given_member(root, SIZED[i])) {
                snprintf(path, sizeof path, "%s/%s", base, SIZED[i]);
                return api_fail(error, COMPOSER_ERR_REQUEST, path,
                                "%s takes its sections from the request and has no %s; leave out %s", request->form->label,
                                SIZED[i], SIZED[i]);
            }
        }
    }
    if (status == COMPOSER_OK) {
        status = read_brief(request, root, base, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    snprintf(path, sizeof path, "%s/sections", base);
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(root, "sections");
    if (given_sections && (!cJSON_IsArray(sections) || cJSON_GetArraySize(sections) < 1)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "the request needs \"sections\": a list with a body");
    }
    /* One more than the most, room for the default ending. */
    if (given_sections && cJSON_GetArraySize(sections) > PIECE_STRUCTURE_MAX_SECTIONS) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "a piece has at most %d sections", PIECE_STRUCTURE_MAX_SECTIONS);
    }
    const cJSON *item = NULL;
    const cJSON *section_list = given_sections ? sections : NULL;
    cJSON_ArrayForEach(item, section_list) {
        char item_path[160];
        snprintf(item_path, sizeof item_path, "%s/%zu", path, piece->section_count);
        status = read_section(request, piece->section_count, item, item_path, &piece->meter, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        piece->section_count++;
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
    request->basic = piece->motive;
    for (size_t i = 0; !request->basic && i < piece->section_count; i++) {
        request->basic = request->sections[i].motive;
    }
    if (!given_sections) {
        status = check_overlay_regions(request, base, error);
        return status == COMPOSER_OK ? check_harmony(request, base, error) : status;
    }
    /* The request checks before the default ending joins it: a request without a body is a bad one. */
    if (piece_request_check(piece) != PIECE_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "the sections are not a piece: they need a body, in the order intro, body, turnaround, ending, with "
                        "different ids, whole beats and a pickup shorter than a bar");
    }
    bool asks_for_regions = false;
    for (size_t i = 0; i < piece->section_count; i++) {
        asks_for_regions = asks_for_regions || (piece->sections[i].description.key_end.kind != SECTION_REGION_NONE &&
                                                piece->sections[i].description.key_end.kind != SECTION_REGION_HOME);
    }
    if (asks_for_regions) {
        PieceRegion regions[PIECE_STRUCTURE_MAX_SECTIONS];
        size_t refused = 0;
        char why[256];
        if (piece_resolve_regions(piece, knowledge, regions, &refused, why, sizeof(why)) != PIECE_OK) {
            char at[160];
            snprintf(at, sizeof(at), "%s/sections/%zu/region", base, refused);
            return api_fail(error, COMPOSER_ERR_REQUEST, at, "%s", why);
        }
    }
    status = check_harmony(request, base, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    size_t given = piece->section_count;
    if (piece_default_ending(request->form, piece, PIECE_STRUCTURE_MAX_SECTIONS + 1) && piece->section_count > given) {
        /* The default ending is made from the basic motive, like an empty section. */
        snprintf(request->source[given], sizeof(request->source[0]), "basic motive");
    }
    return COMPOSER_OK;
}

ComposerStatus api_piece_validate(const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials, const cJSON *root,
                                  cJSON *filled, ComposerError *error) {
    PieceSpec *request = calloc(1, sizeof(*request));
    if (!request) {
        return api_fail_alloc(error);
    }
    ComposerStatus status = api_piece_read_request(request, knowledge, materials, root, "", NULL, false, NULL, error);
    cJSON *sections = cJSON_GetObjectItemCaseSensitive(filled, "sections");
    size_t given = (size_t)cJSON_GetArraySize(sections);
    if (status == COMPOSER_OK && request->form->request_sections) {
        piece_default_cadences(request->form, &request->request);
    } else if (status == COMPOSER_OK) {
        /* A sized form has no sections to fill in: the size the request chose, as bars and count. */
        const FormSpecSize *size = form_spec_size(request->form, request->request.bars, request->request.count);
        const char *const names[] = {"bars", "count"};
        const size_t values[] = {size ? size->bars : 0, size ? size->count : 0};
        for (size_t i = 0; status == COMPOSER_OK && i < 2; i++) {
            const cJSON *item = cJSON_GetObjectItemCaseSensitive(filled, names[i]);
            if (item && !cJSON_IsNull(item)) {
                continue;
            }
            cJSON_DeleteItemFromObjectCaseSensitive(filled, names[i]);
            if (!cJSON_AddNumberToObject(filled, names[i], (double)values[i])) {
                status = api_fail_alloc(error);
            }
        }
    }
    for (size_t i = 0; status == COMPOSER_OK && i < request->request.section_count; i++) {
        const PieceSection *section = &request->request.sections[i];
        cJSON *item = i < given ? cJSON_GetArrayItem(sections, (int)i) : NULL;
        if (!item) {
            item = cJSON_CreateObject();
            if (!item || !cJSON_AddStringToObject(item, "id", section->description.id) ||
                !cJSON_AddStringToObject(item, "part", piece_part_name(piece_section_part(section))) ||
                !cJSON_AddStringToObject(item, "letter", section->description.letter) ||
                !cJSON_AddNumberToObject(item, "beats", (double)section->description.beats) || !cJSON_AddItemToArray(sections, item)) {
                cJSON_Delete(item);
                status = api_fail_alloc(error);
                break;
            }
            cJSON *fill = cJSON_AddObjectToObject(item, "fill");
            if (!fill || !cJSON_AddStringToObject(fill, "kind", piece_fill_name(section->fill))) {
                status = api_fail_alloc(error);
                break;
            }
        }
        const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(item, "cadence");
        if (!cadence || cJSON_IsNull(cadence)) {
            cJSON_DeleteItemFromObjectCaseSensitive(item, "cadence");
            if (!cJSON_AddStringToObject(item, "cadence", plan_cadence_name(section->description.cadence))) {
                status = api_fail_alloc(error);
            }
        }
    }
    api_piece_request_destroy(request);
    free(request);
    return status;
}
