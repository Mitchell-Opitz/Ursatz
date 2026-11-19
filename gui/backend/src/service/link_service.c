#include "link_service.h"
#include "last_error.h"
#include "util/store_lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>

#include <cJSON.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include "analysis_service.h"
#include "collection_service.h"
#include "id_service.h"
#include "workshop_internal.h"
#include "material_service.h"
#include "piece_metadata.h"
#include "subject_service.h"

static const char *kPiecesDir = "pieces";

/* --- id lists ------------------------------------------------------------- */

void ursatz_gui_link_ids_free(char **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

/* Adds a copy of id unless the list has it. false only when memory ran out. */
static bool id_list_add(CommonNames *list, const char *id) {
    return common_names_contains(list, id) || common_names_add(list, id);
}

/* Adds every string of a JSON array. */
static bool id_list_add_array(CommonNames *list, const cJSON *array) {
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (cJSON_IsString(item) && !id_list_add(list, item->valuestring)) {
            return false;
        }
    }
    return true;
}

/* --- scopes --------------------------------------------------------------- */

static bool text_given(const char *text) {
    return text && text[0] != '\0';
}

static void free_summaries(UrsatzGuiPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

/* Every Library piece, or those that match the filter (NULL for all). */
static UrsatzStatus add_library_pieces(CommonNames *ids, const UrsatzGuiMetaFilter *filter) {
    UrsatzGuiPieceSummary *pieces = NULL;
    size_t count = 0;
    if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_OK) {
        return ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
        if ((!filter || ursatz_gui_meta_matches(&pieces[i], filter)) && !id_list_add(ids, pieces[i].id)) {
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    free_summaries(pieces, count);
    return result;
}

static UrsatzStatus add_collection_pieces(CommonNames *ids, const char *collection_id) {
    if (!text_given(collection_id) || !ursatz_gui_collection_is_valid_id(collection_id)) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    UrsatzGuiCollection *collection = NULL;
    UrsatzStatus result = ursatz_gui_collection_get(collection_id, &collection);
    if (result == URSATZ_ERR_NOT_FOUND) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_ALLOC ? ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory") : ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    bool ok = true;
    for (size_t i = 0; ok && i < collection->piece_count; i++) {
        ok = id_list_add(ids, collection->pieces[i]);
    }
    ursatz_gui_collection_destroy(collection);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
}

static UrsatzStatus add_subject_pieces(CommonNames *ids, const char *subject_id) {
    if (!text_given(subject_id)) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    cJSON *subject = NULL;
    UrsatzStatus result = ursatz_gui_subject_get(subject_id, &subject);
    if (result != URSATZ_OK) {
        return result;
    }
    bool ok = id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"));
    cJSON_Delete(subject);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
}

/* True when any of the subject's items is one of the project's. */
static bool subject_links_item_of(const cJSON *subject, const CommonNames *item_ids) {
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(subject, "items")) {
        if (cJSON_IsString(item) && common_names_contains(item_ids, item->valuestring)) {
            return true;
        }
    }
    return false;
}

/* The Library pieces linked to every subject that links at least one item of the project. */
static UrsatzStatus add_project_pieces(CommonNames *ids, const char *project_id) {
    if (!text_given(project_id)) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    cJSON *project = NULL;
    UrsatzStatus found = ursatz_gui_material_project_get(project_id, &project);
    if (found == URSATZ_ERR_REQUEST) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    if (found == URSATZ_ERR_NOT_FOUND) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (found != URSATZ_OK) {
        return found == URSATZ_ERR_ALLOC ? ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory") : ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    cJSON_Delete(project);

    cJSON *items = NULL;
    found = ursatz_gui_material_item_list(project_id, NULL, &items);
    if (found != URSATZ_OK) {
        return found == URSATZ_ERR_ALLOC ? ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory") : ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    CommonNames item_ids = {0};
    const cJSON *item = NULL;
    bool ok = true;
    cJSON_ArrayForEach(item, items) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) && !id_list_add(&item_ids, id->valuestring)) {
            ok = false;
            break;
        }
    }
    cJSON_Delete(items);
    if (!ok) {
        common_names_free(&item_ids);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    UrsatzStatus result = URSATZ_OK;
    cJSON *subjects = NULL;
    if (item_ids.count > 0) {
        result = ursatz_gui_subject_list(&subjects);
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, subjects) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
        if (result != URSATZ_OK || !cJSON_IsString(id)) {
            break;
        }
        /* The list entries carry counts, not the links: read the subject itself. */
        cJSON *subject = NULL;
        UrsatzStatus got = ursatz_gui_subject_get(id->valuestring, &subject);
        if (got == URSATZ_ERR_NOT_FOUND) {
            continue;
        }
        result = got;
        if (result == URSATZ_OK && subject_links_item_of(subject, &item_ids) &&
            !id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"))) {
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        cJSON_Delete(subject);
    }
    cJSON_Delete(subjects);
    common_names_free(&item_ids);
    return result;
}

UrsatzStatus ursatz_gui_link_scope_resolve(const UrsatzGuiLinkScope *scope, char ***out_ids,
                                                  size_t *out_count) {
    if (!scope || !out_ids || !out_count) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    *out_ids = NULL;
    *out_count = 0;

    CommonNames ids = {0};
    UrsatzStatus result = URSATZ_OK;
    switch (scope->kind) {
        case URSATZ_GUI_LINK_SCOPE_ALL:
            result = add_library_pieces(&ids, NULL);
            break;
        case URSATZ_GUI_LINK_SCOPE_COLLECTION:
            result = add_collection_pieces(&ids, scope->collection_id);
            break;
        case URSATZ_GUI_LINK_SCOPE_FILTER: {
            UrsatzGuiMetaFilter filter = {0};
            filter.composer = text_given(scope->composer) ? scope->composer : NULL;
            filter.source = text_given(scope->source) ? scope->source : NULL;
            filter.tag = text_given(scope->tag) ? scope->tag : NULL;
            result = filter.composer || filter.source || filter.tag ? add_library_pieces(&ids, &filter)
                                                                     : ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
            break;
        }
        case URSATZ_GUI_LINK_SCOPE_SUBJECT:
            result = add_subject_pieces(&ids, scope->subject_id);
            break;
        case URSATZ_GUI_LINK_SCOPE_PROJECT:
            result = add_project_pieces(&ids, scope->project_id);
            break;
        default:
            result = ursatz_gui_fail(URSATZ_ERR_REQUEST, "invalid scope");
            break;
    }
    if (result == URSATZ_OK && ids.count == 0) {
        result = ursatz_gui_fail(URSATZ_ERR_UNSATISFIABLE, "the scope names no pieces");
    }
    if (result != URSATZ_OK) {
        common_names_free(&ids);
        return result;
    }
    common_names_sort(&ids);
    *out_ids = ids.items;
    *out_count = ids.count;
    return URSATZ_OK;
}

/* --- files ---------------------------------------------------------------- */

#define PATH_BYTES 400

/* UTC with milliseconds, so two runs in the same second still order. */
static void iso_utc_now(char *out, size_t out_size) {
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == 0) {
        now.tv_sec = time(NULL);
        now.tv_nsec = 0;
    }
    time_t seconds = now.tv_sec;
    struct tm parts;
    gmtime_r(&seconds, &parts);
    char base[32];
    strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &parts);
    snprintf(out, out_size, "%s.%03ldZ", base, (long)(now.tv_nsec / 1000000));
}

