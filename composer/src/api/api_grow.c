#include "api_grow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <standard-texture/texture_rules.h>

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
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

#define MAX_CHORDS 96
#define MAX_LANE_SPANS 128
#define MAX_DEVELOPMENTS 8
#define SLOT_ROWS 16

static const char *const BEGINNING_NAMES[] = {"presentation", "antecedent", "compound basic idea"};
static const char *const ENDING_NAMES[] = {"continuation", "consequent", "cadential"};

NodeId api_find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* --- reading the brief ------------------------------------------------------------------------------ */

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
static bool parse_hybrid(const char *text, PlanThemePairing *pairing) {
    const char *plus = strchr(text, '+');
    if (!plus) {
        return false;
    }
    size_t length = (size_t)(plus - text);
    bool beginning_found = false;
    bool ending_found = false;
    for (int i = 0; API_THEME_BEGINNINGS[i].value; i++) {
        if (strlen(API_THEME_BEGINNINGS[i].value) == length && strncmp(text, API_THEME_BEGINNINGS[i].value, length) == 0) {
            pairing->beginning = (PlanThemeBeginning)i;
            beginning_found = true;
        }
    }
    for (int i = 0; API_THEME_ENDINGS[i].value; i++) {
        if (request_equals_ignore_case(plus + 1, API_THEME_ENDINGS[i].value)) {
            pairing->ending = (PlanThemeEnding)i;
            ending_found = true;
        }
    }
    return beginning_found && ending_found;
}

/* --- the construction -------------------------------------------------------------------------------- */

static const char *const EXTENSION_KINDS[] = {"repeat", "stretch", "addition", "evaded"};

static ComposerStatus read_extension(const cJSON *item, const char *base, size_t index, PlanExtension *out, ComposerError *error) {
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
        if (strcmp(kind, EXTENSION_KINDS[k]) == 0) {
            kind_at = k;
        }
    }
    char member[224];
    if (kind_at < 0) {
        snprintf(member, sizeof member, "%s/kind", path);
        return api_fail(error, COMPOSER_ERR_REQUEST, member, "kind must be repeat, stretch, addition or evaded, not '%s'", kind);
    }
    out->kind = (PlanExtensionKind)kind_at;
    if (idea) {
        const ThemeShapeIdea *row = theme_shape_idea_find(idea);
        if (!row) {
            snprintf(member, sizeof member, "%s/idea", path);
            return api_fail(error, COMPOSER_ERR_REQUEST, member, "unknown idea '%s'", idea);
        }
        out->idea = (unsigned char)row->idea;
    } else if (out->kind == PLAN_EXTENSION_REPEAT || out->kind == PLAN_EXTENSION_STRETCH) {
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

ComposerStatus api_grow_read_construction_object(const cJSON *item, const char *base, PlanConstruction *construction,
                                                 ComposerError *error) {
    char ideas_path[192];
    snprintf(ideas_path, sizeof ideas_path, "%s/ideas", base);
    memset(construction, 0, sizeof(*construction));
    construction->reason = PLAN_CONSTRUCTION_GIVEN;
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
        if ((size_t)cJSON_GetArraySize(extensions) > PLAN_CONSTRUCTION_EXTENSIONS) {
            return api_fail(error, COMPOSER_ERR_REQUEST, extensions_path, "a construction has at most %d extensions",
                            PLAN_CONSTRUCTION_EXTENSIONS);
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
    PlanConstruction *construction = &out->construction;
    ComposerStatus read = api_grow_read_construction_object(item, "/construction", construction, error);
    if (read != COMPOSER_OK) {
        return read;
    }
    out->has_construction = true;
    /* Without bars the theme is as long as its construction. */
    if (!out->text->bars) {
        size_t total = grow_construction_extra_bars(&out->pairing, construction);
        for (int idea = 0; idea < PLAN_CONSTRUCTION_IDEAS; idea++) {
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
        out->pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        out->pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    } else if (request_equals_ignore_case(text->into, "period")) {
        out->into = GROW_INTO_PERIOD;
        out->pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
        out->pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
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
        if (!parse_hybrid(text->hybrid, &out->pairing)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid",
                            "hybrid must be a beginning (presentation, antecedent or cbi), a plus sign and an ending "
                            "(continuation, consequent or cadential), not '%s'",
                            text->hybrid);
        }
        bool sentence = out->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
                        out->pairing.ending == PLAN_THEME_ENDING_CONTINUATION;
        bool period = out->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
                      out->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT;
        if (sentence || period) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid", "%s is a %s, not a hybrid; use into %s", text->hybrid,
                            sentence ? "sentence" : "period", sentence ? "sentence" : "period");
        }
    } else if (text->hybrid) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/hybrid", "hybrid applies only to into hybrid");
    }
    if (text->mode && api_choice_find_ignore_case(API_PIECE_MODES, text->mode) < 0) {
        char modes[64];
        api_choice_list(API_PIECE_MODES, modes, sizeof modes);
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode must be %s, not '%s'", modes, text->mode);
    }
    if (text->mode && request_equals_ignore_case(text->mode, "style") && !text->has_style) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode style needs style");
    }
    /* With a mode the key may be a tonic alone: the mode is the one the mode gives. */
    char tonic_key[64];
    const char *key_text = text->key;
    if (text->mode && strlen(key_text) + 7 < sizeof tonic_key && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof tonic_key, "%s major", key_text);
        key_text = tonic_key;
    }
    if (!request_parse_key(key_text, &out->key)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, for "
                        "example \"D minor\"",
                        text->key);
    }
    if (text->mode && request_equals_ignore_case(text->mode, "major")) {
        out->key.mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (text->mode && request_equals_ignore_case(text->mode, "minor")) {
        out->key.mode = KEY_STRUCTURE_MODE_MINOR;
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
        if (out->pairing.beginning != PLAN_THEME_BEGINNING_ANTECEDENT) {
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

/* --- styles ------------------------------------------------------------------------------------------ */

ComposerStatus api_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                              ComposerStyleWeights *out, ComposerError *error) {
    if (!blend_path) {
        if (all_text || list_text) {
            return api_fail(error, COMPOSER_ERR_REQUEST, all_text ? "/blend_weight" : "/blend_weights",
                            "%s needs blend", all_text ? "blend_weight" : "blend_weights");
        }
        return COMPOSER_OK;
    }
    if (!has_style) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/blend", "blend needs style");
    }
    ComposerRatio all = {1, 2};
    if (all_text) {
        if (!request_parse_ratio(all_text, &all)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weight", "blend_weight must be a fraction like 1/2, not '%s'",
                            all_text);
        }
        if (all.numerator > all.denominator) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weight", "blend_weight must be from 0 to 1, not '%s'",
                            all_text);
        }
    }
    *out = composer_style_weights_all(all);
    if (!list_text) {
        return COMPOSER_OK;
    }
    bool seen[STYLE_DIMENSION_COUNT] = {false};
    const char *at = list_text;
    while (*at) {
        const char *end = strchr(at, ',');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        char item[64];
        if (length == 0 || length >= sizeof item) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights", "blend_weights must be like harmony=1/2,form=0, not '%s'",
                            list_text);
        }
        memcpy(item, at, length);
        item[length] = '\0';
        char *equals = strchr(item, '=');
        StyleDimension dimension;
        if (!equals) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights", "blend_weights must be like harmony=1/2,form=0, not '%s'",
                            list_text);
        }
        *equals = '\0';
        if (!style_dimension_from_name(item, &dimension)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights",
                            "blend_weights: '%s' is not a dimension (harmony, rhythm, melody, form)", item);
        }
        if (seen[dimension]) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights", "blend_weights names %s twice", item);
        }
        seen[dimension] = true;
        ComposerRatio weight;
        if (!request_parse_ratio(equals + 1, &weight)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights", "blend_weights: %s must be a fraction like 1/2, not '%s'",
                            item, equals + 1);
        }
        if (weight.numerator > weight.denominator) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights", "blend_weights: %s must be from 0 to 1, not '%s'", item,
                            equals + 1);
        }
        out->numerator[dimension] = weight.numerator;
        out->denominator[dimension] = weight.denominator;
        at += length;
        if (*at == ',') {
            at++;
            if (*at == '\0') {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/blend_weights",
                                "blend_weights must be like harmony=1/2,form=0, not '%s'", list_text);
            }
        }
    }
    return COMPOSER_OK;
}

