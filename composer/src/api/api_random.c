#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "api_internal.h"
#include "structure/request_parse.h"
#include "util/util.h"

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
static UrsatzStatus read_options(const cJSON *request, const KnowledgeSnapshot *knowledge,
                                   MotiveRandomOptions *out,
                                   bool *accompaniment, bool *has_role, const char **drawn_mode, const char **drawn_meter,
                                   ComposerPreferences *preferences, bool *has_preferences, UrsatzError *error) {
    memset(out, 0, sizeof *out);
    out->knowledge = knowledge;
    *drawn_mode = NULL;
    *drawn_meter = NULL;
    unsigned long count = RANDOM_MOTIVE_COUNT_DEFAULT;
    unsigned long seed = 0;
    bool has_seed = false;
    if (!api_member_whole(request, "count", 1, MOTIVE_RANDOM_MAX_COUNT, &count, NULL, error) ||
        !api_member_whole(request, "seed", 0, 4294967295UL, &seed, &has_seed, error)) {
        return URSATZ_ERR_REQUEST;
    }
    out->count = count;
    out->seed = has_seed ? seed : api_default_seed();
    const char *mode_text = NULL;
    const char *material_member = NULL;
    const char *meter_text = NULL;
    const char *key_text = NULL;
    const char *form_text = NULL;
    const char *strategy_text = NULL;
    const char *role_text = NULL;
    bool present = false;
    bool has_meter = false;
    if (!api_read_material(request, NULL, true, &mode_text, &material_member, error) ||
        !api_member_string(request, "meter", &meter_text, &has_meter, error) ||
        !api_member_string(request, "key", &key_text, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool has_key = present;
    if (!has_meter) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/meter", "meter is required, as in 4/4 or 6/8");
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
    {
        char member_path[32];
        ApiMaterialKind kind;
        snprintf(member_path, sizeof member_path, "/%s", material_member);
        UrsatzStatus resolved = api_resolve_material(knowledge, material_member, member_path, mode_text, 0, &out->material,
                                                       &kind, error);
        if (resolved != URSATZ_OK) {
            return resolved;
        }
    }
    if (!request_parse_meter(meter_text, &out->meter)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/meter",
                        "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8", meter_text);
    }
    if (has_key) {
        if (!request_parse_key(key_text, knowledge, &out->key)) {
            if (api_fail_unsupported_key(error, "/key", knowledge, key_text)) {
                return URSATZ_ERR_REQUEST;
            }
            return api_fail(error, URSATZ_ERR_REQUEST, "/key",
                            "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                            "for example \"D minor\"",
                            key_text);
        }
        if (!composer_material_same_mode(out->key.material, out->material)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/key", "key '%s' is not %s, the mode asked for", key_text,
                            mode_text);
        }
        out->has_key = true;
    }
    out->form = MOTIVE_SKETCH_FORM_FULL;
    if (!api_member_string(request, "form", &form_text, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (present && !motive_form_parse(form_text, &out->form)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/form", "form must be full, rhythm or contour, not '%s'", form_text);
    }
    out->strategy = MOTIVE_RANDOM_STRATEGY_SEGMENTS;
    if (!api_member_string(request, "strategy", &strategy_text, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (present && !motive_random_strategy_parse(strategy_text, &out->strategy)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/strategy", "strategy must be segments or drawn, not '%s'",
                        strategy_text);
    }
    if (!api_member_string(request, "role", &role_text, has_role, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (*has_role && !motive_role_parse(role_text, accompaniment)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/role", "role must be melodic or accompaniment, not '%s'", role_text);
    }
    /* The motif group steers the segments strategy; the drawn strategy keeps its constants (R-304). */
    if (!api_read_preferences(request, preferences, has_preferences, error)) {
        return URSATZ_ERR_REQUEST;
    }
    out->preferences = *has_preferences ? &preferences->motif : NULL;
    /* The candidates group (R-673): variety 0, the default, draws as it always did. */
    out->variety = *has_preferences ? preferences->candidates.variety : 0;
    out->pool = *has_preferences ? preferences->candidates.pool : 4;
    return URSATZ_OK;
}

/* The style of "style", without the categories of "style_without". NULL for none. */
static UrsatzStatus read_style(const cJSON *request, ComposerStyle **style, UrsatzError *error) {
    *style = NULL;
    const char *path = NULL;
    bool has_style = false;
    if (!api_member_string(request, "style", &path, &has_style, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const char *without[STYLE_WITHOUT_MAX];
    size_t without_count = 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(request, "style_without");
    if (list && !cJSON_IsNull(list)) {
        if (!cJSON_IsArray(list)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
        }
        if (cJSON_GetArraySize(list) > STYLE_WITHOUT_MAX) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without has at most %d categories",
                            STYLE_WITHOUT_MAX);
        }
        for (int i = 0; i < cJSON_GetArraySize(list); i++) {
            const cJSON *item = cJSON_GetArrayItem(list, i);
            if (!cJSON_IsString(item)) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
            }
            without[without_count++] = item->valuestring;
        }
    }
    if (without_count > 0 && !has_style) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without needs style");
    }
    return has_style ? api_load_style(path, without, without_count, "/style", "/style_without", style, error)
                     : URSATZ_OK;
}

/* The most motives a request may ask to avoid. */
#define AVOID_MAX 64

/*
 * "avoid" (R-673): 0 to AVOID_MAX motive documents, read like any motive in the JSON format; a bad one is refused at
 * /avoid/<i> with the reader's message. *out is allocated (free it) when the list is not empty, else NULL.
 */
static UrsatzStatus read_avoid(const cJSON *request, const KnowledgeSnapshot *knowledge, Motive **out, size_t *count,
                               UrsatzError *error) {
    *out = NULL;
    *count = 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(request, "avoid");
    if (!list || cJSON_IsNull(list)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsArray(list)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/avoid", "avoid must be a list of motives");
    }
    int size = cJSON_GetArraySize(list);
    if (size > AVOID_MAX) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/avoid", "avoid has at most %d motives", AVOID_MAX);
    }
    if (size == 0) {
        return URSATZ_OK;
    }
    Motive *motives = calloc((size_t)size, sizeof(*motives));
    if (!motives) {
        return api_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    for (int i = 0; i < size; i++) {
        char *text = cJSON_PrintUnformatted(cJSON_GetArrayItem(list, i));
        MotiveOptions options;
        MotiveError reason;
        memset(&options, 0, sizeof options);
        memset(&reason, 0, sizeof reason);
        options.knowledge = knowledge;
        MotiveResult result = text ? motive_from_json(text, &options, &motives[i], &reason) : MOTIVE_ERR_SYNTAX;
        cJSON_free(text);
        if (result != MOTIVE_OK) {
            char path[32];
            snprintf(path, sizeof path, "/avoid/%d", i);
            free(motives);
            if (reason.location[0]) {
                return api_fail(error, URSATZ_ERR_REQUEST, path, "%s (%s)", reason.message, reason.location);
            }
            return api_fail(error, URSATZ_ERR_REQUEST, path, "%s", reason.message);
        }
    }
    *out = motives;
    *count = (size_t)size;
    return URSATZ_OK;
}

/* {"seed", "motives": [<motive document>]}. NULL on failure. */
static cJSON *random_document(unsigned long seed, const Motive *motives, size_t count, const char *drawn_material,
                              const char *drawn_meter, const ComposerPreferences *preferences) {
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
    cJSON *drawn = drawn_material || drawn_meter ? cJSON_AddObjectToObject(document, "drawn") : NULL;
    if ((drawn_material || drawn_meter) &&
        (!drawn || (drawn_material && !cJSON_AddStringToObject(drawn, "material", drawn_material)) ||
         (drawn_meter && !cJSON_AddStringToObject(drawn, "meter", drawn_meter)))) {
        cJSON_Delete(document);
        return NULL;
    }
    /* "preferences" echoes what the request gave, in full, and is there only when the request had it. */
    if (preferences && !cJSON_AddItemToObject(document, "preferences", composer_preferences_write(preferences))) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

static UrsatzStatus make_random(const cJSON *request, const KnowledgeSnapshot *knowledge, const UrsatzJob *job,
                                  ComposerResponse *out) {
    UrsatzError *error = &out->error;
    MotiveRandomOptions options;
    bool accompaniment = false;
    bool has_role = false;
    const char *drawn_mode = NULL;
    const char *drawn_meter = NULL;
    ComposerPreferences preferences;
    bool has_preferences = false;
    UrsatzStatus status = read_options(request, knowledge, &options, &accompaniment, &has_role, &drawn_mode, &drawn_meter,
                                         &preferences, &has_preferences, error);
    if (status != URSATZ_OK) {
        return status;
    }
    Motive *avoid = NULL;
    status = read_avoid(request, knowledge, &avoid, &options.avoid_count, error);
    if (status != URSATZ_OK) {
        return status;
    }
    options.avoid = avoid;
    ComposerStyle *style = NULL;
    status = read_style(request, &style, error);
    if (status != URSATZ_OK) {
        free(avoid);
        return status;
    }
    if (job_step(job, "random-motive", 0, 1)) {
        composer_style_destroy(style);
        free(avoid);
        return api_fail(error, URSATZ_ERR_CANCELLED, "", "cancelled");
    }
    options.style = style;
    Motive motives[MOTIVE_RANDOM_MAX_COUNT];
    size_t made = 0;
    MotiveRandomResult result = motive_random(&options, motives, &made);
    composer_style_destroy(style);
    free(avoid);
    if (result == MOTIVE_RANDOM_ERR_PLAN) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/preferences/motif/plan",
                        "the motif plan does not fit the meter, bars, shapes or endings allowed");
    }
    if (result != MOTIVE_RANDOM_OK) {
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "", "cannot make random motives (%s)",
                        motive_random_result_name(result));
    }
    for (size_t i = 0; has_role && i < made; i++) {
        motives[i].accompaniment = accompaniment;
    }
    cJSON *document = random_document(options.seed, motives, made, drawn_mode ? options.material->id : NULL, drawn_meter, has_preferences ? &preferences : NULL);
    if (!document) {
        return api_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, document);
}

UrsatzStatus api_random_validate(const KnowledgeSnapshot *knowledge, const cJSON *request, cJSON *filled,
                                   UrsatzError *error) {
    MotiveRandomOptions options;
    bool accompaniment = false;
    bool has_role = false;
    const char *drawn_mode = NULL;
    const char *drawn_meter = NULL;
    ComposerPreferences preferences;
    bool has_preferences = false;
    UrsatzStatus status = read_options(request, knowledge, &options, &accompaniment, &has_role, &drawn_mode, &drawn_meter,
                                         &preferences, &has_preferences, error);
    ComposerStyle *style = NULL;
    if (status == URSATZ_OK) {
        Motive *avoid = NULL;
        size_t avoid_count = 0;
        status = read_avoid(request, knowledge, &avoid, &avoid_count, error);
        free(avoid);
    }
    if (status == URSATZ_OK) {
        status = read_style(request, &style, error);
        composer_style_destroy(style);
    }
    if (status != URSATZ_OK) {
        return status;
    }
    /* The seed the draws were made from is the request's own, or the one read from the clock. */
    cJSON_DeleteItemFromObjectCaseSensitive(filled, "seed");
    bool ok = cJSON_AddNumberToObject(filled, "seed", (double)options.seed) != NULL;
    if (drawn_mode) {
        /* The drawn word goes in the member the request wrote, so the filled request never holds both. */
        const char *member = cJSON_GetObjectItemCaseSensitive(filled, "material") ? "material" : "mode";
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "mode");
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "material");
        ok = ok && cJSON_AddStringToObject(filled, member, drawn_mode);
    }
    if (drawn_meter) {
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "meter");
        ok = ok && cJSON_AddStringToObject(filled, "meter", drawn_meter);
    }
    return ok ? URSATZ_OK : api_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

UrsatzStatus composer_api_random_motive(const ComposerContext *context, const char *request_json,
                                          const UrsatzJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    UrsatzStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != URSATZ_OK) {
        return context_status;
    }
    cJSON *request = NULL;
    UrsatzStatus status = api_parse_request(request_json, &request, &out->error);
    if (status != URSATZ_OK) {
        return status;
    }
    status = make_random(request, context->knowledge, job, out);
    cJSON_Delete(request);
    if (status != URSATZ_OK) {
        UrsatzError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
