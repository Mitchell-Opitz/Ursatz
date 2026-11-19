#include "api_grow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/vocabulary/texture_rules.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/theme_shape.h>

#include "api_internal.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"

#include "api_construction.h"

const char *api_accompaniment_name(PlanAccompanimentPattern pattern) {
    return API_ACCOMPANIMENT_PATTERNS[pattern].value;
}

ComposerStatus api_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text,
                                      const PlanMeter *meter, PlanAccompaniment *out, ComposerError *error) {
    out->pattern = PLAN_ACCOMPANIMENT_ANY;
    out->texture = -1;
    if (pattern_text) {
        bool found = false;
        for (int p = 0; p < 5 && !found; p++) {
            found = request_equals_ignore_case(pattern_text, API_ACCOMPANIMENT_PATTERNS[p].value);
            out->pattern = found ? (PlanAccompanimentPattern)p : out->pattern;
        }
        if (!found) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment",
                            "accompaniment must be block, arpeggiated, alberti, oom-pah or any, not '%s'", pattern_text);
        }
    }
    if (texture_text) {
        bool found = false;
        for (int t = 0; t < (int)STANDARD_TEXTURE_COUNT && !found; t++) {
            found = request_equals_ignore_case(texture_text, standard_texture_name((StandardTexture)t));
            out->texture = found ? t : out->texture;
        }
        if (!found) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/texture",
                            "texture must be a texture name of standard-texture, for example \"melody and accompaniment\", "
                            "not '%s'",
                            texture_text);
        }
        if (!standard_texture_is_homophonic((StandardTexture)out->texture)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/texture",
                            "texture '%s' is not one an accompaniment can have; use homophony, homorhythm or \"melody and "
                            "accompaniment\"",
                            texture_text);
        }
    }
    if (out->pattern == PLAN_ACCOMPANIMENT_ALBERTI && meter->beat_unit == 8) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment", "accompaniment alberti needs a simple meter, not %s",
                        meter_text);
    }
    return COMPOSER_OK;
}

/* "antecedent+continuation": a beginning, a plus sign and an ending. */
const char *const API_BEGINNING_NAMES[3] = {"presentation", "antecedent", "compound basic idea"};
const char *const API_ENDING_NAMES[3] = {"continuation", "consequent", "cadential"};
const char *const API_EXTENSION_KINDS[4] = {"repeat", "stretch", "addition", "evaded"};

