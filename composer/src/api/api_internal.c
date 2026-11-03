#include "api_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