/* Replaces the member key of object with item (adds it when missing). Deletes item on failure. */
static bool set_member(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    bool ok = cJSON_HasObjectItem(object, key) ? cJSON_ReplaceItemInObjectCaseSensitive(object, key, item)
                                               : cJSON_AddItemToObject(object, key, item);
    if (!ok) {
        cJSON_Delete(item);
    }
    return ok;
}

static cJSON *string_or_null(const char *text) {
    return text_given(text) ? cJSON_CreateString(text) : cJSON_CreateNull();
}

/* --- store state ---------------------------------------------------------- */

static const char *kCacheFile = "motive-link-cache.json";

static UrsatzGuiLock g_lock = URSATZ_GUI_LOCK_INIT;
#define LOCK() ursatz_gui_lock_acquire(&g_lock)
#define UNLOCK() ursatz_gui_lock_release(&g_lock)

static struct {
    char dir[PATH_BYTES];
    bool ready;
    bool active; /* a worker is running */
    char id[64]; /* the active run */
    size_t done;
    size_t total;
} g_state;

static bool runs_dir_path(char *out, size_t out_size) {
    return snprintf(out, out_size, "%s/runs", g_state.dir) < (int)out_size;
}

static bool run_dir_path(const char *id, char *out, size_t out_size) {
    return ursatz_gui_collection_is_valid_id(id) && snprintf(out, out_size, "%s/runs/%s", g_state.dir, id) < (int)out_size;
}