ComposerStatus api_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                              const char *const *without, size_t without_count, ComposerStyle **out,
                              ComposerError *error) {
    if (!blend_path) {
        return api_load_style(style_path, without, without_count, "/style", "/without", out, error);
    }
    ComposerStyle *first = NULL;
    ComposerStyle *second = NULL;
    ComposerStatus status = api_load_style(style_path, without, without_count, "/style", "/without", &first, error);
    if (status == COMPOSER_OK) {
        status = api_load_style(blend_path, without, without_count, "/blend", "/without", &second, error);
    }
    if (status == COMPOSER_OK) {
        ComposerStyleResult blended = composer_style_blend(first, second, weights, out);
        if (blended != COMPOSER_STYLE_OK) {
            status = api_fail(error, blended == COMPOSER_STYLE_ERR_ARGUMENT ? COMPOSER_ERR_REQUEST : COMPOSER_ERR_UNSATISFIABLE,
                              "/blend", "cannot blend '%s' with '%s': %s", style_path, blend_path,
                              composer_style_result_name(blended));
        }
    }
    composer_style_destroy(first);
    composer_style_destroy(second);
    return status;
}

ComposerStatus api_grow_set_tempo_and_mode(GrowSetup *out, const ComposerRatio *option_tempo, ComposerError *error) {
    out->tempo.numerator = 0;
    out->tempo.denominator = 0;
    out->tempo_from = "none";
    if (option_tempo) {
        out->tempo = *option_tempo;
        out->tempo_from = "option";
    } else if (composer_style_tempo(out->style, &out->tempo)) {
        out->tempo_from = "style";
    }
    bool from_style = out->text->mode && request_equals_ignore_case(out->text->mode, "style");
    if (from_style) {
        bool minor = false;
        if (!composer_style_mode(out->style, &minor)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode style needs a style with a mode category");
        }
        out->key.mode = minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
    }
    out->mode_from = from_style ? "style" : "request";
    return COMPOSER_OK;
}

/* --- growing ---------------------------------------------------------------------------------------- */

typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Text;

static bool text_append(Text *text, const char *format, ...) __attribute__((format(printf, 2, 3)));

static bool text_append(Text *text, const char *format, ...) {
    char piece[256];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(piece, sizeof(piece), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(piece)) {
        return false;
    }
    if (text->length + (size_t)written + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity * 2 : 1024;
        while (capacity < text->length + (size_t)written + 1) {
            capacity *= 2;
        }
        char *grown = realloc(text->text, capacity);
        if (!grown) {
            return false;
        }
        text->text = grown;
        text->capacity = capacity;
    }
    memcpy(text->text + text->length, piece, (size_t)written + 1);
    text->length += (size_t)written;
    return true;
}

/* Everything the harmony, the bass and the melody say over the theme, as text: two themes with the same text are one. */
static char *signature_of(const PiecePlan *plan, NodeId theme_node) {
    static const PlanLaneKind LANES[3] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    PlanSpan span = plan_node_span(plan, theme_node);
    Text text = {0};
    for (int lane = 0; lane < 3; lane++) {
        LaneSpanId ids[MAX_LANE_SPANS];
        size_t count = plan_lane_find(plan, LANES[lane], 0, span, ids, MAX_LANE_SPANS);
        for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
            PlanSpan at = plan_lane_span_span(plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
            if (!text_append(&text, "|%d@%lld/%lld+%lld/%lld:", lane, at.start.num, at.start.den, at.length.num,
                             at.length.den)) {
                free(text.text);
                return NULL;
            }
            if (LANES[lane] == PLAN_LANE_HARMONY) {
                if (!text_append(&text, "%s", payload->harmony.roman_numeral)) {
                    free(text.text);
                    return NULL;
                }
                continue;
            }
            for (size_t n = 0; n < payload->notes.note_count; n++) {
                const PlanNote *note = &payload->notes.notes[n];
                if (!text_append(&text, "%d@%lld/%lld+%lld/%lld,", note->midi, note->offset.num, note->offset.den,
                                 note->duration.num, note->duration.den)) {
                    free(text.text);
                    return NULL;
                }
            }
        }
    }
    return text.text ? text.text : strdup("");
}

/* The bar (from 0) of a plan time in the theme. */
static long bar_at(Rational time, PlanSpan theme, Rational bar) {
    long long num = ((long long)time.num * theme.start.den - (long long)theme.start.num * time.den) * bar.den;
    long long den = (long long)time.den * theme.start.den * bar.num;
    return den > 0 && num >= 0 ? (long)(num / den) : 0;
}

/*
 * The bar (from 0) of a chord Given by the request that the earliest span the theme still has open lies in or beside, else
 * -1: the chords are what the stage could not write around.
 */
