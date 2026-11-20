#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_composer/composer_api.h>

#include <common/files.h>
#include <common/json.h>

#include "coordinator.h"
#include "items/migrate.h"
#include "items/recover.h"
#include "items/store.h"
#include "items/subjects.h"
#include "base/events_hub.h"
#include "items/collections.h"
#include "analysis/links.h"
#include "jobs/job_queue.h"
#include <unistd.h>

#include <limits.h>

/* The store's hook for an item an item delete removed: the subjects drop their links to it (items/store.h). */
static void forget_item_in_subjects(WorkshopTxn *txn, const char *item_id) {
    (void)workshop_subject_forget_item(txn, item_id);
}

/* One workshop per process (R-656): the workshops open now. A tools workshop is refused while any is open. */
static int g_open = 0;

UrsatzStatus workshop_tools_open(Workshop **out, UrsatzError *error) {
    *out = NULL;
    if (g_open > 0) {
        return ursatz_error_set(error, URSATZ_ERR_CONFLICT, "", "a workshop is already open in this process");
    }
    if (!workshop_knowledge_init()) {
        return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the knowledge cannot be read");
    }
    Workshop *workshop = calloc(1, sizeof(*workshop));
    if (!workshop) {
        workshop_knowledge_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    workshop->lock_descriptor = -1;
    workshop->tools_only = true;
    g_open++;
    *out = workshop;
    return URSATZ_OK;
}

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
    UrsatzStatus prepared = workshop_data_folder_prepare(resolved->data_dir, error);
    if (prepared != URSATZ_OK) {
        workshop_data_lock_release(lock);
        return prepared;
    }
    /* A stop in the middle of a change is finished or undone before any service starts (items/recover.h). */
    WorkshopRecoverReport recovered;
    UrsatzStatus recovery = workshop_recover(resolved->data_dir, &recovered, error);
    if (recovery != URSATZ_OK) {
        workshop_data_lock_release(lock);
        return recovery;
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
    workshop_recover_summary(&recovered, workshop->recovery, sizeof(workshop->recovery));
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
    workshop_store_set_forget_item(forget_item_in_subjects);
    g_open++;
    *out = workshop;
    return URSATZ_OK;
}

/* The services read and write the data folder by relative paths (items/layout.h), so the process works in it once the workshop runs. */
UrsatzStatus workshop_start(Workshop *workshop, UrsatzError *error) {
    if (!workshop) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "no workshop to start");
    }
    if (workshop->tools_only) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "this workshop has no data folder");
    }
    if (workshop->started) {
        return URSATZ_OK;
    }
    if (chdir(workshop->data_dir) != 0) {
        return ursatz_error_set(error, URSATZ_ERR_IO, workshop->data_dir, "cannot enter the data folder %s", workshop->data_dir);
    }
    /* Collections, materials, subjects and the motive link runs are files the stores prepare once here; long Composer and Analyzer runs go
     * to worker threads, which are up before any query or command. */
    if (!ursatz_gui_collection_service_init()) {
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "the collections cannot be read");
    }
    if (!workshop_material_service_init()) {
        workshop_collection_service_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "the materials cannot be read");
    }
    if (!workshop_subject_service_init()) {
        workshop_material_service_shutdown();
        workshop_collection_service_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "the subjects cannot be read");
    }
    if (!workshop_link_service_init()) {
        workshop_subject_service_shutdown();
        workshop_material_service_shutdown();
        workshop_collection_service_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "the motive link runs cannot be read");
    }
    if (!workshop_jobs_start((size_t)workshop->job_workers)) {
        workshop_link_service_shutdown();
        workshop_subject_service_shutdown();
        workshop_material_service_shutdown();
        workshop_collection_service_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the job workers cannot start");
    }
    CommonEventBus *bus = common_event_bus_create();
    if (!bus) {
        workshop_jobs_stop();
        workshop_link_service_shutdown();
        workshop_subject_service_shutdown();
        workshop_material_service_shutdown();
        workshop_collection_service_shutdown();
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    workshop_events_hub_set(bus);
    workshop->started = true;
    return URSATZ_OK;
}

void workshop_stop(Workshop *workshop) {
    if (!workshop || !workshop->started) {
        return;
    }
    workshop_jobs_stop(); /* cancels what runs and joins the workers, after the last request is answered */
    CommonEventBus *bus = workshop_events_hub();
    workshop_events_hub_set(NULL);
    common_event_bus_destroy(bus);
    workshop_link_service_shutdown();
    workshop_subject_service_shutdown();
    workshop_material_service_shutdown();
    workshop_collection_service_shutdown();
    workshop->started = false;
}

void workshop_close(Workshop *workshop) {
    workshop_stop(workshop);
    if (workshop) {
        if (!workshop->tools_only) {
            workshop_store_set_forget_item(NULL);
        }
        workshop_knowledge_shutdown();
        workshop_data_lock_release(workshop->lock_descriptor); /* -1 for a tools workshop: nothing to release */
        if (g_open > 0) {
            g_open--;
        }
    }
    free(workshop);
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
         (workshop->tools_only ? cJSON_AddNullToObject(object, "data_dir") != NULL
                               : cJSON_AddStringToObject(object, "data_dir", workshop->data_dir) != NULL);
    if (!ok) {
        cJSON_Delete(object);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = object;
    return URSATZ_OK;
}

const char *workshop_recovery_summary(const Workshop *workshop) {
    return workshop->recovery;
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

UrsatzStatus workshop_announce(Workshop *workshop, int port) {
    if (!workshop || workshop->tools_only || port < 1 || port > 65535) {
        return URSATZ_ERR_REQUEST;
    }
    return workshop_data_lock_announce(workshop->lock_descriptor, port, NULL);
}
