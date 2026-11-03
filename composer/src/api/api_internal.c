#include "api_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "util/request_parse.h"

ComposerStatus api_fail(ComposerError *error, ComposerStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s", path ? path : "");
    }
    return status;
}

ComposerStatus api_parse_request(const char *text, cJSON **out, ComposerError *error) {
    *out = NULL;
    if (!text) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request is missing");
    }
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request is not valid JSON");
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request must be a JSON object");
    }
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "api_version");
    if (version && (!cJSON_IsNumber(version) || version->valuedouble != (double)COMPOSER_API_VERSION_MAJOR)) {
        cJSON_Delete(root);
        return api_fail(error, COMPOSER_ERR_REQUEST, "/api_version", "api_version must be %d", COMPOSER_API_VERSION_MAJOR);
    }
    *out = root;
    return COMPOSER_OK;
}

ComposerStatus api_respond_json(ComposerResponse *out, cJSON *value) {
    char *text = value ? cJSON_PrintUnformatted(value) : NULL;
    cJSON_Delete(value);
    if (!text) {
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    out->json = text;
    return COMPOSER_OK;
}

bool composer_job_step(const ComposerJob *job, const char *stage, unsigned done, unsigned total) {
    if (!job) {
        return false;
    }
    if (job->progress) {
        job->progress(job->user, stage, done, total);
    }
    return job->cancelled ? job->cancelled(job->user) : false;
}

/* The member, or NULL (with *present false) when it is absent or null. */
static const cJSON *member(const cJSON *object, const char *name, bool *present) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    bool found = item && !cJSON_IsNull(item);
    if (present) {
        *present = found;
    }
    return found ? item : NULL;
}

static bool member_fail(ComposerError *error, const char *name, const char *what) {
    char path[sizeof error->path];
    snprintf(path, sizeof path, "/%s", name);
    api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be %s", name, what);
    return false;
}

bool api_member_string(const cJSON *object, const char *name, const char **out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsString(item)) {
        return member_fail(error, name, "a string");
    }
    *out = item->valuestring;
    return true;
}

bool api_member_bool(const cJSON *object, const char *name, bool *out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        return member_fail(error, name, "true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool api_member_int(const cJSON *object, const char *name, long minimum, long maximum, long *out, bool *present,
                    ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble || item->valuedouble < (double)minimum ||
        item->valuedouble > (double)maximum) {
        char what[64];
        snprintf(what, sizeof what, "a whole number from %ld to %ld", minimum, maximum);
        return member_fail(error, name, what);
    }
    *out = (long)item->valuedouble;
    return true;
}

bool api_member_rational(const cJSON *object, const char *name, ComposerRatio *out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 4294967295.0 &&
               item->valuedouble == (double)(unsigned long)item->valuedouble) {
        snprintf(text, sizeof text, "%lu", (unsigned long)item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_ratio(source, out)) {
        return member_fail(error, name, "a whole number or a fraction like \"3/2\"");
    }
    return true;
}

bool api_member_key(const cJSON *object, const char *name, AnalysisKey *out, bool *present, ComposerError *error) {
    const char *text = NULL;
    bool found = false;
    if (!api_member_string(object, name, &text, &found, error)) {
        return false;
    }
    if (present) {
        *present = found;
    }
    return !found || request_parse_key(text, out) || member_fail(error, name, "a key like \"D minor\"");
}

bool api_member_meter(const cJSON *object, const char *name, PlanMeter *out, bool *present, ComposerError *error) {
    const char *text = NULL;
    bool found = false;
    if (!api_member_string(object, name, &text, &found, error)) {
        return false;
    }
    if (present) {
        *present = found;
    }
    return !found || request_parse_meter(text, out) || member_fail(error, name, "a meter like \"3/4\"");
}

bool api_member_cadence(const cJSON *object, const char *name, CadenceType *out, bool *present, ComposerError *error) {
    const char *text = NULL;
    bool found = false;
    if (!api_member_string(object, name, &text, &found, error)) {
        return false;
    }
    if (present) {
        *present = found;
    }
    return !found || request_parse_cadence(text, out) || member_fail(error, name, "PAC, IAC, HC, DC or PC");
}

bool api_member_pickup(const cJSON *object, const char *name, int beat_unit, Rational *out, bool *present,
                       ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item)) {
        snprintf(text, sizeof text, "%g", item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_pickup(source, beat_unit, out)) {
        return member_fail(error, name, "a number of beats like 1 or \"1/2\"");
    }
    return true;
}

ComposerStatus api_read_tempo(const cJSON *request, PlanExportOptions *options, ComposerError *error) {
    ComposerRatio tempo;
    bool has_tempo = false;
    if (!api_member_rational(request, "tempo", &tempo, &has_tempo, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (has_tempo && (tempo.denominator == 0 || tempo.numerator < 20UL * tempo.denominator ||
                      tempo.numerator > 400UL * tempo.denominator)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/tempo",
                        "tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like 195/2");
    }
    if (has_tempo) {
        options->tempo_numerator = tempo.numerator;
        options->tempo_denominator = tempo.denominator;
    }
    return COMPOSER_OK;
}