static long open_bar_with_chords(const PiecePlan *plan, NodeId theme_node, const PlanMeter *meter,
                                 const GrowMaterial *material, GrowStage stage) {
    Rational bar;
    PlanSpan theme;
    PlanSpan open = {{0, 1}, {0, 1}};
    Rational open_end;
    bool any = false;
    /* Only the stages that write around the chords can be stopped by them. */
    PlanLaneKind lane = stage == GROW_STAGE_HARMONY ? PLAN_LANE_HARMONY
                        : stage == GROW_STAGE_BASS  ? PLAN_LANE_BASS
                                                    : PLAN_LANE_MELODY;
    if (material->chord_count == 0 || theme_node == PLAN_NODE_NONE ||
        (stage != GROW_STAGE_HARMONY && stage != GROW_STAGE_BASS && stage != GROW_STAGE_MELODY) ||
        rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar) != RATIONAL_OK) {
        return -1;
    }
    theme = plan_node_span(plan, theme_node);
    LaneSpanId ids[MAX_LANE_SPANS];
    size_t count = plan_lane_find(plan, lane, 0, theme, ids, MAX_LANE_SPANS);
    for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED &&
            (!any || rational_cmp(span.start, open.start) < 0)) {
            open = span;
            any = true;
        }
    }
    if (!any || plan_span_end(open, &open_end) != PLAN_OK) {
        return -1;
    }
    long first = bar_at(open.start, theme, bar);
    long last = bar_at(open_end, theme, bar);
    for (long at = first > 0 ? first - 1 : 0; at <= last; at++) {
        for (size_t c = 0; c < material->chord_count; c++) {
            if (bar_at((Rational){material->chords[c].onset.num, material->chords[c].onset.den}, (PlanSpan){{0, 1}, {0, 1}},
                       bar) == at) {
                return at;
            }
        }
    }
    return -1;
}

static void passed_add(GrowRun *run, const ComposeRejection *rejection) {
    ComposeRejection *grown = realloc(run->passed, (run->passed_count + 1) * sizeof(ComposeRejection));
    if (!grown) {
        free(rejection->items);
        return;
    }
    run->passed = grown;
    run->passed[run->passed_count++] = *rejection;
}

static void theme_destroy(GrowTheme *theme) {
    plan_destroy(theme->plan);
    grow_outcome_destroy(&theme->outcome);
    analyzer_check_destroy(&theme->check);
    free(theme->signature);
    memset(theme, 0, sizeof(*theme));
}

void api_grow_run_destroy(GrowRun *run) {
    for (size_t k = 0; k < run->made; k++) {
        theme_destroy(&run->themes[k]);
    }
    compose_rejections_free(run->passed, run->passed_count);
    memset(run, 0, sizeof(*run));
    run->chord_bar = -1;
}

/* The request of the theme for a seed: the setup and its material, which it points into and must outlive it. */
void api_grow_request_for(const GrowSetup *setup, unsigned long seed, GrowRequest *request) {
    const GrowMaterial *material = setup->material;
    (void)seed;
    memset(request, 0, sizeof(*request));
    request->motive = setup->motive;
    request->source = setup->seeded ? &setup->source : NULL;
    request->key = setup->key;
    request->meter = setup->meter;
    request->bars = setup->bars;
    request->construction = setup->has_construction ? &setup->construction : NULL;
    request->pairing = setup->pairing;
    request->cadence = setup->cadence;
    request->first_cadence = setup->first_cadence;
    request->has_accompaniment = setup->has_accompaniment;
    request->accompaniment = setup->accompaniment;
    request->accompaniment_motive = setup->accompaniment_motive;
    request->has_voicing = setup->has_voicing;
    request->voicing = setup->voicing;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; material && slot < GROW_SLOT_COUNT; slot++) {
        request->slot_motives[slot] = material->has_slot[slot] ? &material->slot_motives[slot] : NULL;
    }
    for (int idea = 0; material && idea < GROW_IDEA_COUNT; idea++) {
        request->idea_motives[idea] = material->has_idea[idea] ? &material->idea_motives[idea] : NULL;
    }
    if (material) {
        request->chords = material->chord_count > 0 ? material->chords : NULL;
        request->chord_count = material->chord_count;
    }
}

/* Builds the plan and runs the stages for one seed. GROW_ERR_STUCK is the only result a later seed can mend. */
static GrowResult grow_one(const GrowSetup *setup, unsigned long seed, size_t attempt, GrowTheme *theme, GrowRun *run,
                           bool *cancelled) {
    const GrowMaterial *material = setup->material;
    GrowRequest request;
    api_grow_request_for(setup, seed, &request);
    memset(theme, 0, sizeof(*theme));
    theme->seed = seed;
    GrowResult built = grow_build_plan_checked(&request, &theme->plan, &run->refusal);
    if (built != GROW_OK) {
        return built;
    }
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = setup->motive;
    config.context.development = setup->development;
    if (setup->continuation) {
        config.context.continuation = *setup->continuation;
    }
    config.context.basic_as_written = setup->basic_as_written;
    if (setup->contrast) {
        config.context.contrast = *setup->contrast;
    }
    config.context.motif = setup->motif;
    config.fidelity = setup->fidelity;
    config.melody = setup->melody;
    config.style = setup->style;
    config.style_weight = setup->style_weight;
    config.tempo = setup->tempo;
    config.job = setup->job;
    if (material) {
        config.pool = material->pool;
        config.pool_count = material->pool_count;
    }
    GrowResult ran = grow_run(theme->plan, &config, &theme->outcome);
    NodeId theme_node = api_find_node(theme->plan, "theme");
    if (theme->outcome.cancelled) {
        *cancelled = true;
        theme_destroy(theme);
        return GROW_ERR_STUCK;
    }
    if (ran == GROW_ERR_STUCK && material) {
        run->chord_bar = open_bar_with_chords(theme->plan, theme_node, &setup->meter, material, theme->outcome.stuck_stage);
    }
    if (ran == GROW_ERR_STUCK && setup->explain) {
        ComposeRejection rejection;
        memset(&rejection, 0, sizeof(rejection));
        rejection.round = attempt;
        rejection.seed = seed;
        snprintf(rejection.stage, sizeof(rejection.stage), "%s",
                 theme->outcome.stuck ? grow_stage_name(theme->outcome.stuck_stage) : "");
        snprintf(rejection.reason, sizeof(rejection.reason), "stuck");
        passed_add(run, &rejection);
    }
    /* A theme the Analyzer cannot read is not held against it. */
    if (ran == GROW_OK && setup->check && theme_node != PLAN_NODE_NONE) {
        theme->checked = analyzer_check_section(theme->plan, theme_node, &theme->check) == ANALYZER_CHECK_OK;
        if (theme->checked && theme->check.contradicts) {
            ComposeRejection rejection;
            if (setup->explain &&
                explain_analyzer_rejection(theme->plan, theme_node, &theme->check, attempt, seed, &rejection) == EXPLAIN_OK) {
                passed_add(run, &rejection);
            }
            ran = GROW_ERR_STUCK;
        }
    }
    if (ran == GROW_OK) {
        theme->signature = theme_node == PLAN_NODE_NONE ? NULL : signature_of(theme->plan, theme_node);
        if (!theme->signature) {
            ran = GROW_ERR_ALLOC;
        }
    }
    if (ran != GROW_OK) {
        theme_destroy(theme);
    }
    return ran;
}

ComposerStatus api_fail_motif_long(ComposerError *error, const char *path, const Motive *motive, const PlanMeter *meter,
                                   size_t theme_bars, size_t idea_bars) {
    size_t bars = motive ? grow_motive_layout_bars(motive, grow_bar_length(meter)) : 0;
    return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "the motif is %zu bars; the basic idea of a %zu-bar theme is %zu",
                    bars, theme_bars, idea_bars);
}