bool api_parse_hybrid(const char *text, SectionPairing *pairing) {
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

static ComposerStatus read_extension(const cJSON *item, const char *base, size_t index, SectionExtension *out, ComposerError *error) {
    char path[192];
    snprintf(path, sizeof path, "%s/extensions/%zu", base, index);
    if (!cJSON_IsObject(item)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "each extension must be {\"kind\", \"idea\", \"bars\", \"varied\"}");
    }
    memset(out, 0, sizeof(*out));
    const char *kind;
    const char *idea = NULL;
    bool found;
    if (!api_request_string(item, path, "kind", true, &kind, error) || !api_request_string(item, path, "idea", false, &idea, error) ||
        !api_member_bool(item, "varied", &out->varied, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    int kind_at = -1;
    for (int k = 0; k < 4; k++) {
        if (strcmp(kind, API_EXTENSION_KINDS[k]) == 0) {
            kind_at = k;
        }
    }
    char member[224];
    if (kind_at < 0) {
        snprintf(member, sizeof member, "%s/kind", path);
        return api_fail(error, COMPOSER_ERR_REQUEST, member, "kind must be repeat, stretch, addition or evaded, not '%s'", kind);
    }
    out->kind = (SectionExtensionKind)kind_at;
    if (idea) {
        const ThemeShapeIdea *row = theme_shape_idea_find(idea);
        if (!row) {
            snprintf(member, sizeof member, "%s/idea", path);
            return api_fail(error, COMPOSER_ERR_REQUEST, member, "unknown idea '%s'", idea);
        }
        out->idea = (unsigned char)row->idea;
    } else if (out->kind == SECTION_EXTENSION_REPEAT || out->kind == SECTION_EXTENSION_STRETCH) {
        snprintf(member, sizeof member, "%s/idea", path);
        return api_fail(error, COMPOSER_ERR_REQUEST, member, "a %s needs the idea it follows", kind);
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(item, "bars");
    if (bars && !cJSON_IsNull(bars)) {
        if (!cJSON_IsNumber(bars) || bars->valuedouble != (double)(int)bars->valuedouble || bars->valuedouble < 0 ||
            bars->valuedouble > 4) {
            snprintf(member, sizeof member, "%s/bars", path);
            return api_fail(error, COMPOSER_ERR_REQUEST, member, "an extension's bars must be a whole number from 0 to 4");
        }
        out->bars = (unsigned char)bars->valuedouble;
    }
    return COMPOSER_OK;
}

ComposerStatus api_grow_read_construction_object(const cJSON *item, const char *base, SectionConstruction *construction,
                                                 ComposerError *error) {
    char ideas_path[192];
    snprintf(ideas_path, sizeof ideas_path, "%s/ideas", base);
    memset(construction, 0, sizeof(*construction));
    construction->reason = SECTION_CONSTRUCTION_GIVEN;
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(item, "ideas");
    if (!cJSON_IsObject(ideas)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, ideas_path,
                        "\"ideas\" must be an object of idea names and bars, for example {\"basic idea\": 2}");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        char path[320];
        snprintf(path, sizeof path, "%s/%.80s", ideas_path, member->string);
        const ThemeShapeIdea *row = theme_shape_idea_find(member->string);
        if (!row) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "unknown idea '%.60s'", member->string);
        }
        if (!cJSON_IsNumber(member) || member->valuedouble != (double)(int)member->valuedouble || member->valuedouble < 0 ||
            member->valuedouble > 16) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "the bars of an idea must be a whole number from 0 to 16");
        }
        construction->idea_bars[row->idea] = (unsigned char)member->valuedouble;
    }
    const cJSON *extensions = cJSON_GetObjectItemCaseSensitive(item, "extensions");
    if (extensions && !cJSON_IsNull(extensions)) {
        char extensions_path[192];
        snprintf(extensions_path, sizeof extensions_path, "%s/extensions", base);
        if (!cJSON_IsArray(extensions)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, extensions_path, "\"extensions\" must be a list");
        }
        if ((size_t)cJSON_GetArraySize(extensions) > SECTION_CONSTRUCTION_EXTENSIONS) {
            return api_fail(error, COMPOSER_ERR_REQUEST, extensions_path, "a construction has at most %d extensions",
                            SECTION_CONSTRUCTION_EXTENSIONS);
        }
        const cJSON *extension = NULL;
        cJSON_ArrayForEach(extension, extensions) {
            ComposerStatus read = read_extension(extension, base, construction->extension_count,
                                                 &construction->extensions[construction->extension_count], error);
            if (read != COMPOSER_OK) {
                return read;
            }
            construction->extension_count++;
        }
    }
    return COMPOSER_OK;
}

ComposerStatus api_grow_read_construction(const cJSON *request, GrowSetup *out, ComposerError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, "construction");
    out->has_construction = false;
    if (!item || cJSON_IsNull(item)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsObject(item)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/construction",
                        "\"construction\" must be null or {\"ideas\": {...}, \"extensions\": [...]}");
    }
    SectionConstruction *construction = &out->construction;
    ComposerStatus read = api_grow_read_construction_object(item, "/construction", construction, error);
    if (read != COMPOSER_OK) {
        return read;
    }
    out->has_construction = true;
    /* Without bars the theme is as long as its construction. */
    if (!out->text->bars) {
        size_t total = grow_construction_extra_bars(&out->pairing, construction);
        for (int idea = 0; idea < SECTION_CONSTRUCTION_IDEAS; idea++) {
            total += construction->idea_bars[idea];
        }
        if (total >= 3 && total <= 16) {
            out->bars = total;
        }
    }
    return COMPOSER_OK;
}

