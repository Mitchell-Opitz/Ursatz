#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>

#include <cJSON.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_workshop/workshop_api.h>

static char g_root[64];

static void test_open_creates_the_folder_and_reports_status(void) {
    char data[96];
    assert(common_path_join(data, sizeof(data), g_root, "fresh/data"));
    assert(!common_path_exists(data));
    WorkshopConfig config = {data, NULL, 9123, 3, 4};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    assert(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
    assert(common_path_is_dir(data));
    assert(workshop_port(workshop) == 9123 && workshop_http_threads(workshop) == 3 && workshop_job_workers(workshop) == 4);
    assert(strstr(workshop_data_dir(workshop), "fresh/data"));
    cJSON *status = NULL;
    assert(workshop_status(workshop, &status, &error) == URSATZ_OK);
    /* Each member is its source's own: the Analyzer's version and producer key, the Composer's three, and the folder the workshop opened. */
    ComposerResponse versions;
    assert(composer_api_versions(&versions) == URSATZ_OK && versions.json);
    cJSON *composer = cJSON_Parse(versions.json);
    assert(composer);
    static const char *const kFromComposer[] = {"composer_version", "api_version", "producer_key"};
    for (size_t i = 0; i < sizeof(kFromComposer) / sizeof(kFromComposer[0]); i++) {
        const cJSON *got = cJSON_GetObjectItemCaseSensitive(status, kFromComposer[i]);
        const cJSON *want = cJSON_GetObjectItemCaseSensitive(composer, kFromComposer[i]);
        assert(cJSON_IsString(got) && cJSON_IsString(want) && strcmp(got->valuestring, want->valuestring) == 0);
    }
    cJSON_Delete(composer);
    composer_response_destroy(&versions);
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(status, "analyzer_version");
    const cJSON *analyzer_key = cJSON_GetObjectItemCaseSensitive(status, "analyzer_producer_key");
    assert(cJSON_IsString(analyzer) && strcmp(analyzer->valuestring, analyzer_version()) == 0);
    assert(cJSON_IsString(analyzer_key) && strcmp(analyzer_key->valuestring, analyzer_producer_key()) == 0);
    const cJSON *data_dir = cJSON_GetObjectItemCaseSensitive(status, "data_dir");
    assert(cJSON_IsString(data_dir) && strcmp(data_dir->valuestring, workshop_data_dir(workshop)) == 0);
    cJSON_Delete(status);
    workshop_close(workshop);
    workshop_close(NULL);
}

static void test_open_on_an_existing_folder_twice_in_turn(void) {
    char data[96];
    assert(common_path_join(data, sizeof(data), g_root, "again"));
    WorkshopConfig config = {data, NULL, 0, 0, 0};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    assert(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    for (int i = 0; i < 2; i++) {
        Workshop *workshop = NULL;
        assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
        assert(workshop_port(workshop) == 8765 && workshop_http_threads(workshop) == 16 && workshop_job_workers(workshop) == 2);
        workshop_close(workshop);
    }
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "workshop-open-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    test_open_creates_the_folder_and_reports_status();
    test_open_on_an_existing_folder_twice_in_turn();
    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    return 0;
}