ComposerStatus api_grow_run(const GrowSetup *setup, GrowRun *run, ComposerError *error) {
    memset(run, 0, sizeof(*run));
    run->chord_bar = -1;
    run->last_seed = setup->seed;
    const GrowBriefText *text = setup->text;
    unsigned long tries = (unsigned long)GROW_MAX_SEED_TRIES * setup->suggest;
    for (unsigned long attempt = 0; attempt < tries && run->made < setup->suggest; attempt++) {
        if (job_step(setup->job, "grow", (unsigned)attempt, (unsigned)tries)) {
            return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        }
        GrowTheme theme;
        bool cancelled = false;
        run->last_seed = (setup->seed + attempt) & GROW_SEED_MAX;
        GrowResult result = grow_one(setup, run->last_seed, attempt, &theme, run, &cancelled);
        if (cancelled) {
            return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        }
        if (result == GROW_OK) {
            bool repeated = false;
            for (size_t k = 0; k < run->made; k++) {
                repeated = repeated || strcmp(run->themes[k].signature, theme.signature) == 0;
            }
            if (repeated) {
                theme_destroy(&theme);
            } else {
                theme.passed_before = run->passed_count;
                run->themes[run->made++] = theme;
            }
        } else if (result != GROW_ERR_STUCK) {
            /* A refusal before seeding says why and where: its path is that of the field in a theme request. */
            const GrowRefusal *refusal = &run->refusal;
            bool named = setup->material || strncmp(refusal->path, "/construction", 13) == 0 ||
                         strcmp(refusal->path, "/bars") == 0;
            return refusal->message[0] != '\0'
                       ? api_fail(error, COMPOSER_ERR_UNSATISFIABLE, named ? refusal->path : "", "%s", refusal->message)
                       : api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", grow_result_name(result));
        }
    }
    if (run->made == 0 && run->chord_bar >= 0) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, setup->material ? "/chords" : "", "the given chords in bar %ld do not allow the %s",
                        run->chord_bar, text->cadence ? text->cadence : "PAC");
    }
    if (run->made == 0) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                        "no theme could be grown for %s, %s, %zu bars, %s from seeds %lu to %lu", text->key, text->meter,
                        setup->bars, text->into, setup->seed, run->last_seed);
    }
    return COMPOSER_OK;
}

/* --- describing a theme ------------------------------------------------------------------------------ */

static const char *const KIND_NAMES[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};

static double in_bars(Rational time, Rational bar) {
    return ((double)time.num / (double)time.den) / ((double)bar.num / (double)bar.den);
}

/* A time as bars from the start of the theme. */
static double offset_in_bars(Rational time, PlanSpan theme, Rational bar) {
    Rational offset;
    offset.num = time.num * (long)theme.start.den - theme.start.num * (long)time.den;
    offset.den = time.den * theme.start.den;
    return in_bars(offset, bar);
}

/* A number as the commands print it with %g: six digits. */
static cJSON *number_g(double value) {
    char text[64];
    snprintf(text, sizeof text, "%g", value);
    return cJSON_CreateNumber(strtod(text, NULL));
}

static const char *relation_word(const PlanMotiveRef *ref) {
    if (!ref->has_step) {
        return "none";
    }
    switch (ref->step.kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        /* A transposition by steps of the key is a sequence (ch. V). */
        return ref->step.value[0] != 0 ? "sequence" : "transposed";
    case MOTIVE_RELATION_INVERT:
        return "inverted";
    case MOTIVE_RELATION_RETROGRADE:
        return "retrograde";
    case MOTIVE_RELATION_AUGMENT:
        return "augmented";
    case MOTIVE_RELATION_DIMINISH:
        return "diminished";
    case MOTIVE_RELATION_FRAGMENT:
        return "fragmented";
    case MOTIVE_RELATION_EXACT:
        return "restated";
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        return "interval changed";
    case MOTIVE_RELATION_LIQUIDATE:
        return "liquidated";
    case MOTIVE_RELATION_DOMINANT_FORM:
        return "dominant form";
    default:
        return "none";
    }
}

/* "restated, exact", "restated, 1 step up (tonal)", "fragmented, leading, 3 notes": the development the ref asks for. */
static void describe_ref(const PlanMotiveRef *ref, char *out, size_t size) {
    static const char *const POSITIONS[] = {"", ", leading", ", trailing", ", middle"};
    const long *v = ref->step.value;
    MotiveRelationKind kind = ref->step.kind;
    bool has = ref->has_step;
    bool tonal = has && ((kind == MOTIVE_RELATION_TRANSPOSE && v[0] != 0) || (kind == MOTIVE_RELATION_INVERT && v[1] != 0));
    int steps = has && kind == MOTIVE_RELATION_TRANSPOSE && v[0] != 0 ? (int)v[1] : 0;
    int semitones = has && kind == MOTIVE_RELATION_TRANSPOSE && v[0] == 0 ? (int)v[1] : 0;
    size_t used = (size_t)snprintf(out, size, "%s", relation_word(ref));
    char *end = out + size;
    bool moved = false;
    if (steps != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d step%s %s%s", steps > 0 ? steps : -steps,
                                 steps == 1 || steps == -1 ? "" : "s", steps > 0 ? "up" : "down",
                                 tonal ? " (tonal)" : "");
        moved = true;
    } else if (semitones != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones", semitones);
        moved = true;
    }
    if (has && kind == MOTIVE_RELATION_EXACT && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", exact");
    }
    if (tonal && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", tonal");
    }
    if (has && kind == MOTIVE_RELATION_FRAGMENT && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, "%s", POSITIONS[v[0] >= 0 && v[0] < 4 ? v[0] : 0]);
        if (v[1] > 0 && used < size) {
            used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d notes", (int)v[1]);
        }
    }
    if (has && kind == MOTIVE_RELATION_LIQUIDATE && v[0] > 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", drop %d", (int)v[0]);
    }
    if (has && kind == MOTIVE_RELATION_INTERVAL_CHANGE && v[1] != 0 && used < size) {
        snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones on interval %d", (int)v[1], (int)v[0]);
    }
}

static cJSON *structure_json(const PiecePlan *plan, NodeId theme_node, Rational bar) {
    cJSON *list = cJSON_CreateArray();
    /* A depth-first walk with an explicit stack (the plan is a tree of a handful of levels). */
    struct {
        NodeId node;
        size_t next;
    } stack[16];
    size_t depth = 0;
    PlanSpan theme = plan_node_span(plan, theme_node);
    stack[0].node = theme_node;
    stack[0].next = 0;
    bool entered = false;
    while (list) {
        if (!entered) {
            NodeId node = stack[depth].node;
            PlanSpan span = plan_node_span(plan, node);
            Rational end;
            rational_add(span.start, span.length, &end);
            const char *name = plan_node_name(plan, node);
            cJSON *item = cJSON_CreateObject();
            if (!item || !cJSON_AddStringToObject(item, "name", name ? name : "") ||
                !cJSON_AddStringToObject(item, "kind", KIND_NAMES[plan_node_kind(plan, node)]) ||
                !cJSON_AddItemToObject(item, "start", number_g(offset_in_bars(span.start, theme, bar))) ||
                !cJSON_AddItemToObject(item, "end", number_g(offset_in_bars(end, theme, bar))) ||
                !cJSON_AddItemToArray(list, item)) {
                cJSON_Delete(item);
                cJSON_Delete(list);
                return NULL;
            }
            entered = true;
        }
        if (stack[depth].next < plan_node_child_count(plan, stack[depth].node) && depth + 1 < 16) {
            NodeId child = plan_node_child_at(plan, stack[depth].node, stack[depth].next++);
            depth++;
            stack[depth].node = child;
            stack[depth].next = 0;
            entered = false;
            continue;
        }
        if (depth == 0) {
            break;
        }
        depth--;
    }
    return list;
}

