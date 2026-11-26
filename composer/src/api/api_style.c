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

#include "api_style.h"

static cJSON *four_places(double value) {
    char text[64];
    snprintf(text, sizeof text, "%.4f", value);
    return cJSON_CreateNumber(strtod(text, NULL));
}

UrsatzStatus api_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                              ComposerStyleWeights *out, UrsatzError *error) {
    if (!blend_path) {
        if (all_text || list_text) {
            return api_fail(error, URSATZ_ERR_REQUEST, all_text ? "/blend_weight" : "/blend_weights",
                            "%s needs blend", all_text ? "blend_weight" : "blend_weights");
        }
        return URSATZ_OK;
    }
    if (!has_style) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/blend", "blend needs style");
    }
    Rational all = {1, 2};
    if (all_text) {
        if (!request_parse_ratio(all_text, &all)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weight", "blend_weight must be a fraction like 1/2, not '%s'",
                            all_text);
        }
        if (all.num > all.den) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weight", "blend_weight must be from 0 to 1, not '%s'",
                            all_text);
        }
    }
    *out = composer_style_weights_all(all);
    if (!list_text) {
        return URSATZ_OK;
    }
    bool seen[STYLE_DIMENSION_COUNT] = {false};
    const char *at = list_text;
    while (*at) {
        const char *end = strchr(at, ',');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        char item[64];
        if (length == 0 || length >= sizeof item) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights", "blend_weights must be like harmony=1/2,form=0, not '%s'",
                            list_text);
        }
        memcpy(item, at, length);
        item[length] = '\0';
        char *equals = strchr(item, '=');
        StyleDimension dimension;
        if (!equals) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights", "blend_weights must be like harmony=1/2,form=0, not '%s'",
                            list_text);
        }
        *equals = '\0';
        if (!style_dimension_from_name(item, &dimension)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights",
                            "blend_weights: '%s' is not a dimension (harmony, rhythm, melody, form)", item);
        }
        if (seen[dimension]) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights", "blend_weights names %s twice", item);
        }
        seen[dimension] = true;
        Rational weight;
        if (!request_parse_ratio(equals + 1, &weight)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights", "blend_weights: %s must be a fraction like 1/2, not '%s'",
                            item, equals + 1);
        }
        if (weight.num > weight.den) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights", "blend_weights: %s must be from 0 to 1, not '%s'", item,
                            equals + 1);
        }
        out->numerator[dimension] = weight.num;
        out->denominator[dimension] = weight.den;
        at += length;
        if (*at == ',') {
            at++;
            if (*at == '\0') {
                return api_fail(error, URSATZ_ERR_REQUEST, "/blend_weights",
                                "blend_weights must be like harmony=1/2,form=0, not '%s'", list_text);
            }
        }
    }
    return URSATZ_OK;
}

UrsatzStatus api_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                              const char *const *without, size_t without_count, ComposerStyle **out,
                              UrsatzError *error) {
    if (!blend_path) {
        return api_load_style(style_path, without, without_count, "/style", "/without", out, error);
    }
    ComposerStyle *first = NULL;
    ComposerStyle *second = NULL;
    UrsatzStatus status = api_load_style(style_path, without, without_count, "/style", "/without", &first, error);
    if (status == URSATZ_OK) {
        status = api_load_style(blend_path, without, without_count, "/blend", "/without", &second, error);
    }
    if (status == URSATZ_OK) {
        ComposerStyleResult blended = composer_style_blend(first, second, weights, out);
        if (blended != COMPOSER_STYLE_OK) {
            status = api_fail(error, blended == COMPOSER_STYLE_ERR_ARGUMENT ? URSATZ_ERR_REQUEST : URSATZ_ERR_UNSATISFIABLE,
                              "/blend", "cannot blend '%s' with '%s': %s", style_path, blend_path,
                              composer_style_result_name(blended));
        }
    }
    composer_style_destroy(first);
    composer_style_destroy(second);
    return status;
}

cJSON *api_style_distance_json(const StyleComparison *comparison) {
    cJSON *object = cJSON_CreateObject();
    for (int d = 0; object && d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d] &&
            !cJSON_AddItemToObject(object, style_dimension_name((StyleDimension)d), four_places(comparison->dimension[d]))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    if (object && comparison->overall_present &&
        !cJSON_AddItemToObject(object, "overall", four_places(comparison->overall))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

UrsatzStatus api_style_category_check(const char *name, UrsatzError *error) {
    StyleDimension dimension;
    if (!style_category_dimension(name, &dimension)) {
        char list[640];
        size_t used = 0;
        list[0] = '\0';
        for (size_t i = 0; i < style_category_count() && used < sizeof list; i++) {
            int written = snprintf(list + used, sizeof list - used, "%s%s", i ? ", " : "", style_category_name(i));
            used += written > 0 ? (size_t)written : 0;
        }
        return api_fail(error, URSATZ_ERR_REQUEST, "/style_without",
                        "style_without: '%s' is not a style category (the categories are %s)", name, list);
    }
    return URSATZ_OK;
}

UrsatzStatus api_style_without_add(const char **names, size_t *count, size_t capacity, const char *name,
                                     UrsatzError *error) {
    UrsatzStatus checked = api_style_category_check(name, error);
    if (checked != URSATZ_OK) {
        return checked;
    }
    for (size_t i = 0; i < *count; i++) {
        if (strcmp(names[i], name) == 0) {
            return URSATZ_OK;
        }
    }
    if (*count >= capacity) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without is given at most %zu times", capacity);
    }
    names[(*count)++] = name;
    return URSATZ_OK;
}


cJSON *api_brief_json(const GrowBrief *brief) {
    cJSON *object = cJSON_CreateObject();
    cJSON *tempo = cJSON_CreateObject();
    cJSON *material = cJSON_CreateObject();
    cJSON *without = cJSON_CreateArray();
    if (!object || !tempo || !material || !without ||
        !cJSON_AddItemToObject(tempo, "qpm", brief->tempo.den > 0 ? api_ratio_number(&brief->tempo) : cJSON_CreateNull()) ||
        !cJSON_AddStringToObject(tempo, "from", brief->tempo_from) ||
        !cJSON_AddStringToObject(material, "material", brief->material ? brief->material->id : "omt.material.major") ||
        !cJSON_AddStringToObject(material, "from", brief->mode_from) || !cJSON_AddItemToObject(object, "tempo", tempo) ||
        !cJSON_AddItemToObject(object, "material", material) ||
        !cJSON_AddItemToObject(object, "density", brief->has_density ? api_ratio_number(&brief->density) : cJSON_CreateNull()) ||
        !cJSON_AddItemToObject(object, "without", without) ||
        (brief->development && !cJSON_AddStringToObject(object, "development", brief->development)) ||
        (brief->preset && !cJSON_AddStringToObject(object, "preset", brief->preset))) {
        /* Whatever was not added to the object is still ours. */
        if (!object || !cJSON_GetObjectItem(object, "tempo")) {
            cJSON_Delete(tempo);
        }
        if (!object || !cJSON_GetObjectItem(object, "material")) {
            cJSON_Delete(material);
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

const char *api_setup_development_name(const GrowSetup *setup) {
    return setup->generate_blueprint ? "blueprint" : api_development_name(setup->development);
}

const char *api_development_name(GrowDevelopment development) {
    return development == GROW_DEVELOP_MELODY ? "melody" : development == GROW_DEVELOP_THEME ? "theme" : "balanced";
}
