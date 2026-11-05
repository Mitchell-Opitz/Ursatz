#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>

/* Links the Composer as a subproject and asks it for its versions and its vocabulary, nothing more. */

int main(void) {
    if (strcmp(composer_api_version(), "1.2") != 0) {
        fprintf(stderr, "consumer: API version %s, expected 1.2\n", composer_api_version());
        return 1;
    }
    ComposerResponse response;
    if (composer_api_versions(&response) != COMPOSER_OK || !response.json) {
        fprintf(stderr, "consumer: versions failed: %s\n", response.error.message);
        return 1;
    }
    composer_response_destroy(&response);
    if (composer_api_vocabulary(&response) != COMPOSER_OK || !response.json) {
        fprintf(stderr, "consumer: vocabulary failed: %s\n", response.error.message);
        return 1;
    }
    cJSON *vocabulary = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    const cJSON *bars = vocabulary ? cJSON_GetObjectItemCaseSensitive(vocabulary, "theme_bars") : NULL;
    int ok = cJSON_IsArray(bars) && cJSON_GetArraySize(bars) == 2;
    cJSON_Delete(vocabulary);
    if (!ok) {
        fprintf(stderr, "consumer: the vocabulary has no theme_bars\n");
        return 1;
    }
    printf("consumer: ok\n");
    return 0;
}