static cJSON *chords_json(const PiecePlan *plan, NodeId theme_node, const PlanMeter *meter) {
    cJSON *list = cJSON_CreateArray();
    PlanSpan span = plan_node_span(plan, theme_node);
    Rational bar;
    rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_CHORDS);
    if (count > MAX_CHORDS) {
        count = MAX_CHORDS;
    }
    for (size_t i = 0; list && i < count; i++) {
        PlanSpan chord = plan_lane_span_span(plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        /* The offset from the start of the theme, as a fraction of a whole note, split into bars and the rest. */
        long off_num = chord.start.num * (long)span.start.den - span.start.num * (long)chord.start.den;
        unsigned long off_den = chord.start.den * span.start.den;
        long bars = (off_num * (long)bar.den) / ((long)off_den * bar.num);
        long rest_num = off_num * (long)bar.den - bars * bar.num * (long)off_den;
        unsigned long rest_den = off_den * bar.den;
        char bass[48] = "";
        LaneSpanId basses[2];
        if (plan_lane_find(plan, PLAN_LANE_BASS, 0, chord, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof bass; n++) {
                char name[16];
                util_pitch_name(notes->notes[n].midi, false, name, sizeof(name));
                used += (size_t)snprintf(bass + used, sizeof bass - used, n ? " %s" : "%s", name);
            }
        }
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddNumberToObject(item, "bar", (double)((int)bars + 1)) ||
            !cJSON_AddNumberToObject(item, "beat", (double)((int)(rest_num * meter->beat_unit / (long)rest_den) + 1)) ||
            !cJSON_AddStringToObject(item, "chord", harmony->roman_numeral) ||
            !cJSON_AddStringToObject(item, "function", util_function_name(harmony->has_function, harmony->function)) ||
            !cJSON_AddStringToObject(item, "bass", bass) || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

typedef struct {
    char motive[16];
    const char *idea;
    char how[96];
    char segment[PLAN_SEGMENT_LABEL_MAX + 1]; /* the segment the idea is made from, empty for none */
    char summary[192];
    bool measured;  /* the idea has a melody span whose fidelity was measured (R-332) */
    double fidelity; /* the score as a fraction, 3 decimals */
    double minimum;  /* the least the melody stage accepts for the idea's role and setting; 0 for an idea its ref makes differ */
    char heard[96];  /* the relation the recognizer hears between the idea and its source */
    bool agrees;     /* the heard relation is the one the ref intended */
} Development;

static cJSON *string_or_null(const char *text);

/* The motive the ideas are made from, in words: a segment, a slot's own motif, else the basic idea. */
static void source_words(const PiecePlan *plan, const PlanMotiveRef *ref, char *out, size_t size) {
    static const char *const SLOT_WORDS[] = {"basic idea", "contrasting motif", "continuation motif", "cadential motif"};
    const char *name = plan_has_node(plan, ref->motive) ? plan_node_name(plan, ref->motive) : NULL;
    if (ref->segment[0] != '\0') {
        snprintf(out, size, "segment %s", ref->segment);
    } else if (name && strncmp(name, "distant for m", 13) == 0) {
        /* A distant contrast made for the idea (R-475): its own rhythm, the contour changed. */
        snprintf(out, size, "a distant variant");
    } else if (ref->steps > 0 || !name) {
        snprintf(out, size, "basic idea");
    } else if (name[0] == 's' && name[1] >= '1' && name[1] <= '3' && name[2] == '\0') {
        snprintf(out, size, "%s", SLOT_WORDS[name[1] - '0']);
    } else if (name[0] == 'i' && name[1] >= '0' && name[1] <= '9' && name[2] == '\0') {
        snprintf(out, size, "its own motif");
    } else {
        snprintf(out, size, "basic idea");
    }
}

/*
 * One line naming an idea, its source and its operation (ch. VIII): "repetition = basic idea, dominant form",
 * "fragment 2 = segment b, 1 step down (tonal)", "cadential = basic idea, segments liquidated, 2 steps". The idea is named
 * as the plan names it; `how` is describe_ref's text, without the word it starts with when the operation is a restatement or
 * a sequence (the operation says it).
 */
static void summarize_development(const PiecePlan *plan, const PlanMotiveRef *ref, const char *idea, const char *how,
                                  char *out, size_t size) {
    char source[48];
    char operation[96];
    source_words(plan, ref, source, sizeof(source));
    if (ref->steps > 0) {
        snprintf(operation, sizeof(operation), "segments liquidated, %zu step%s", ref->steps, ref->steps == 1 ? "" : "s");
    } else if (strncmp(how, "restated, ", 10) == 0 || strncmp(how, "sequence, ", 10) == 0) {
        snprintf(operation, sizeof(operation), "%s", how + 10);
    } else {
        snprintf(operation, sizeof(operation), "%s", how);
    }
    snprintf(out, size, "%s = %s, %s", idea[0] != '\0' ? idea : "(unused)", source, operation);
}

static double score_fraction(MotiveScore score) {
    double value = (double)score.numerator / (double)(score.denominator > 0 ? score.denominator : 1);
    return (double)(long)(value * 1000.0 + 0.5) / 1000.0;
}

/*
 * The idea's fidelity to its source and the relation heard (R-332): the same measure the melody stage ranks and refuses by
 * (grow_idea_fidelity), with the minimum of the idea's role under the development setting. An idea whose ref makes it differ
 * from its source (grow_ref_departs) has no minimum: it is ranked, never refused. Left unmeasured when the plan cannot say.
 */
static void measure_development(const PiecePlan *plan, LaneSpanId span, const Motive *basic, const GrowFidelityTable *table,
                                GrowDevelopment development,
                                Development *out) {
    MotiveFidelity fidelity;
    Motive source;
    PlanMotiveRef ref;
    bool from_segment = false;
    out->measured = false;
    if (!grow_idea_fidelity(plan, span, basic, &fidelity) ||
        !grow_idea_fidelity_source(plan, span, basic, &source, &ref, &from_segment)) {
        return;
    }
    MotiveScore minimum = {0, 1};
    if (!grow_ref_departs(&ref, from_segment)) {
        minimum = grow_fidelity_minimum_in(table, development, grow_idea_role(out->idea));
    }
    out->measured = true;
    out->fidelity = score_fraction(fidelity.score);
    out->minimum = score_fraction(minimum);
    snprintf(out->heard, sizeof(out->heard), "%s", fidelity.heard);
    out->agrees = fidelity.agrees;
}

/*
 * The motives made from m0 (every motive node with a motive_ref) and the idea each one is the melody of, with the idea's
 * fidelity when it has a melody span (basic is the motive the run was made from, development its setting).
 */
static size_t collect_developments(const PiecePlan *plan, NodeId theme, const Motive *basic, const GrowFidelityTable *fidelity,
                                   GrowDevelopment development,
                                   Development *out) {
    size_t count = 0;
    LaneSpanId melodies[MAX_LANE_SPANS];
    size_t melody_count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), melodies, MAX_LANE_SPANS);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < MAX_DEVELOPMENTS; id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) != PLAN_NODE_MOTIVE || !name || strcmp(name, "m0") == 0) {
            continue;
        }
        const PlanConstraint *ref = NULL;
        for (size_t c = 0; c < plan_node_constraint_count(plan, id) && !ref; c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, c);
            ref = constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF ? constraint : NULL;
        }
        if (!ref) {
            continue;
        }
        snprintf(out[count].motive, sizeof(out[count].motive), "%s", name);
        out[count].idea = "";
        out[count].measured = false;
        LaneSpanId idea_span = 0;
        bool has_span = false;
        for (size_t m = 0; m < melody_count && m < MAX_LANE_SPANS; m++) {
            for (size_t c = 0; c < plan_lane_span_constraint_count(plan, melodies[m]); c++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, melodies[m], c);
                if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == id) {
                    const char *idea = plan_node_name(plan, plan_lane_span_owner(plan, melodies[m]));
                    out[count].idea = idea ? idea : "";
                    idea_span = melodies[m];
                    has_span = true;
                }
            }
        }
        if (has_span) {
            measure_development(plan, idea_span, basic, fidelity, development, &out[count]);
        }
        describe_ref(&ref->params.motive, out[count].how, sizeof(out[count].how));
        snprintf(out[count].segment, sizeof(out[count].segment), "%s", ref->params.motive.segment);
        summarize_development(plan, &ref->params.motive, out[count].idea, out[count].how, out[count].summary,
                              sizeof(out[count].summary));
        count++;
    }
    return count;
}

