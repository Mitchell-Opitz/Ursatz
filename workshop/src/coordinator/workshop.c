#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_composer/composer_api.h>

#include <common/files.h>
#include <common/json.h>

#include "workshop_internal.h"

#include <limits.h>

UrsatzStatus workshop_open(const WorkshopConfig *resolved, Workshop **out, UrsatzError *error) {
    *out = NULL;
    if (!resolved || !resolved->data_dir || resolved->data_dir[0] == '\0' || resolved->port <= 0 || resolved->http_threads <= 0 ||
        resolved->job_workers <= 0) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the settings are not resolved: every member must be set");
    }
    if (strlen(resolved->data_dir) >= sizeof(((Workshop *)0)->data_dir)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the data folder path is too long");
    }
    UrsatzStatus made = common_dir_make(resolved->data_dir, error);
    if (made != URSATZ_OK) {
        return made;
    }
    int lock = -1;
    UrsatzStatus locked = workshop_data_lock_acquire(resolved->data_dir, &lock, error);
    if (locked != URSATZ_OK) {
        return locked;
    }
    if (!workshop_knowledge_init()) {
        workshop_data_lock_release(lock);
        return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the knowledge cannot be read");
    }
    Workshop *workshop = calloc(1, sizeof(*workshop));
    if (!workshop) {
        workshop_knowledge_shutdown();
        workshop_data_lock_release(lock);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    workshop->lock_descriptor = lock;
    /* The folder as an absolute path: a service that changes its working directory still finds it. */
    char absolute[PATH_MAX];
    const char *folder = realpath(resolved->data_dir, absolute) ? absolute : resolved->data_dir;
    if (strlen(folder) >= sizeof(workshop->data_dir)) {
        free(workshop);
        workshop_knowledge_shutdown();
        workshop_data_lock_release(lock);
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the data folder path is too long");
    }
    memcpy(workshop->data_dir, folder, strlen(folder) + 1);
    workshop->port = resolved->port;
    workshop->http_threads = resolved->http_threads;
    workshop->job_workers = resolved->job_workers;
    *out = workshop;
    return URSATZ_OK;
}

void workshop_close(Workshop *workshop) {
    if (workshop) {
        workshop_knowledge_shutdown();
        workshop_data_lock_release(workshop->lock_descriptor);
    }
    free(workshop);
}

const KnowledgeSnapshot *workshop_knowledge(const Workshop *workshop) {
    (void)workshop;
    return workshop_knowledge_snapshot();
}

const ComposerContext *workshop_composer_context(const Workshop *workshop) {
    (void)workshop;
    return workshop_composer_context_global();
}

UrsatzStatus workshop_status(Workshop *workshop, cJSON **out, UrsatzError *error) {
    *out = NULL;
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_versions(&response);
    cJSON *versions = status == URSATZ_OK && response.json ? cJSON_Parse(response.json) : NULL;
    composer_response_destroy(&response);
    if (!versions) {
        return ursatz_error_set(error, status != URSATZ_OK ? status : URSATZ_ERR_INTERNAL, "", "the Composer gave no versions");
    }
    static const char *const kFromComposer[] = {"composer_version", "api_version", "producer_key", "composition_key",
                                                "plan_format_version"};
    cJSON *object = cJSON_CreateObject();
    bool ok = object != NULL;
    for (size_t i = 0; ok && i < sizeof(kFromComposer) / sizeof(kFromComposer[0]); i++) {
        ok = common_json_add_copy_or_null(object, versions, kFromComposer[i]);
    }
    cJSON_Delete(versions);
    ok = ok && cJSON_AddStringToObject(object, "analyzer_version", analyzer_version()) &&
         cJSON_AddStringToObject(object, "analyzer_producer_key", analyzer_producer_key()) &&
         cJSON_AddStringToObject(object, "data_dir", workshop->data_dir);
    if (!ok) {
        cJSON_Delete(object);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = object;
    return URSATZ_OK;
}

const char *workshop_data_dir(const Workshop *workshop) {
    return workshop->data_dir;
}

int workshop_http_threads(const Workshop *workshop) {
    return workshop->http_threads;
}

int workshop_job_workers(const Workshop *workshop) {
    return workshop->job_workers;
}

int workshop_port(const Workshop *workshop) {
    return workshop->port;
}