ComposerStatus api_grow_read_brief(const GrowBriefText *text, GrowSetup *out, ComposerError *error) {
    out->text = text;
    if (!text->into || !text->key || !text->meter) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request needs into, key and meter");
    }
    if (request_equals_ignore_case(text->into, "sentence")) {
        out->into = GROW_INTO_SENTENCE;
        out->pairing.beginning = SECTION_BEGINNING_PRESENTATION;
        out->pairing.ending = SECTION_ENDING_CONTINUATION;
    } else if (request_equals_ignore_case(text->into, "period")) {
        out->into = GROW_INTO_PERIOD;
        out->pairing.beginning = SECTION_BEGINNING_ANTECEDENT;
        out->pairing.ending = SECTION_ENDING_CONSEQUENT;
    } else if (request_equals_ignore_case(text->into, "hybrid")) {
        out->into = GROW_INTO_HYBRID;
    } else {
        char intos[64];
        api_choice_list(API_PIECE_INTOS, intos, sizeof intos);
        return api_fail(error, COMPOSER_ERR_REQUEST, "/into", "into must be %s, not '%s'", intos, text->into);
    }
    if (out->into == GROW_INTO_HYBRID) {
        if (!text->hybrid) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid",
                            "into hybrid needs hybrid <beginning>+<ending>, for example antecedent+continuation");
        }
        if (!api_parse_hybrid(text->hybrid, &out->pairing)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid",
                            "hybrid must be a beginning (presentation, antecedent or cbi), a plus sign and an ending "
                            "(continuation, consequent or cadential), not '%s'",
                            text->hybrid);
        }
        bool sentence = out->pairing.beginning == SECTION_BEGINNING_PRESENTATION &&
                        out->pairing.ending == SECTION_ENDING_CONTINUATION;
        bool period = out->pairing.beginning == SECTION_BEGINNING_ANTECEDENT &&
                      out->pairing.ending == SECTION_ENDING_CONSEQUENT;
        if (sentence || period) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid", "%s is a %s, not a hybrid; use into %s", text->hybrid,
                            sentence ? "sentence" : "period", sentence ? "sentence" : "period");
        }
    } else if (text->hybrid) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid", "hybrid applies only to into hybrid");
    }
    const PitchMaterial *picked = NULL;
    char member_path[32];
    if (text->material) {
        ApiMaterialKind kind;
        snprintf(member_path, sizeof member_path, "/%s", text->material_member);
        ComposerStatus resolved = api_resolve_material(out->knowledge, text->material_member, member_path, text->material,
                                                       API_MATERIAL_ALLOW_STYLE, &picked, &kind, error);
        if (resolved != COMPOSER_OK) {
            return resolved;
        }
        if (kind == API_MATERIAL_STYLE && !text->has_style) {
            return api_fail(error, COMPOSER_ERR_REQUEST, member_path, "%s style needs style", text->material_member);
        }
    }
    /* With a material the key may be a tonic alone: the material is the one the member gives. */
    char tonic_key[64];
    const char *key_text = text->key;
    if (text->material && strlen(key_text) + 7 < sizeof tonic_key && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof tonic_key, "%s major", key_text);
        key_text = tonic_key;
    }
    if (!request_parse_key(key_text, out->knowledge, &out->key)) {
        if (api_fail_unsupported_key(error, "/key", out->knowledge, key_text)) {
            return COMPOSER_ERR_REQUEST;
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, for "
                        "example \"D minor\"",
                        text->key);
    }
    if (picked) {
        composer_key_set_material(&out->key, picked);
    }
    if (!request_parse_meter(text->meter, &out->meter) || !harmony_meter_supported(&out->meter)) {
        return api_fail_meter(error, "/meter", text->meter);
    }
    out->bars = 8;
    if (text->bars) {
        unsigned long bars;
        if (!request_parse_whole_number(text->bars, 16, &bars) || bars < 3) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/bars", "bars must be 3 to 16, not '%s'", text->bars);
        }
        out->bars = bars;
    }
    out->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    if (text->cadence &&
        (!request_parse_cadence(text->cadence, &out->cadence) || !api_theme_cadence_ok(out->cadence))) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/cadence", "cadence must be PAC, IAC or HC, not '%s'", text->cadence);
    }
    out->development = GROW_DEVELOP_BALANCED;
    if (text->development) {
        if (strcmp(text->development, "melody") == 0) {
            out->development = GROW_DEVELOP_MELODY;
        } else if (strcmp(text->development, "theme") == 0) {
            out->development = GROW_DEVELOP_THEME;
        } else if (strcmp(text->development, "balanced") != 0) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/development", "development must be melody, balanced or theme, not '%s'",
                            text->development);
        }
    }
    out->first_cadence = CADENCE_TYPE_HALF;
    if (text->first_cadence) {
        if (out->pairing.beginning != SECTION_BEGINNING_ANTECEDENT) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/first_cadence",
                            "first_cadence applies only to a period or a hybrid that begins with an antecedent");
        }
        if (!request_parse_cadence(text->first_cadence, &out->first_cadence) ||
            (out->first_cadence != CADENCE_TYPE_HALF && out->first_cadence != CADENCE_TYPE_IMPERFECT_AUTHENTIC)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/first_cadence", "first_cadence must be HC or IAC, not '%s'",
                            text->first_cadence);
        }
    }
    /* A chorale is four parts with the melody written too: a voicing, not an accompaniment. */
    bool chorale = text->texture && request_equals_ignore_case(text->texture, "chorale");
    if (chorale || text->voicing) {
        if (text->accompaniment || (text->texture && !chorale)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/voicing",
                            "voicing and texture chorale write the inner voices themselves; they cannot be combined with "
                            "accompaniment or another texture");
        }
        if (chorale && text->voicing) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/voicing",
                            "texture chorale writes the melody, voicing keeps the grown one; use one");
        }
        out->has_voicing = true;
        out->voicing.write_soprano = chorale;
    } else if (text->accompaniment || text->texture || text->accompaniment_motive) {
        /* The word "motive" names the accompaniment motive's pattern, which api_read_accompaniment does not read. */
        bool motive_word = text->accompaniment && request_equals_ignore_case(text->accompaniment, "motive");
        if (motive_word && !text->accompaniment_motive) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment",
                            "accompaniment motive needs an accompaniment motive");
        }
        if (text->accompaniment_motive && text->accompaniment && !motive_word) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment",
                            "the accompaniment motive is the accompaniment pattern; it cannot be combined with "
                            "accompaniment %s",
                            text->accompaniment);
        }
        out->has_accompaniment = true;
        ComposerStatus read = api_read_accompaniment(motive_word ? NULL : text->accompaniment, text->texture, text->meter,
                                                     &out->meter, &out->accompaniment, error);
        if (read != COMPOSER_OK) {
            return read;
        }
        if (text->accompaniment_motive) {
            out->accompaniment.pattern = PLAN_ACCOMPANIMENT_MOTIVE;
        }
    }
    if (text->accompaniment_motive && (chorale || text->voicing)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment",
                        "an accompaniment motive cannot be combined with voicing or texture chorale");
    }
    if (text->style_weight && !text->has_style) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight", "style_weight needs style");
    }
    if (text->without_count > 0 && !text->has_style) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/without", "style_without needs style");
    }
    if (text->style_weight && !request_parse_ratio(text->style_weight, &out->style_weight)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight",
                        "style_weight must be a fraction like 1/2 or a whole number, not '%s'", text->style_weight);
    }
    return COMPOSER_OK;
}

