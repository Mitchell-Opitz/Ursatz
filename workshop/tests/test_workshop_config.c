#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>

#include <ursatz_workshop/workshop_api.h>

static char g_root[64];

static void put(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static void clear_environment(void) {
    unsetenv("URSATZ_DATA");
    unsetenv("URSATZ_PORT");
    unsetenv("URSATZ_JOB_WORKERS");
}

static char *folder(const char *name) {
    static char path[4][96];
    static int next = 0;
    char *out = path[next++ % 4];
    assert(common_path_join(out, 96, g_root, name));
    assert(common_dir_make(out, NULL) == URSATZ_OK);
    return out;
}

static void write_config(const char *dir, const char *body) {
    char path[160];
    assert(common_path_join(path, sizeof(path), dir, "ursatz.json"));
    put(path, body);
}

static void test_defaults(void) {
    clear_environment();
    WorkshopConfig out;
    char storage[512];
    UrsatzError error;
    assert(workshop_config_resolve(NULL, &out, storage, sizeof(storage), &error) == URSATZ_OK);
    assert(strcmp(out.data_dir, ".") == 0 && out.port == 8765 && out.http_threads == 16 && out.job_workers == 2);
    assert(strcmp(out.config_path, "./ursatz.json") == 0);
}

static void test_each_layer_overrides_the_one_before(void) {
    clear_environment();
    char *dir = folder("layers");
    write_config(dir, "{\"format\":\"ursatz-config\",\"format_version\":1,\"port\":9001,\"http_threads\":8,\"job_workers\":3}");
    WorkshopConfig flags = {dir, NULL, 0, 0, 0};
    WorkshopConfig out;
    char storage[512];
    UrsatzError error;
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_OK);
    assert(out.port == 9001 && out.http_threads == 8 && out.job_workers == 3);

    setenv("URSATZ_PORT", "9002", 1);
    setenv("URSATZ_JOB_WORKERS", "5", 1);
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_OK);
    assert(out.port == 9002 && out.http_threads == 8 && out.job_workers == 5);

    flags.port = 9003;
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_OK);
    assert(out.port == 9003 && out.job_workers == 5);

    setenv("URSATZ_DATA", dir, 1);
    flags.data_dir = NULL;
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_OK);
    assert(strcmp(out.data_dir, dir) == 0 && out.port == 9003);
    clear_environment();
}

static void test_bad_values_are_refused(void) {
    clear_environment();
    char *dir = folder("bad");
    WorkshopConfig flags = {dir, NULL, 0, 0, 0};
    WorkshopConfig out;
    char storage[512];
    UrsatzError error;

    write_config(dir, "{\"format\":\"ursatz-config\",\"format_version\":1,\"http_threads\":0}");
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_FORMAT);
    assert(strstr(error.message, "http_threads") && strcmp(error.path, "/http_threads") == 0);

    write_config(dir, "{\"format\":\"ursatz-config\",\"format_version\":1,\"port\":70000}");
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_FORMAT);
    assert(strstr(error.message, "port"));

    write_config(dir, "{\"format\":\"other\",\"format_version\":1}");
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_FORMAT);

    write_config(dir, "{\"format\":\"ursatz-config\",\"format_version\":2}");
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_FORMAT);
    assert(strstr(error.message, "format_version"));

    write_config(dir, "{\"format\":\"ursatz-config\",\"format_version\":1}");
    setenv("URSATZ_PORT", "abc", 1);
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_REQUEST);
    assert(strstr(error.message, "URSATZ_PORT"));
    clear_environment();

    flags.job_workers = 9;
    assert(workshop_config_resolve(&flags, &out, storage, sizeof(storage), &error) == URSATZ_ERR_REQUEST);
    assert(strstr(error.message, "job_workers"));
    flags.job_workers = 0;

    assert(workshop_config_resolve(&flags, &out, storage, 4, &error) == URSATZ_ERR_REQUEST);
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "workshop-settings-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    test_defaults();
    test_each_layer_overrides_the_one_before();
    test_bad_values_are_refused();
    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    return 0;
}
