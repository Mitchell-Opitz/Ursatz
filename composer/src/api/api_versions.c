#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/version.h>
#include <ursatz_composer/composer.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/plan_json.h>

#include "api_internal.h"

static const char *const STATUS_NAMES[] = {"ok", "request", "unsatisfiable", "cancelled", "alloc", "internal"};

const char *composer_api_version(void) {
#define API_STR2(x) #x
#define API_STR(x) API_STR2(x)
    return API_STR(COMPOSER_API_VERSION_MAJOR) "." API_STR(COMPOSER_API_VERSION_MINOR);
}

const char *composer_status_name(ComposerStatus status) {
    return (unsigned)status < sizeof STATUS_NAMES / sizeof STATUS_NAMES[0] ? STATUS_NAMES[status] : "unknown";
}

void composer_response_destroy(ComposerResponse *response) {
    if (!response) {
        return;
    }
    free(response->json);
    free(response->plan_json);
    free(response->midi);
    free(response->musicxml);
    memset(response, 0, sizeof *response);
}

ComposerStatus composer_api_versions(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "composer_version", composer_version()) ||
        !cJSON_AddStringToObject(root, "api_version", composer_api_version()) ||
        !cJSON_AddStringToObject(root, "analyzer_version", ursatz_analyzer_version()) ||
        !cJSON_AddStringToObject(root, "ursatz_commit", ursatz_analyzer_ursatz_commit()) ||
        !cJSON_AddNumberToObject(root, "plan_format_version", URSATZ_COMPOSER_PLAN_FORMAT_VERSION)) {
        cJSON_Delete(root);
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, root);
}