cJSON *api_grow_construction_json(const SectionConstruction *given, const SectionPairing *pairing, const char *text) {
    static const char *const REASONS[] = {"given", "motive", "length"};
    GrowConstructionSpans spans;
    grow_construction_spans(pairing, given, &spans);
    cJSON *object = cJSON_CreateObject();
    cJSON *halves = object ? cJSON_AddArrayToObject(object, "halves") : NULL;
    cJSON *ideas = object ? cJSON_AddArrayToObject(object, "ideas") : NULL;
    cJSON *extensions = object ? cJSON_AddArrayToObject(object, "extensions") : NULL;
    bool ok = object && halves && ideas && extensions;
    for (int half = 0; ok && half < 2; half++) {
        ok = cJSON_AddItemToArray(halves, cJSON_CreateNumber((double)spans.halves[half]));
    }
    bool listed[GROW_IDEA_COUNT] = {false};
    for (int taken = 0; ok && taken < GROW_IDEA_COUNT; taken++) {
        int next = -1;
        for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
            if (!listed[idea] && spans.idea_bars[idea] > 0 && (next < 0 || spans.idea_first[idea] < spans.idea_first[next])) {
                next = idea;
            }
        }
        if (next < 0) {
            break;
        }
        listed[next] = true;
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(ideas, item) && cJSON_AddStringToObject(item, "idea", theme_shape_idea_name((GrowIdea)next)) &&
             cJSON_AddNumberToObject(item, "bars", (double)spans.idea_bars[next]) &&
             cJSON_AddNumberToObject(item, "first_bar", (double)spans.idea_first[next] + 1);
    }
    for (size_t k = 0; ok && k < given->extension_count; k++) {
        const SectionExtension *extension = &given->extensions[k];
        bool names_idea = extension->kind == SECTION_EXTENSION_REPEAT || extension->kind == SECTION_EXTENSION_STRETCH;
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(extensions, item) && cJSON_AddStringToObject(item, "kind", API_EXTENSION_KINDS[extension->kind]) &&
             cJSON_AddItemToObject(item, "idea",
                                   names_idea ? cJSON_CreateString(theme_shape_idea_name((GrowIdea)extension->idea)) : cJSON_CreateNull()) &&
             cJSON_AddNumberToObject(item, "bars", (double)spans.extension_bars[k]) &&
             cJSON_AddNumberToObject(item, "first_bar", (double)spans.extension_first[k] + 1) &&
             cJSON_AddBoolToObject(item, "varied", extension->varied);
    }
    ok = ok && cJSON_AddStringToObject(object, "reason", REASONS[given->reason]) && cJSON_AddStringToObject(object, "text", text);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

ComposerStatus api_fail_motif_long(ComposerError *error, const char *path, const Motive *motive, const PlanMeter *meter,
                                   size_t theme_bars, size_t idea_bars) {
    Rational bar;
    plan_meter_bar_length(meter, &bar);
    size_t bars = motive ? grow_motive_layout_bars(motive, bar) : 0;
    return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "the motif is %zu bars; the basic idea of a %zu-bar theme is %zu",
                    bars, theme_bars, idea_bars);
}
