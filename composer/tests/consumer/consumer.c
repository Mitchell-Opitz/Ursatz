#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/composer_api.h>

/* Links the Composer as a subproject and asks it for its versions and its vocabulary, nothing more. The vocabulary takes a
   call context: the knowledge snapshot of the embedded entries. */

int main(void) {
    if (strcmp(composer_api_version(), "4.5") != 0) {
        fprintf(stderr, "consumer: API version %s, expected 4.5\n", composer_api_version());
        return 1;
    }
    KnowledgeSnapshot *knowledge = NULL;
    char message[256];
    if (knowledge_snapshot_build(NULL, 0, &knowledge, message, sizeof message) != KNOWLEDGE_OK) {
        fprintf(stderr, "consumer: knowledge failed: %s\n", message);
        return 1;
    }
    const ComposerContext context = {knowledge};
    ComposerResponse response;
    if (composer_api_versions(&response) != URSATZ_OK || !response.json) {
        fprintf(stderr, "consumer: versions failed: %s\n", response.error.message);
        return 1;
    }
    composer_response_destroy(&response);
    if (composer_api_vocabulary(&context, &response) != URSATZ_OK || !response.json) {
        fprintf(stderr, "consumer: vocabulary failed: %s\n", response.error.message);
        return 1;
    }
    cJSON *vocabulary = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    const cJSON *bars = vocabulary ? cJSON_GetObjectItemCaseSensitive(vocabulary, "theme_bars") : NULL;
    int ok = cJSON_IsArray(bars) && cJSON_GetArraySize(bars) == 14;
    cJSON_Delete(vocabulary);
    knowledge_snapshot_destroy(knowledge);
    if (!ok) {
        fprintf(stderr, "consumer: the vocabulary has no theme_bars\n");
        return 1;
    }
    printf("consumer: ok\n");
    return 0;
}