static bool run_file_path(const char *id, char *out, size_t out_size) {
    return ursatz_gui_collection_is_valid_id(id) &&
           snprintf(out, out_size, "%s/runs/%s/run.json", g_state.dir, id) < (int)out_size;
}

/* The names in the runs directory. false only when memory ran out. */
static bool list_run_dirs(CommonNames *names) {
    char runs[PATH_BYTES];
    if (!runs_dir_path(runs, sizeof(runs))) {
        return false;
    }
    CommonNames listing = {0};
    UrsatzStatus status = common_dir_list(runs, &listing, NULL);
    if (status != URSATZ_OK) {
        return status != URSATZ_ERR_ALLOC;
    }
    bool ok = true;
    for (size_t i = 0; ok && i < listing.count; i++) {
        ok = id_list_add(names, listing.items[i]);
    }
    common_names_free(&listing);
    return ok;
}

/* --- scope as JSON -------------------------------------------------------- */

static const char *scope_kind_label(UrsatzGuiLinkScopeKind kind) {
    switch (kind) {
        case URSATZ_GUI_LINK_SCOPE_COLLECTION:
            return "collection";
        case URSATZ_GUI_LINK_SCOPE_FILTER:
            return "filter";
        case URSATZ_GUI_LINK_SCOPE_SUBJECT:
            return "subject";
        case URSATZ_GUI_LINK_SCOPE_PROJECT:
            return "project";
        case URSATZ_GUI_LINK_SCOPE_ALL:
        default:
            return "all";
    }
}

/* {"kind","collection_id","filter":{"composer","source","tag"},"subject_id","project_id"}: what the kind does not use
 * is null, so two runs over the same scope have the same text. */