static cJSON *developments_json(const PiecePlan *plan, NodeId theme, const Motive *basic, const GrowFidelityTable *fidelity,
                                GrowDevelopment development) {
    Development developments[MAX_DEVELOPMENTS];
    size_t count = collect_developments(plan, theme, basic, fidelity, development, developments);
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddStringToObject(item, "motive", developments[i].motive) ||
            !cJSON_AddStringToObject(item, "idea", developments[i].idea) ||
            !cJSON_AddStringToObject(item, "development", developments[i].how) ||
            !cJSON_AddItemToObject(item, "segment", string_or_null(developments[i].segment)) ||
            !cJSON_AddStringToObject(item, "summary", developments[i].summary) ||
            !cJSON_AddItemToObject(item, "fidelity",
                                   developments[i].measured ? cJSON_CreateNumber(developments[i].fidelity) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "minimum",
                                   developments[i].measured ? cJSON_CreateNumber(developments[i].minimum) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "heard", developments[i].measured ? cJSON_CreateString(developments[i].heard) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "heard_agrees",
                                   developments[i].measured ? cJSON_CreateBool(developments[i].agrees) : cJSON_CreateNull()) ||
            !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* "hybrid (compound basic idea + continuation)" for a hybrid with its beginning and ending, else the form's name. */
static void form_label(int form, int beginning, int ending, char *out, size_t size) {
    if (form == PHRASE_LEVEL_FORM_HYBRID && beginning >= 0 && ending >= 0) {
        snprintf(out, size, "%s (%s + %s)", form_hear_form_name(form), phrase_beginning_name((PhraseBeginning)beginning),
                 phrase_ending_name((PhraseEnding)ending));
    } else {
        snprintf(out, size, "%s", form >= 0 ? form_hear_form_name(form) : "none");
    }
}

/* What was heard, in words; heard_form and requested are the labels of the two forms. */
static void heard_text(const GrowOutcome *outcome, char *heard_form, size_t form_size, char *requested, size_t requested_size,
                       char *out, size_t size) {
    form_label(outcome->requested_form, outcome->requested_beginning, outcome->requested_ending, requested, requested_size);
    heard_form[0] = '\0';
    if (outcome->heard_result != FORM_HEAR_OK) {
        snprintf(out, size, "not heard (%s)", form_hear_result_name(outcome->heard_result));
    } else if (outcome->heard.count == 0) {
        snprintf(out, size, "no theme heard");
    } else {
        const FormHeardTheme *first = &outcome->heard.themes[0];
        form_label(first->form, first->beginning, first->ending, heard_form, form_size);
        if (outcome->heard_as_requested) {
            snprintf(out, size, "heard as %s", heard_form);
        } else {
            snprintf(out, size, "heard as %s, not %s", heard_form, requested);
        }
    }
}

/* --- the slots of a theme (the theme command's) ------------------------------------------------------- */

/* The slot an idea has the function of, by the idea's name (grow_form.c has the same table). */
static GrowSlot idea_slot(const char *name) {
    for (int idea = 0; name && idea < GROW_IDEA_COUNT; idea++) {
        if (strcmp(name, grow_idea_name((GrowIdea)idea)) == 0) {
            return theme_shape_idea_slot((GrowIdea)idea);
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
} SlotRow;

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

static void describe_row(const GrowOutcome *outcome, NodeId motive_node, SlotRow *row) {
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
static size_t collect_slot_rows(const PiecePlan *plan, const GrowOutcome *outcome, SlotRow *rows) {
    NodeId theme = api_find_node(plan, "theme");
    size_t count = 0;
    if (theme == PLAN_NODE_NONE) {
        return 0;
    }
    PlanSpan span = plan_node_span(plan, theme);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < SLOT_ROWS; id++) {
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
                SlotRow *row = &rows[count];
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
                } else if (direct && strncmp(direct, "distant for m", 13) == 0) {
                    /* A distant contrast made for the idea (R-475). */
                    row->source = "distant";
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

static cJSON *string_or_null(const char *text) {
    return text && text[0] ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *slots_json(const GrowThemeInfo *info, const PiecePlan *plan, const GrowOutcome *outcome) {
    SlotRow rows[SLOT_ROWS];
    size_t count = collect_slot_rows(plan, outcome, rows);
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        const char *motive = rows[i].own_idea >= 0 ? info->idea_motive_id[rows[i].own_idea]
                                                   : info->slot_motive_id[rows[i].origin];
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddStringToObject(item, "idea", rows[i].idea ? rows[i].idea : "") ||
            !cJSON_AddStringToObject(item, "slot", grow_slot_name(rows[i].slot)) ||
            !cJSON_AddStringToObject(item, "motive", motive ? motive : "") ||
            !cJSON_AddStringToObject(item, "source", rows[i].source) ||
            !cJSON_AddItemToObject(item, "item", string_or_null(rows[i].item)) ||
            !cJSON_AddItemToObject(item, "chain", string_or_null(rows[i].chain)) ||
            !cJSON_AddItemToObject(item, "connection",
                                   rows[i].connection >= 0 ? cJSON_CreateNumber((double)rows[i].connection)
                                                           : cJSON_CreateNull()) ||
            !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* --- the result -------------------------------------------------------------------------------------- */

static const char *const STYLE_DIMENSION_LABELS[STYLE_DIMENSION_COUNT] = {"harmony", "rhythm", "melody", "form"};

static cJSON *four_places(double value) {
    char text[64];
    snprintf(text, sizeof text, "%.4f", value);
    return cJSON_CreateNumber(strtod(text, NULL));
}

cJSON *api_style_distance_json(const StyleComparison *comparison) {
    cJSON *object = cJSON_CreateObject();
    for (int d = 0; object && d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d] &&
            !cJSON_AddItemToObject(object, STYLE_DIMENSION_LABELS[d], four_places(style_rational_value(comparison->dimension[d])))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    if (object && comparison->overall_present &&
        !cJSON_AddItemToObject(object, "overall", four_places(style_rational_value(comparison->overall)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_seed_json(const CorpusSeed *seed) {
    char score[48];
    snprintf(score, sizeof score, "%lu/%lu", seed->score_numerator, seed->score_denominator);
    cJSON *object = cJSON_CreateObject();
    cJSON *start = cJSON_CreateObject();
    cJSON *end = cJSON_CreateObject();
    if (!object || !start || !end || !cJSON_AddStringToObject(object, "piece", seed->piece) ||
        !cJSON_AddNumberToObject(start, "measure", seed->start_measure) ||
        !cJSON_AddItemToObject(start, "beat", number_g(seed->start_beat)) ||
        !cJSON_AddNumberToObject(end, "measure", seed->end_measure) ||
        !cJSON_AddItemToObject(end, "beat", number_g(seed->end_beat)) || !cJSON_AddItemToObject(object, "start", start) ||
        !cJSON_AddItemToObject(object, "end", end) || !cJSON_AddStringToObject(object, "score", score)) {
        /* start and end are owned by the object once added; free what is not. */
        if (object && !cJSON_GetObjectItem(object, "start")) {
            cJSON_Delete(start);
        }
        if (object && !cJSON_GetObjectItem(object, "end")) {
            cJSON_Delete(end);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *ratio_number(const ComposerRatio *ratio) {
    return number_g((double)ratio->numerator / (double)ratio->denominator);
}

cJSON *api_brief_json(const GrowBrief *brief) {
    cJSON *object = cJSON_CreateObject();
    cJSON *tempo = cJSON_CreateObject();
    cJSON *mode = cJSON_CreateObject();
    cJSON *without = cJSON_CreateArray();
    if (!object || !tempo || !mode || !without ||
        !cJSON_AddItemToObject(tempo, "qpm", brief->tempo.denominator > 0 ? ratio_number(&brief->tempo) : cJSON_CreateNull()) ||
        !cJSON_AddStringToObject(tempo, "from", brief->tempo_from) ||
        !cJSON_AddStringToObject(mode, "mode", brief->minor ? "minor" : "major") ||
        !cJSON_AddStringToObject(mode, "from", brief->mode_from) || !cJSON_AddItemToObject(object, "tempo", tempo) ||
        !cJSON_AddItemToObject(object, "mode", mode) ||
        !cJSON_AddItemToObject(object, "density", brief->has_density ? ratio_number(&brief->density) : cJSON_CreateNull()) ||
        !cJSON_AddItemToObject(object, "without", without) ||
        (brief->development && !cJSON_AddStringToObject(object, "development", brief->development))) {
        /* Whatever was not added to the object is still ours. */
        if (!object || !cJSON_GetObjectItem(object, "tempo")) {
            cJSON_Delete(tempo);
        }
        if (!object || !cJSON_GetObjectItem(object, "mode")) {
            cJSON_Delete(mode);
        }
        if (!object || !cJSON_GetObjectItem(object, "without")) {
            cJSON_Delete(without);
        }
        cJSON_Delete(object);
        return NULL;
    }
    for (size_t i = 0; i < brief->without_count; i++) {
        if (!cJSON_AddItemToArray(without, cJSON_CreateString(brief->without[i]))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}

const char *api_development_name(GrowDevelopment development) {
    return development == GROW_DEVELOP_MELODY ? "melody" : development == GROW_DEVELOP_THEME ? "theme" : "balanced";
}

void api_grow_brief(const GrowSetup *setup, GrowBrief *out) {
    memset(out, 0, sizeof(*out));
    out->tempo = setup->tempo;
    out->tempo_from = setup->tempo_from;
    out->minor = setup->key.mode == KEY_STRUCTURE_MODE_MINOR;
    out->mode_from = setup->mode_from;
    out->has_density = composer_style_density(setup->style, &out->density);
    out->without = setup->without;
    out->without_count = setup->without_count;
    out->development = setup->material ? api_development_name(setup->development) : NULL; /* a theme's, not a grow's */
}

cJSON *api_grow_construction_json(const PlanConstruction *given, const PlanThemePairing *pairing, const char *text) {
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
        const PlanExtension *extension = &given->extensions[k];
        bool names_idea = extension->kind == PLAN_EXTENSION_REPEAT || extension->kind == PLAN_EXTENSION_STRETCH;
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(extensions, item) && cJSON_AddStringToObject(item, "kind", EXTENSION_KINDS[extension->kind]) &&
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

/* The construction of the theme (R-468) as the response gives it, null for a theme without one. */
static cJSON *construction_json(const PiecePlan *plan, NodeId node) {
    const PlanConstraint *construction = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_CONSTRUCTION);
    const PlanConstraint *pairing = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_THEME_PAIRING);
    if (!construction || !pairing) {
        return cJSON_CreateNull();
    }
    char text[192];
    grow_section_describe(plan, node, text, sizeof text);
    return api_grow_construction_json(&construction->params.construction, &pairing->params.theme_pairing, text);
}

static cJSON *theme_json(const GrowSetup *setup, const GrowTheme *theme, const GrowThemeInfo *info, Rational bar) {
    const PiecePlan *plan = theme->plan;
    NodeId node = api_find_node(plan, "theme");
    cJSON *object = cJSON_CreateObject();
    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    cJSON *score = cJSON_CreateObject();
    bool ok = object && score && cJSON_AddNumberToObject(object, "seed", (double)theme->seed) &&
              cJSON_AddItemToObject(object, "structure", structure_json(plan, node, bar)) &&
              cJSON_AddItemToObject(object, "construction", construction_json(plan, node)) &&
              cJSON_AddItemToObject(object, "chords", chords_json(plan, node, &setup->meter)) &&
              cJSON_AddItemToObject(object, "developments",
                                    developments_json(plan, node, setup->motive, setup->fidelity, setup->development));
    if (ok && info) {
        ok = cJSON_AddItemToObject(object, "slots", slots_json(info, plan, &theme->outcome));
    }
    ok = ok && cJSON_AddNumberToObject(score, "numerator", (double)theme->outcome.score.numerator) &&
         cJSON_AddNumberToObject(score, "denominator", (double)theme->outcome.score.denominator) &&
         cJSON_AddItemToObject(object, "score", score);
    if (ok) {
        score = NULL;
    }
    ok = ok && cJSON_AddStringToObject(object, "requested", requested) &&
         cJSON_AddItemToObject(object, "heard", string_or_null(heard_form)) &&
         cJSON_AddBoolToObject(object, "heard_as_requested", theme->outcome.heard_as_requested) &&
         cJSON_AddStringToObject(object, "heard_text", heard);
    if (ok && theme->outcome.style_measured) {
        ok = cJSON_AddItemToObject(object, "style_distance", api_style_distance_json(&theme->outcome.style_distance));
    }
    if (!ok) {
        cJSON_Delete(score);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The construction as the request gave it: {"ideas": {"<idea>": bars}, "extensions": [{"kind", "idea", "bars", "varied"}]}. */
static cJSON *given_construction_json(const PlanConstruction *construction) {
    cJSON *object = cJSON_CreateObject();
    cJSON *ideas = object ? cJSON_AddObjectToObject(object, "ideas") : NULL;
    cJSON *extensions = object ? cJSON_AddArrayToObject(object, "extensions") : NULL;
    bool ok = object && ideas && extensions;
    for (int idea = 0; ok && idea < PLAN_CONSTRUCTION_IDEAS; idea++) {
        ok = construction->idea_bars[idea] == 0 ||
             cJSON_AddNumberToObject(ideas, theme_shape_idea_name((GrowIdea)idea), construction->idea_bars[idea]) != NULL;
    }
    for (size_t k = 0; ok && k < construction->extension_count; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        bool names_idea = extension->kind == PLAN_EXTENSION_REPEAT || extension->kind == PLAN_EXTENSION_STRETCH;
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(extensions, item) && cJSON_AddStringToObject(item, "kind", EXTENSION_KINDS[extension->kind]) &&
             cJSON_AddItemToObject(item, "idea",
                                   names_idea ? cJSON_CreateString(theme_shape_idea_name((GrowIdea)extension->idea)) : cJSON_CreateNull()) &&
             cJSON_AddNumberToObject(item, "bars", extension->bars) && cJSON_AddBoolToObject(item, "varied", extension->varied);
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_grow_result_json(const GrowSetup *setup, const GrowRun *run, const GrowThemeInfo *info) {
    const GrowBriefText *text = setup->text;
    char key[24];
    util_key_name(analysis_key_tonic_pitch_class(&setup->key), setup->key.mode == KEY_STRUCTURE_MODE_MINOR, key, sizeof key);
    char pairing[64];
    snprintf(pairing, sizeof(pairing), "%s + %s", BEGINNING_NAMES[setup->pairing.beginning],
             ENDING_NAMES[setup->pairing.ending]);
    char meter[16];
    snprintf(meter, sizeof meter, "%d/%d", setup->meter.beats_per_bar, setup->meter.beat_unit);
    Rational bar;
    rational_make(setup->meter.beats_per_bar, (long long)setup->meter.beat_unit, &bar);
    cJSON *root = cJSON_CreateObject();
    bool ok = root && cJSON_AddStringToObject(root, "key", key) && cJSON_AddStringToObject(root, "meter", meter) &&
              cJSON_AddNumberToObject(root, "bars", (double)setup->bars) &&
              cJSON_AddStringToObject(root, "into", text->into) && cJSON_AddStringToObject(root, "pairing", pairing) &&
              cJSON_AddStringToObject(root, "cadence", plan_cadence_name(setup->cadence));
    if (ok && setup->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        ok = cJSON_AddStringToObject(root, "first_cadence", plan_cadence_name(setup->first_cadence)) != NULL;
    }
    if (ok && setup->has_construction) {
        ok = cJSON_AddItemToObject(root, "construction", given_construction_json(&setup->construction));
    }
    if (ok && setup->has_accompaniment) {
        ok = cJSON_AddStringToObject(root, "accompaniment", api_accompaniment_name(setup->accompaniment.pattern)) != NULL;
        if (ok && setup->accompaniment.texture >= 0) {
            ok = cJSON_AddStringToObject(root, "texture", standard_texture_name((StandardTexture)setup->accompaniment.texture)) !=
                 NULL;
        }
    }
    if (ok && setup->has_voicing) {
        ok = cJSON_AddStringToObject(root, "voicing", setup->voicing.write_soprano ? "chorale" : "satb") != NULL;
    }
    if (ok && setup->seeded) {
        ok = cJSON_AddItemToObject(root, "source", api_seed_json(&setup->seed_found));
    }
    cJSON *tempo = ok ? cJSON_CreateObject() : NULL;
    ok = tempo && cJSON_AddItemToObject(tempo, "qpm", setup->tempo.denominator > 0 ? ratio_number(&setup->tempo) : cJSON_CreateNull()) &&
         cJSON_AddStringToObject(tempo, "from", setup->tempo_from) && cJSON_AddItemToObject(root, "tempo", tempo);
    if (!ok) {
        if (tempo && !cJSON_GetObjectItem(root, "tempo")) {
            cJSON_Delete(tempo);
        }
        cJSON_Delete(root);
        return NULL;
    }
    if (info) {
        GrowBrief brief;
        api_grow_brief(setup, &brief);
        ok = cJSON_AddItemToObject(root, "brief", api_brief_json(&brief));
        /* The chords the request gave, as read: [{"bar", "text"}]. */
        cJSON *chords = ok ? cJSON_CreateArray() : NULL;
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, info->chords) {
            cJSON *item = cJSON_CreateObject();
            if (!item || !cJSON_AddNumberToObject(item, "bar", cJSON_GetObjectItemCaseSensitive(entry, "bar")->valuedouble) ||
                !cJSON_AddStringToObject(item, "text", cJSON_GetObjectItemCaseSensitive(entry, "text")->valuestring) ||
                !cJSON_AddItemToArray(chords, item)) {
                cJSON_Delete(item);
                ok = false;
                break;
            }
        }
        if (!ok || !chords || !cJSON_AddItemToObject(root, "chords", chords)) {
            if (chords && !cJSON_GetObjectItem(root, "chords")) {
                cJSON_Delete(chords);
            }
            cJSON_Delete(root);
            return NULL;
        }
    }
    cJSON *themes = cJSON_AddArrayToObject(root, "themes");
    for (size_t k = 0; themes && k < run->made; k++) {
        if (!cJSON_AddItemToArray(themes, theme_json(setup, &run->themes[k], info, bar))) {
            themes = NULL;
        }
    }
    if (!themes) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
