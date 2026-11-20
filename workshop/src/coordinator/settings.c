#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>

#include <ursatz_workshop/workshop_api.h>

#define DEFAULT_DATA_DIR "."
#define DEFAULT_PORT 8765
#define DEFAULT_HTTP_THREADS 16
#define DEFAULT_JOB_WORKERS 2
#define CONFIG_FORMAT "ursatz-config"
#define CONFIG_FORMAT_VERSION 1
#define CONFIG_NAME "ursatz.json"

typedef struct {
    const char *name;
    int min;
    int max;
} Limit;

static const Limit kPort = {"port", 1, 65535};
static const Limit kHttpThreads = {"http_threads", 1, 64};
static const Limit kJobWorkers = {"job_workers", 1, 8};

/* A whole number in text, the whole text. */
static bool parse_whole(const char *text, long long *out) {
    char *end = NULL;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno != 0) {
        return false;
    }
    *out = value;
    return true;
}

static bool in_limit(const Limit *limit, long long value) {
    return value >= limit->min && value <= limit->max;
}

/* The settings file: a member of the file that is not right is FORMAT, naming the file and the member. */
static UrsatzStatus apply_file(const char *path, WorkshopConfig *config, UrsatzError *error) {
    if (!common_path_exists(path)) {
        return URSATZ_OK;
    }
    cJSON *root = NULL;
    UrsatzStatus status = common_json_read_file(path, &root, error);
    if (status != URSATZ_OK) {
        return status == URSATZ_ERR_NOT_FOUND ? URSATZ_OK : status;
    }
    if (!cJSON_IsObject(root) || strcmp(common_json_string(root, "format"), CONFIG_FORMAT) != 0) {
        cJSON_Delete(root);
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "%s is not a settings file: its format is not \"%s\"", path,
                                CONFIG_FORMAT);
    }
    long long version = 0;
    if (!common_json_long(root, "format_version", &version) || version < 1 || version > CONFIG_FORMAT_VERSION) {
        cJSON_Delete(root);
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "%s: format_version must be %d (this build reads no other)", path,
                                CONFIG_FORMAT_VERSION);
    }
    const struct {
        const Limit *limit;
        int *target;
    } members[] = {{&kPort, &config->port}, {&kHttpThreads, &config->http_threads}, {&kJobWorkers, &config->job_workers}};
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        const Limit *limit = members[i].limit;
        if (!cJSON_GetObjectItemCaseSensitive(root, limit->name)) {
            continue;
        }
        long long value = 0;
        if (!common_json_long(root, limit->name, &value) || !in_limit(limit, value)) {
            char pointer[64];
            snprintf(pointer, sizeof(pointer), "/%s", limit->name);
            status = ursatz_error_set(error, URSATZ_ERR_FORMAT, pointer, "%s: %s must be a whole number from %d to %d", path,
                                      limit->name, limit->min, limit->max);
            cJSON_Delete(root);
            return status;
        }
        *members[i].target = (int)value;
    }
    cJSON_Delete(root);
    return URSATZ_OK;
}

static UrsatzStatus apply_environment(WorkshopConfig *config, UrsatzError *error) {
    const struct {
        const char *variable;
        const Limit *limit;
        int *target;
    } variables[] = {{"URSATZ_PORT", &kPort, &config->port}, {"URSATZ_JOB_WORKERS", &kJobWorkers, &config->job_workers}};
    for (size_t i = 0; i < sizeof(variables) / sizeof(variables[0]); i++) {
        const char *text = getenv(variables[i].variable);
        if (!text || text[0] == '\0') {
            continue;
        }
        long long value = 0;
        if (!parse_whole(text, &value) || !in_limit(variables[i].limit, value)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "%s must be a whole number from %d to %d, not \"%s\"",
                                    variables[i].variable, variables[i].limit->min, variables[i].limit->max, text);
        }
        *variables[i].target = (int)value;
    }
    return URSATZ_OK;
}

static UrsatzStatus apply_flags(const WorkshopConfig *flags, WorkshopConfig *config, UrsatzError *error) {
    const struct {
        const Limit *limit;
        int value;
        int *target;
    } members[] = {{&kPort, flags->port, &config->port},
                   {&kHttpThreads, flags->http_threads, &config->http_threads},
                   {&kJobWorkers, flags->job_workers, &config->job_workers}};
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        if (members[i].value == 0) {
            continue;
        }
        if (!in_limit(members[i].limit, members[i].value)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "%s must be a whole number from %d to %d, not %d",
                                    members[i].limit->name, members[i].limit->min, members[i].limit->max, members[i].value);
        }
        *members[i].target = members[i].value;
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_settings_resolve(const WorkshopConfig *flags, WorkshopConfig *out, char *storage, size_t storage_size,
                                       UrsatzError *error) {
    WorkshopConfig none;
    memset(&none, 0, sizeof(none));
    if (!flags) {
        flags = &none;
    }
    WorkshopConfig config = {DEFAULT_DATA_DIR, NULL, DEFAULT_PORT, DEFAULT_HTTP_THREADS, DEFAULT_JOB_WORKERS};

    /* The data folder comes first: the settings file lies in it. */
    const char *environment_data = getenv("URSATZ_DATA");
    if (environment_data && environment_data[0] != '\0') {
        config.data_dir = environment_data;
    }
    if (flags->data_dir && flags->data_dir[0] != '\0') {
        config.data_dir = flags->data_dir;
    }
    char default_config[1024];
    if (flags->config_path && flags->config_path[0] != '\0') {
        config.config_path = flags->config_path;
    } else if (common_path_join(default_config, sizeof(default_config), config.data_dir, CONFIG_NAME)) {
        config.config_path = default_config;
    } else {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the data folder path is too long");
    }

    UrsatzStatus status = apply_file(config.config_path, &config, error);
    if (status == URSATZ_OK) {
        status = apply_environment(&config, error);
    }
    if (status == URSATZ_OK) {
        status = apply_flags(flags, &config, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }

    size_t data_size = strlen(config.data_dir) + 1;
    size_t config_size = strlen(config.config_path) + 1;
    if (!storage || data_size + config_size > storage_size) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the settings paths do not fit the storage given");
    }
    memcpy(storage, config.data_dir, data_size);
    memcpy(storage + data_size, config.config_path, config_size);
    config.data_dir = storage;
    config.config_path = storage + data_size;
    *out = config;
    return URSATZ_OK;
}