static cJSON *scope_json(const UrsatzGuiLinkScope *scope) {
    cJSON *object = cJSON_CreateObject();
    cJSON *filter = NULL;
    bool is_filter = scope->kind == URSATZ_GUI_LINK_SCOPE_FILTER;
    if (is_filter) {
        filter = cJSON_CreateObject();
        if (!filter || !cJSON_AddItemToObject(filter, "composer", string_or_null(scope->composer)) ||
            !cJSON_AddItemToObject(filter, "source", string_or_null(scope->source)) ||
            !cJSON_AddItemToObject(filter, "tag", string_or_null(scope->tag))) {
            cJSON_Delete(filter);
            cJSON_Delete(object);
            return NULL;
        }
    } else {
        filter = cJSON_CreateNull();
    }
    if (!object || !filter || !cJSON_AddStringToObject(object, "kind", scope_kind_label(scope->kind)) ||
        !cJSON_AddItemToObject(object, "collection_id",
                               scope->kind == URSATZ_GUI_LINK_SCOPE_COLLECTION ? string_or_null(scope->collection_id)
                                                                               : cJSON_CreateNull())) {
        cJSON_Delete(filter);
        cJSON_Delete(object);
        return NULL;
    }
    cJSON_AddItemToObject(object, "filter", filter);
    if (!cJSON_AddItemToObject(object, "subject_id",
                               scope->kind == URSATZ_GUI_LINK_SCOPE_SUBJECT ? string_or_null(scope->subject_id)
                                                                            : cJSON_CreateNull()) ||
        !cJSON_AddItemToObject(object, "project_id",
                               scope->kind == URSATZ_GUI_LINK_SCOPE_PROJECT ? string_or_null(scope->project_id)
                                                                            : cJSON_CreateNull())) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* --- a run ---------------------------------------------------------------- */

typedef struct {
    char id[64];
    cJSON *run;  /* the run.json object, owned by the worker */
    char **ids;  /* the scope's piece ids */
    size_t count;
    time_t last_write;
} Job;

static void job_free(Job *job) {
    cJSON_Delete(job->run);
    ursatz_gui_link_ids_free(job->ids, job->count);
    free(job);
}

static bool job_write(const Job *job) {
    char path[PATH_BYTES];
    char *text = run_file_path(job->id, path, sizeof(path)) ? cJSON_PrintUnformatted(job->run) : NULL;
    bool ok = text && (common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    return ok;
}

static bool job_set_progress(Job *job, size_t done, size_t total) {
    return set_member(job->run, "done", cJSON_CreateNumber((double)done)) &&
           set_member(job->run, "total", cJSON_CreateNumber((double)total));
}

/* Called by the Analyzer when the pairs are known and after each pair. run.json follows at most once a second. */
static void job_progress(size_t done, size_t total, void *context) {
    Job *job = context;
    LOCK();
    g_state.done = done;
    g_state.total = total;
    UNLOCK();
    time_t now = time(NULL);
    if (now != job->last_write && job_set_progress(job, done, total)) {
        job->last_write = now;
        job_write(job);
    }
}

/* Why a piece cannot be added, or NULL when it was. */
static const char *add_piece(MotiveLinkScope *scope, const char *id) {
    if (!ursatz_gui_collection_is_valid_id(id)) {
        return "not found";
    }
    /* Another schema, Analyzer version or Ursatz commit: analyze the piece again before the run. The error-triggered
       re-analysis below stays as a fallback. */
    UrsatzStatus ensured = ursatz_gui_analysis_ensure_current(kPiecesDir, id);
    if (ensured != URSATZ_OK) {
        return ensured == URSATZ_ERR_NOT_FOUND ? "not found" : "re-analysis failed";
    }

    for (int attempt = 0; attempt < 2; attempt++) {
        char *document = NULL;
        if (ursatz_gui_analysis_get_document(kPiecesDir, id, NULL, 0, &document) != URSATZ_OK) {
            return "unreadable analysis";
        }
        UrsatzStatus result = analyzer_links_scope_add_document(scope, id, document);
        free(document);
        if (result == URSATZ_OK) {
            return NULL;
        }
        /* A document from before the features (or another schema version): analyze the piece again, once. */
        bool stale = result == URSATZ_ERR_UNSATISFIABLE || result == URSATZ_ERR_FORMAT;
        if (!stale || attempt > 0) {
            return "unreadable analysis";
        }
        if (ursatz_gui_analysis_reanalyze(kPiecesDir, id) != URSATZ_OK) {
            return "re-analysis failed";
        }
    }
    return "unreadable analysis";
}

static bool skipped_add(cJSON *skipped, const char *id, const char *reason) {
    cJSON *item = cJSON_CreateObject();
    if (!item || !cJSON_AddStringToObject(item, "id", id) || !cJSON_AddStringToObject(item, "reason", reason)) {
        cJSON_Delete(item);
        return false;
    }
    cJSON_AddItemToArray(skipped, item);
    return true;
}

/* Marks the run ended: status, message, finished_at, and on done the result. Takes result. */
static void job_finish(Job *job, const char *status, const char *message, cJSON *result) {
    char now[40];
    iso_utc_now(now, sizeof(now));
    set_member(job->run, "status", cJSON_CreateString(status));
    set_member(job->run, "message", cJSON_CreateString(message));
    set_member(job->run, "finished_at", cJSON_CreateString(now));
    if (result) {
        set_member(job->run, "result", result);
    }
    job_write(job);
}

static void job_fail(Job *job, const char *message) {
    job_finish(job, "failed", message, NULL);
}

/* The run itself, on the worker thread. */
static void job_run(Job *job) {
    cJSON *skipped = cJSON_GetObjectItemCaseSensitive(job->run, "skipped");
    MotiveLinkScope *scope = analyzer_links_scope_create(workshop_knowledge(NULL));
    if (!scope) {
        job_fail(job, "out of memory");
        return;
    }
    size_t used = 0;
    bool ok = true;
    for (size_t i = 0; ok && i < job->count; i++) {
        const char *reason = add_piece(scope, job->ids[i]);
        if (!reason) {
            used++;
        } else {
            ok = skipped_add(skipped, job->ids[i], reason);
        }
    }
    if (!ok) {
        analyzer_links_scope_destroy(scope);
        job_fail(job, "out of memory");
        return;
    }
    if (used == 0) {
        analyzer_links_scope_destroy(scope);
        job_fail(job, "no analyzable pieces");
        return;
    }

    char cache_path[PATH_BYTES + 64];
    snprintf(cache_path, sizeof(cache_path), "%s/%s", g_state.dir, kCacheFile);
    MotiveLinkCache *cache = NULL;
    if (analyzer_links_cache_load(cache_path, &cache) != URSATZ_OK) {
        /* An unreadable cache is only a cache: start it again. */
        remove(cache_path);
        cache = NULL;
        if (analyzer_links_cache_load(cache_path, &cache) != URSATZ_OK) {
            analyzer_links_scope_destroy(scope);
            job_fail(job, "cache unreadable");
            return;
        }
    }

    MotiveLinksOptions options;
    analyzer_links_options_default(&options);
    options.progress = job_progress;
    options.progress_context = job;

    MotiveLinks links;
    memset(&links, 0, sizeof(links));
    UrsatzStatus run = analyzer_links_run(scope, cache, &options, &links);
    char *rendered = NULL;
    if (run == URSATZ_OK) {
        /* A cache that cannot be saved costs only time on the next run. */
        analyzer_links_cache_save(cache, cache_path);
        run = analyzer_links_render(&links, &rendered);
    }
    size_t pairs = links.pairs;
    if (run == URSATZ_OK) {
        analyzer_links_destroy(&links);
    }
    analyzer_links_cache_destroy(cache);
    analyzer_links_scope_destroy(scope);

    cJSON *result = rendered ? cJSON_Parse(rendered) : NULL;
    free(rendered);
    if (run != URSATZ_OK || !result) {
        cJSON_Delete(result);
        job_fail(job, "linking failed");
        return;
    }
    job_set_progress(job, pairs, pairs);
    job_finish(job, "done", "", result);
}

static void job_main(Job *job) {
    job_run(job);
    LOCK();
    g_state.active = false;
    g_state.id[0] = '\0';
    UNLOCK();
    job_free(job);
}

static void *job_entry(void *argument) {
    job_main(argument);
    return NULL;
}

static bool job_spawn(Job *job) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, job_entry, job) != 0) {
        return false;
    }
    pthread_detach(thread);
    return true;
}

