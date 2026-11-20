#include <stdlib.h>
#include <string.h>

#include <chain/analysis_chain.h>
#include <ursatz_composer/composer.h>
#include <ursatz_composer/composer_api.h>
#include <plan/plan_json.h>

#include "api_internal.h"

const char *composer_api_version(void) {
#define API_STR2(x) #x
#define API_STR(x) API_STR2(x)
    return API_STR(COMPOSER_API_VERSION_MAJOR) "." API_STR(COMPOSER_API_VERSION_MINOR);
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

UrsatzStatus composer_api_versions(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "composer_version", composer_version()) ||
        !cJSON_AddStringToObject(root, "api_version", composer_api_version()) ||
        !cJSON_AddStringToObject(root, "producer_key", analysis_chain_producer_key()) ||
        !cJSON_AddStringToObject(root, "composition_key", composer_composition_key()) ||
        !cJSON_AddNumberToObject(root, "plan_format_version", URSATZ_PLAN_FORMAT_VERSION)) {
        cJSON_Delete(root);
        return api_fail(&out->error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, root);
}
