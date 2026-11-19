#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>

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
    assert(workshop_settings_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
    assert(common_path_is_dir(data));
    assert(workshop_port(workshop) == 9123 && workshop_http_threads(workshop) == 3 && workshop_job_workers(workshop) == 4);
    assert(strstr(workshop_data_dir(workshop), "fresh/data"));
    cJSON *status = NULL;
    assert(workshop_status(workshop, &status, &error) == URSATZ_OK);
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "composer_version")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "api_version")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "producer_key")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "analyzer_version")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "data_dir")));
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
    assert(workshop_settings_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
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