static void sleep_ms(unsigned ms) {
    usleep(ms * 1000u);
}

/* --- service -------------------------------------------------------------- */

/* Marks every run still "running" as failed: the process that ran it is gone. */
static void mark_interrupted(void) {
    CommonNames names = {0};
    if (!list_run_dirs(&names)) {
        return;
    }
    for (size_t i = 0; i < names.count; i++) {
        char path[PATH_BYTES];
        if (!run_file_path(names.items[i], path, sizeof(path))) {
            continue;
        }
        cJSON *run = NULL;
        common_json_read_object(path, &run, NULL);
        const cJSON *status = cJSON_GetObjectItemCaseSensitive(run, "status");
        if (cJSON_IsString(status) && strcmp(status->valuestring, "running") == 0) {
            char now[40];
            iso_utc_now(now, sizeof(now));
            set_member(run, "status", cJSON_CreateString("failed"));
            set_member(run, "message", cJSON_CreateString("interrupted"));
            set_member(run, "finished_at", cJSON_CreateString(now));
            char *text = cJSON_PrintUnformatted(run);
            if (text) {
                (common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
            }
            free(text);
        }
        cJSON_Delete(run);
    }
    common_names_free(&names);
}

bool ursatz_gui_link_service_init(const char *dir) {
    char runs[PATH_BYTES];
    if (!dir || dir[0] == '\0' || snprintf(runs, sizeof(runs), "%s/runs", dir) >= (int)sizeof(runs)) {
        return false;
    }
    LOCK();
    snprintf(g_state.dir, sizeof(g_state.dir), "%s", dir);
    bool ok = (common_dir_make(dir, NULL) == URSATZ_OK) && (common_dir_make(runs, NULL) == URSATZ_OK);
    g_state.ready = ok;
    if (ok) {
        mark_interrupted();
    }
    UNLOCK();
    return ok;
}

void ursatz_gui_link_service_shutdown(void) {
    for (;;) {
        LOCK();
        bool active = g_state.active;
        if (!active) {
            g_state.ready = false;
        }
        UNLOCK();
        if (!active) {
            return;
        }
        sleep_ms(10);
    }
}

UrsatzStatus ursatz_gui_link_run_start(const UrsatzGuiLinkScope *scope, char **out_id) {
    if (out_id) {
        *out_id = NULL;
    }
    char **ids = NULL;
    size_t count = 0;
    UrsatzStatus result = ursatz_gui_link_scope_resolve(scope, &ids, &count);
    if (result != URSATZ_OK) {
        return result;
    }

    char *id = ursatz_gui_new_id("lnk");
    Job *job = id ? calloc(1, sizeof(*job)) : NULL;
    if (!job || strlen(id) >= sizeof(job->id)) {
        free(id);
        free(job);
        ursatz_gui_link_ids_free(ids, count);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    snprintf(job->id, sizeof(job->id), "%s", id);
    free(id);
    job->ids = ids;
    job->count = count;

    char now[40];
    iso_utc_now(now, sizeof(now));
    job->run = cJSON_CreateObject();
    bool built = job->run && cJSON_AddStringToObject(job->run, "id", job->id) &&
                 cJSON_AddItemToObject(job->run, "scope", scope_json(scope)) &&
                 cJSON_AddStringToObject(job->run, "created_at", now) &&
                 cJSON_AddNullToObject(job->run, "finished_at") &&
                 cJSON_AddStringToObject(job->run, "status", "running") &&
                 cJSON_AddStringToObject(job->run, "message", "") && cJSON_AddNumberToObject(job->run, "done", 0) &&
                 cJSON_AddNumberToObject(job->run, "total", 0) && cJSON_AddArrayToObject(job->run, "skipped");
    if (!built) {
        job_free(job);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    /* The worker owns job once it starts and may free it at once: keep the id here. */
    char run_id[sizeof(job->id)];
    snprintf(run_id, sizeof(run_id), "%s", job->id);

    LOCK();
    if (!g_state.ready) {
        result = ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    } else if (g_state.active) {
        result = ursatz_gui_fail(URSATZ_ERR_CONFLICT, "a run is in progress");
    } else {
        char dir[PATH_BYTES];
        result = run_dir_path(job->id, dir, sizeof(dir)) && (common_dir_make(dir, NULL) == URSATZ_OK) && job_write(job) ? URSATZ_OK
                                                                                                 : ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
        if (result == URSATZ_OK) {
            g_state.active = true;
            g_state.done = 0;
            g_state.total = 0;
            snprintf(g_state.id, sizeof(g_state.id), "%s", job->id);
            if (!job_spawn(job)) {
                g_state.active = false;
                g_state.id[0] = '\0';
                result = ursatz_gui_fail(URSATZ_ERR_INTERNAL, "link operation failed");
                if (run_dir_path(job->id, dir, sizeof(dir))) {
                    common_dir_remove(dir, NULL);
                }
            }
        }
    }
    UNLOCK();

    if (result != URSATZ_OK) {
        job_free(job);
        return result;
    }
    if (out_id) {
        *out_id = common_strdup(run_id);
        if (!*out_id) {
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    return URSATZ_OK;
}

/* --- reading runs --------------------------------------------------------- */


/* Newest first: by created_at, then id, both descending. */
static int compare_runs(const void *left, const void *right) {
    const cJSON *a = *(const cJSON *const *)left;
    const cJSON *b = *(const cJSON *const *)right;
    int by_time = strcmp(common_json_string(b, "created_at"), common_json_string(a, "created_at"));
    return by_time != 0 ? by_time : strcmp(common_json_string(b, "id"), common_json_string(a, "id"));
}

static void runs_free(cJSON **runs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        cJSON_Delete(runs[i]);
    }
    free(runs);
}

/* Every readable run.json, newest first. The caller frees with runs_free. */
static UrsatzStatus load_runs(cJSON ***out_runs, size_t *out_count) {
    *out_runs = NULL;
    *out_count = 0;
    CommonNames names = {0};
    if (!list_run_dirs(&names)) {
        common_names_free(&names);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    cJSON **runs = names.count ? calloc(names.count, sizeof(*runs)) : NULL;
    if (names.count && !runs) {
        common_names_free(&names);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    size_t count = 0;
    for (size_t i = 0; i < names.count; i++) {
        char path[PATH_BYTES];
        if (!run_file_path(names.items[i], path, sizeof(path))) {
            continue;
        }
        cJSON *run = NULL;
        common_json_read_object(path, &run, NULL);
        if (run && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(run, "id"))) {
            runs[count++] = run;
        } else {
            cJSON_Delete(run);
        }
    }
    common_names_free(&names);
    qsort(runs, count, sizeof(*runs), compare_runs);
    *out_runs = runs;
    *out_count = count;
    return URSATZ_OK;
}

static bool copy_member(cJSON *to, const cJSON *from, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(from, key);
    return item && cJSON_AddItemToObject(to, key, cJSON_Duplicate(item, 1));
}

UrsatzStatus ursatz_gui_link_run_list(cJSON **out_runs) {
    *out_runs = NULL;
    cJSON **runs = NULL;
    size_t count = 0;
    UrsatzStatus result = load_runs(&runs, &count);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *list = cJSON_CreateArray();
    static const char *kKeys[] = {"id", "scope", "status", "created_at", "finished_at", "done", "total"};
    LOCK();
    for (size_t i = 0; list && i < count; i++) {
        cJSON *entry = cJSON_CreateObject();
        bool ok = entry != NULL;
        for (size_t k = 0; ok && k < sizeof(kKeys) / sizeof(kKeys[0]); k++) {
            ok = copy_member(entry, runs[i], kKeys[k]);
        }
        /* A running run reports what the worker has now, not what it last wrote. */
        if (ok && g_state.active && strcmp(common_json_string(runs[i], "id"), g_state.id) == 0) {
            ok = set_member(entry, "done", cJSON_CreateNumber((double)g_state.done)) &&
                 set_member(entry, "total", cJSON_CreateNumber((double)g_state.total));
        }
        if (ok) {
            cJSON_AddItemToArray(list, entry);
        } else {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            list = NULL;
        }
    }
    UNLOCK();
    runs_free(runs, count);
    if (!list) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_runs = list;
    return URSATZ_OK;
}

UrsatzStatus ursatz_gui_link_run_get(const char *id, cJSON **out_run) {
    *out_run = NULL;
    char path[PATH_BYTES];
    if (!id || !run_file_path(id, path, sizeof(path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON *run = NULL;
    common_json_read_object(path, &run, NULL);
    if (!run) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    LOCK();
    bool live = g_state.active && strcmp(g_state.id, id) == 0;
    size_t done = g_state.done;
    size_t total = g_state.total;
    UNLOCK();
    if (live && (!set_member(run, "done", cJSON_CreateNumber((double)done)) ||
                 !set_member(run, "total", cJSON_CreateNumber((double)total)))) {
        cJSON_Delete(run);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_run = run;
    return URSATZ_OK;
}

UrsatzStatus ursatz_gui_link_run_delete(const char *id) {
    char dir[PATH_BYTES];
    if (!id || !run_dir_path(id, dir, sizeof(dir))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    LOCK();
    UrsatzStatus result = URSATZ_OK;
    if (g_state.active && strcmp(g_state.id, id) == 0) {
        result = ursatz_gui_fail(URSATZ_ERR_CONFLICT, "a run is in progress");
    } else if (!common_path_is_dir(dir)) {
        result = ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, "not found");
    } else {
        common_dir_remove(dir, NULL);
    }
    UNLOCK();
    return result;
}

UrsatzStatus ursatz_gui_link_piece_links(const char *piece_id, cJSON **out_links) {
    *out_links = NULL;
    cJSON **runs = NULL;
    size_t count = 0;
    UrsatzStatus result = load_runs(&runs, &count);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *list = cJSON_CreateArray();
    CommonNames seen = {0}; /* the scopes whose newest done run was taken */
    for (size_t i = 0; list && result == URSATZ_OK && i < count; i++) {
        const cJSON *run = runs[i];
        const cJSON *scope = cJSON_GetObjectItemCaseSensitive(run, "scope");
        const cJSON *links = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(run, "result"), "links");
        if (strcmp(common_json_string(run, "status"), "done") != 0 || !scope) {
            continue;
        }
        char *scope_text = cJSON_PrintUnformatted(scope);
        if (!scope_text) {
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        bool newest = !common_names_contains(&seen, scope_text);
        if (newest && !id_list_add(&seen, scope_text)) {
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        free(scope_text);
        const cJSON *link = NULL;
        const cJSON *taken = newest && result == URSATZ_OK ? links : NULL;
        cJSON_ArrayForEach(link, taken) {
            const char *a = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "a"), "piece");
            const char *b = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "b"), "piece");
            if (strcmp(a, piece_id) != 0 && strcmp(b, piece_id) != 0) {
                continue;
            }
            cJSON *entry = cJSON_CreateObject();
            if (!entry || !cJSON_AddStringToObject(entry, "run", common_json_string(run, "id")) ||
                !cJSON_AddItemToObject(entry, "scope", cJSON_Duplicate(scope, 1)) ||
                !cJSON_AddItemToObject(entry, "link", cJSON_Duplicate(link, 1))) {
                cJSON_Delete(entry);
                result = ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            cJSON_AddItemToArray(list, entry);
        }
    }
    common_names_free(&seen);
    runs_free(runs, count);
    if (!list || result != URSATZ_OK) {
        cJSON_Delete(list);
        return result == URSATZ_OK ? ursatz_gui_fail(URSATZ_ERR_ALLOC, "out of memory") : result;
    }
    *out_links = list;
    return URSATZ_OK;
}

char *ursatz_gui_link_piece_title(const char *piece_id) {
    char *filename = NULL;
    if (!piece_id || !ursatz_gui_collection_is_valid_id(piece_id) ||
        ursatz_gui_analysis_get_filename(kPiecesDir, piece_id, &filename) != URSATZ_OK) {
        filename = NULL;
    }
    char *title = common_strdup(filename && filename[0] ? filename : (piece_id ? piece_id : ""));
    free(filename);
    char *dot = title ? strrchr(title, '.') : NULL;
    if (dot && dot != title) {
        *dot = '\0';
    }
    return title;
}
