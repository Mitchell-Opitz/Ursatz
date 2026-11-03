#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "api_internal.h"
#include "util/request_parse.h"

/* Random motives (docs/api.md): the request of the `random-motive` command. */

#define RANDOM_MOTIVE_COUNT_DEFAULT 1
#define STYLE_WITHOUT_MAX 8

/*
 * What a seed draws when the request leaves the mode or the meter to chance ("random"): the rule the GUI used before the
 * Composer drew (materials-model.js randomSettings, R-269). The seed is mixed, ((seed + 1) * 2654435761) mod 2^32; the
 * mode is major when bit 8 is clear and minor when it is set, and the meter is one of RANDOM_METERS by bits 16 and 17.
 */
static const char *const RANDOM_METERS[] = {"4/4", "3/4", "2/4", "6/8"};

static unsigned long mix_seed(unsigned long seed) {
    return (unsigned long)((((unsigned long long)seed + 1ULL) * 2654435761ULL) & 0xFFFFFFFFULL);
}

/* Reads the members that make the options, without the style. drawn_mode and drawn_meter are set to what was drawn. */
static ComposerStatus read_options(const cJSON *request, MotiveRandomOptions *out, bool *accompaniment, bool *has_role,
                                   const char **drawn_mode, const char **drawn_meter, ComposerError *error) {
    memset(out, 0, sizeof *out);
    *drawn_mode = NULL;
    *drawn_meter = NULL;
    unsigned long count = RANDOM_MOTIVE_COUNT_DEFAULT;
    unsigned long seed = 0;
    bool has_seed = false;
    if (!api_member_whole(request, "count", 1, MOTIVE_RANDOM_MAX_COUNT, &count, NULL, error) ||
        !api_member_whole(request, "seed", 0, 4294967295UL, &seed, &has_seed, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    out->count = count;
    out->seed = has_seed ? seed : api_default_seed();
    const char *mode_text = NULL;
    const char *meter_text = NULL;
    const char *key_text = NULL;
    const char *form_text = NULL;
    const char *role_text = NULL;
    bool present = false;
    bool has_mode = false;
    bool has_meter = false;
    if (!api_member_string(request, "mode", &mode_text, &has_mode, error) ||
        !api_member_string(request, "meter", &meter_text, &has_meter, error) ||
        !api_member_string(request, "key", &key_text, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    bool has_key = present;
    if (!has_mode) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode is required: major or minor");
    }
    if (!has_meter) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/meter", "meter is required, as in 4/4 or 6/8");
    }
    unsigned long mixed = mix_seed(out->seed);
    if (request_equals_ignore_case(mode_text, "random")) {
        mode_text = ((mixed >> 8) % 2) == 0 ? "major" : "minor";
        *drawn_mode = mode_text;
    }
    if (request_equals_ignore_case(meter_text, "random")) {
        meter_text = RANDOM_METERS[(mixed >> 16) % 4];
        *drawn_meter = meter_text;
    }
    if (request_equals_ignore_case(mode_text, "major")) {
        out->mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (request_equals_ignore_case(mode_text, "minor")) {
        out->mode = KEY_STRUCTURE_MODE_MINOR;
    } else {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode must be major or minor, not '%s'", mode_text);
    }
    if (!motive_parse_meter(meter_text, &out->meter)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/meter",
                        "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8", meter_text);
    }
    if (has_key) {
        if (!motive_parse_key(key_text, &out->key)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                            "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                            "for example \"D minor\"",
                            key_text);
        }
        if (out->key.mode != out->mode) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/key", "key '%s' is not %s, the mode asked for", key_text,
                            mode_text);
        }
        out->has_key = true;
    }
    out->form = MOTIVE_SKETCH_FORM_FULL;
    if (!api_member_string(request, "form", &form_text, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (present && !motive_form_parse(form_text, &out->form)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/form", "form must be full, rhythm or contour, not '%s'", form_text);
    }
    if (!api_member_string(request, "role", &role_text, has_role, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (*has_role && !motive_role_parse(role_text, accompaniment)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/role", "role must be melodic or accompaniment, not '%s'", role_text);
    }
    return COMPOSER_OK;
}

/* The style of "style", without the categories of "style_without". NULL for none. */
static ComposerStatus read_style(const cJSON *request, ComposerStyle **style, ComposerError *error) {
    *style = NULL;
    const char *path = NULL;
    bool has_style = false;
    if (!api_member_string(request, "style", &path, &has_style, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    const char *without[STYLE_WITHOUT_MAX];
    size_t without_count = 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(request, "style_without");
    if (list && !cJSON_IsNull(list)) {
        if (!cJSON_IsArray(list)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
        }
        if (cJSON_GetArraySize(list) > STYLE_WITHOUT_MAX) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without has at most %d categories",
                            STYLE_WITHOUT_MAX);
        }
        for (int i = 0; i < cJSON_GetArraySize(list); i++) {
            const cJSON *item = cJSON_GetArrayItem(list, i);
            if (!cJSON_IsString(item)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
            }
            without[without_count++] = item->valuestring;
        }
    }
    if (without_count > 0 && !has_style) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without needs style");
    }
    return has_style ? api_load_style(path, without, without_count, "/style", "/style_without", style, error)
                     : COMPOSER_OK;
}

/* {"seed", "motives": [<motive document>]}. NULL on failure. */
static cJSON *random_document(unsigned long seed, const Motive *motives, size_t count, const char *drawn_mode,
                              const char *drawn_meter) {
    cJSON *document = cJSON_CreateObject();
    cJSON *array = document && cJSON_AddNumberToObject(document, "seed", (double)seed)
                       ? cJSON_AddArrayToObject(document, "motives")
                       : NULL;
    if (!array) {
        cJSON_Delete(document);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        MotiveAnalysis analysis;
        char *text = motive_analyze(&motives[i], &analysis) ? motive_analysis_to_json(&motives[i], &analysis) : NULL;
        cJSON *item = text ? cJSON_Parse(text) : NULL;
        free(text);
        if (!item || !cJSON_AddItemToArray(array, item)) {
            cJSON_Delete(item);
            cJSON_Delete(document);
            return NULL;
        }
    }
    /* "drawn" names what the seed drew, and is there only when something was. */
    cJSON *drawn = drawn_mode || drawn_meter ? cJSON_AddObjectToObject(document, "drawn") : NULL;
    if ((drawn_mode || drawn_meter) &&
        (!drawn || (drawn_mode && !cJSON_AddStringToObject(drawn, "mode", drawn_mode)) ||
         (drawn_meter && !cJSON_AddStringToObject(drawn, "meter", drawn_meter)))) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

static ComposerStatus make_random(const cJSON *request, const ComposerJob *job, ComposerResponse *out) {
    ComposerError *error = &out->error;
    MotiveRandomOptions options;
    bool accompaniment = false;
    bool has_role = false;
    const char *drawn_mode = NULL;
    const char *drawn_meter = NULL;
    ComposerStatus status = read_options(request, &options, &accompaniment, &has_role, &drawn_mode, &drawn_meter, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    ComposerStyle *style = NULL;
    status = read_style(request, &style, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (composer_job_step(job, "random-motive", 0, 1)) {
        composer_style_destroy(style);
        return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
    }
    options.style = style;
    Motive motives[MOTIVE_RANDOM_MAX_COUNT];
    size_t made = 0;
    MotiveRandomResult result = motive_random(&options, motives, &made);
    composer_style_destroy(style);
    if (result != MOTIVE_RANDOM_OK) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot make random motives (%s)",
                        motive_random_result_name(result));
    }
    for (size_t i = 0; has_role && i < made; i++) {
        motives[i].accompaniment = accompaniment;
    }
    cJSON *document = random_document(options.seed, motives, made, drawn_mode, drawn_meter);
    if (!document) {
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, document);
}

ComposerStatus api_random_validate(const cJSON *request, cJSON *filled, ComposerError *error) {
    MotiveRandomOptions options;
    bool accompaniment = false;
    bool has_role = false;
    const char *drawn_mode = NULL;
    const char *drawn_meter = NULL;
    ComposerStatus status = read_options(request, &options, &accompaniment, &has_role, &drawn_mode, &drawn_meter, error);
    ComposerStyle *style = NULL;
    if (status == COMPOSER_OK) {
        status = read_style(request, &style, error);
        composer_style_destroy(style);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    /* The seed the draws were made from is the request's own, or the one read from the clock. */
    cJSON_DeleteItemFromObjectCaseSensitive(filled, "seed");
    bool ok = cJSON_AddNumberToObject(filled, "seed", (double)options.seed) != NULL;
    if (drawn_mode) {
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "mode");
        ok = ok && cJSON_AddStringToObject(filled, "mode", drawn_mode);
    }
    if (drawn_meter) {
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "meter");
        ok = ok && cJSON_AddStringToObject(filled, "meter", drawn_meter);
    }
    return ok ? COMPOSER_OK : api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

ComposerStatus composer_api_random_motive(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    status = make_random(request, job, out);
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
