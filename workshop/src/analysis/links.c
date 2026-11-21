#include "links.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/txn.h"
#include "jobs/job_queue.h"
#include "base/last_error.h"
#include "items/lock.h"

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

#include "analysis.h"
#include "items/collections.h"
#include "items/ids.h"
#include "base/knowledge.h"
#include "items/store.h"
#include "items/library_metadata.h"
#include "items/subjects.h"

static const char *kPiecesDir = WORKSHOP_DIR_PIECES;

/* --- id lists ------------------------------------------------------------- */

void workshop_link_ids_free(char **ids, size_t count) {
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

static void free_summaries(WorkshopPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        workshop_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

/* Every Library piece, or those that match the filter (NULL for all). */
static UrsatzStatus add_library_pieces(CommonNames *ids, const WorkshopMetaFilter *filter) {
    WorkshopPieceSummary *pieces = NULL;
    size_t count = 0;
    if (workshop_library_list(kPiecesDir, &pieces, &count) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
        if ((!filter || workshop_meta_matches(&pieces[i], filter)) && !id_list_add(ids, pieces[i].id)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    free_summaries(pieces, count);
    return result;
}

static UrsatzStatus add_collection_pieces(CommonNames *ids, const char *collection_id) {
    if (!text_given(collection_id) || !workshop_collection_is_valid_id(collection_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    WorkshopCollection *collection = NULL;
    UrsatzStatus result = workshop_collection_get(collection_id, &collection);
    if (result == URSATZ_ERR_NOT_FOUND) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    bool ok = true;
    for (size_t i = 0; ok && i < collection->piece_count; i++) {
        ok = id_list_add(ids, collection->pieces[i]);
    }
    workshop_collection_destroy(collection);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
}

static UrsatzStatus add_subject_pieces(CommonNames *ids, const char *subject_id) {
    if (!text_given(subject_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    cJSON *subject = NULL;
    UrsatzStatus result = workshop_subject_get(subject_id, &subject);
    if (result != URSATZ_OK) {
        return result;
    }
    bool ok = id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"));
    cJSON_Delete(subject);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
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
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    cJSON *project = NULL;
    UrsatzStatus found = workshop_material_project_get(project_id, &project);
    if (found == URSATZ_ERR_REQUEST) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    if (found == URSATZ_ERR_NOT_FOUND) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (found != URSATZ_OK) {
        return found == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    }
    cJSON_Delete(project);

    cJSON *items = NULL;
    found = workshop_material_item_list(project_id, NULL, &items);
    if (found != URSATZ_OK) {
        return found == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
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
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    UrsatzStatus result = URSATZ_OK;
    cJSON *subjects = NULL;
    if (item_ids.count > 0) {
        result = workshop_subject_list(&subjects);
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, subjects) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
        if (result != URSATZ_OK || !cJSON_IsString(id)) {
            break;
        }
        /* The list entries carry counts, not the links: read the subject itself. */
        cJSON *subject = NULL;
        UrsatzStatus got = workshop_subject_get(id->valuestring, &subject);
        if (got == URSATZ_ERR_NOT_FOUND) {
            continue;
        }
        result = got;
        if (result == URSATZ_OK && subject_links_item_of(subject, &item_ids) &&
            !id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"))) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        cJSON_Delete(subject);
    }
    cJSON_Delete(subjects);
    common_names_free(&item_ids);
    return result;
}

UrsatzStatus workshop_link_scope_resolve(const WorkshopLinkScope *scope, char ***out_ids,
                                                  size_t *out_count) {
    if (!scope || !out_ids || !out_count) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
    }
    *out_ids = NULL;
    *out_count = 0;

    CommonNames ids = {0};
    UrsatzStatus result = URSATZ_OK;
    switch (scope->kind) {
        case WORKSHOP_LINK_SCOPE_ALL:
            result = add_library_pieces(&ids, NULL);
            break;
        case WORKSHOP_LINK_SCOPE_COLLECTION:
            result = add_collection_pieces(&ids, scope->collection_id);
            break;
        case WORKSHOP_LINK_SCOPE_FILTER: {
            WorkshopMetaFilter filter = {0};
            filter.composer = text_given(scope->composer) ? scope->composer : NULL;
            filter.source = text_given(scope->source) ? scope->source : NULL;
            filter.tag = text_given(scope->tag) ? scope->tag : NULL;
            result = filter.composer || filter.source || filter.tag ? add_library_pieces(&ids, &filter)
                                                                     : workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
            break;
        }
        case WORKSHOP_LINK_SCOPE_SUBJECT:
            result = add_subject_pieces(&ids, scope->subject_id);
            break;
        case WORKSHOP_LINK_SCOPE_PROJECT:
            result = add_project_pieces(&ids, scope->project_id);
            break;
        default:
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid scope");
            break;
    }
    if (result == URSATZ_OK && ids.count == 0) {
        result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, "the scope names no pieces");
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


static WorkshopLock g_lock = WORKSHOP_LOCK_INIT;
#define LOCK() workshop_lock_acquire(&g_lock)
#define UNLOCK() workshop_lock_release(&g_lock)

static struct {
    bool ready;
} g_state;

static bool runs_dir_path(char *out, size_t out_size) {
    return snprintf(out, out_size, "%s", WORKSHOP_DIR_LINKS) < (int)out_size;
}

static bool run_dir_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && workshop_layout_link_dir(id, out, out_size);
}

static bool run_file_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && workshop_layout_link_file(id, out, out_size);
}


/* --- the run index (A20261109-57) ------------------------------------------------------------------------------------------
 * A summary of every run held in memory: built when the service opens and kept up to date when a run starts, writes its progress,
 * finishes or is deleted, so the run list and the links of a piece read it and parse only the runs they need. */

typedef struct {
    char id[64];
    cJSON *scope;
    char *scope_text;
    char status[16];
    char created_at[40];
    char finished_at[40];
    bool has_finished;
    double done;
    double total;
    char **pieces; /* the Library pieces a finished run linked, sorted and without repeats */
    size_t piece_count;
} RunSummary;

static WorkshopLock g_index_lock = WORKSHOP_LOCK_INIT;
static RunSummary *g_index = NULL;
static size_t g_index_count = 0;

static void summary_clear(RunSummary *summary) {
    cJSON_Delete(summary->scope);
    free(summary->scope_text);
    for (size_t i = 0; i < summary->piece_count; i++) {
        free(summary->pieces[i]);
    }
    free(summary->pieces);
    memset(summary, 0, sizeof(*summary));
}

static int compare_text(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* The links of a run's result, when the result is of the schema version this build reads (ANALYZER_MOTIVE_LINKS_VERSION, as the
   link cache checks its own): NULL for a run with no result yet and for a result of another version, whose members may mean
   something else (R-692). */
static const cJSON *current_links(const cJSON *run) {
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(run, "result");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(result, "schema_version");
    if (!cJSON_IsObject(result) || !cJSON_IsNumber(version) || version->valuedouble != ANALYZER_MOTIVE_LINKS_VERSION) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(result, "links");
}

/* Fills a summary from a run record. false on allocation failure. */
static bool summary_fill(RunSummary *summary, const cJSON *run) {
    memset(summary, 0, sizeof(*summary));
    snprintf(summary->id, sizeof(summary->id), "%s", common_json_string(run, "id"));
    snprintf(summary->status, sizeof(summary->status), "%s", common_json_string(run, "status"));
    snprintf(summary->created_at, sizeof(summary->created_at), "%s", common_json_string(run, "created_at"));
    const cJSON *finished = cJSON_GetObjectItemCaseSensitive(run, "finished_at");
    summary->has_finished = cJSON_IsString(finished);
    if (summary->has_finished) {
        snprintf(summary->finished_at, sizeof(summary->finished_at), "%s", finished->valuestring);
    }
    const cJSON *done = cJSON_GetObjectItemCaseSensitive(run, "done");
    const cJSON *total = cJSON_GetObjectItemCaseSensitive(run, "total");
    summary->done = cJSON_IsNumber(done) ? done->valuedouble : 0;
    summary->total = cJSON_IsNumber(total) ? total->valuedouble : 0;
    const cJSON *scope = cJSON_GetObjectItemCaseSensitive(run, "scope");
    summary->scope = scope ? cJSON_Duplicate(scope, 1) : NULL;
    summary->scope_text = scope ? cJSON_PrintUnformatted(scope) : NULL;
    if (scope && (!summary->scope || !summary->scope_text)) {
        summary_clear(summary);
        return false;
    }
    CommonNames pieces = {0};
    const cJSON *link = NULL;
    cJSON_ArrayForEach(link, current_links(run)) {
        const char *a = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "a"), "piece");
        const char *b = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "b"), "piece");
        if ((a[0] && !common_names_contains(&pieces, a) && !common_names_add(&pieces, a)) ||
            (b[0] && !common_names_contains(&pieces, b) && !common_names_add(&pieces, b))) {
            common_names_free(&pieces);
            summary_clear(summary);
            return false;
        }
    }
    summary->pieces = pieces.items;
    summary->piece_count = pieces.count;
    if (summary->piece_count > 1) {
        qsort(summary->pieces, summary->piece_count, sizeof(char *), compare_text);
    }
    return true;
}

static void index_clear_locked(void) {
    for (size_t i = 0; i < g_index_count; i++) {
        summary_clear(&g_index[i]);
    }
    free(g_index);
    g_index = NULL;
    g_index_count = 0;
}

/* Puts the summary of run in the index (replacing the one with its id). */
static void index_update(const cJSON *run) {
    RunSummary summary;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(run, "id")) || !summary_fill(&summary, run)) {
        return;
    }
    workshop_lock_acquire(&g_index_lock);
    for (size_t i = 0; i < g_index_count; i++) {
        if (strcmp(g_index[i].id, summary.id) == 0) {
            summary_clear(&g_index[i]);
            g_index[i] = summary;
            workshop_lock_release(&g_index_lock);
            return;
        }
    }
    RunSummary *grown = realloc(g_index, (g_index_count + 1) * sizeof(*g_index));
    if (grown) {
        g_index = grown;
        g_index[g_index_count++] = summary;
    } else {
        summary_clear(&summary);
    }
    workshop_lock_release(&g_index_lock);
}

static void index_remove(const char *id) {
    workshop_lock_acquire(&g_index_lock);
    for (size_t i = 0; i < g_index_count; i++) {
        if (strcmp(g_index[i].id, id) == 0) {
            summary_clear(&g_index[i]);
            memmove(&g_index[i], &g_index[i + 1], (g_index_count - i - 1) * sizeof(*g_index));
            g_index_count--;
            break;
        }
    }
    workshop_lock_release(&g_index_lock);
}

/* Newest first: by created_at, then id, both descending. */
static int compare_summaries(const void *left, const void *right) {
    const RunSummary *a = left;
    const RunSummary *b = right;
    int by_time = strcmp(b->created_at, a->created_at);
    return by_time != 0 ? by_time : strcmp(b->id, a->id);
}

/* A copy of the index, newest first, for a reader that works after the lock is released. NULL with count 0 when it is empty. */
static RunSummary *index_snapshot(size_t *count) {
    workshop_lock_acquire(&g_index_lock);
    RunSummary *copy = g_index_count ? calloc(g_index_count, sizeof(*copy)) : NULL;
    size_t made = 0;
    for (size_t i = 0; copy && i < g_index_count; i++) {
        /* a shallow copy of what the readers use: the strings are copied, the scope and the pieces are not needed after */
        copy[made] = g_index[i];
        copy[made].scope = g_index[i].scope ? cJSON_Duplicate(g_index[i].scope, 1) : NULL;
        copy[made].scope_text = g_index[i].scope_text ? common_strdup(g_index[i].scope_text) : NULL;
        copy[made].pieces = NULL;
        copy[made].piece_count = 0;
        if (g_index[i].piece_count) {
            copy[made].pieces = calloc(g_index[i].piece_count, sizeof(char *));
            for (size_t p = 0; copy[made].pieces && p < g_index[i].piece_count; p++) {
                copy[made].pieces[p] = common_strdup(g_index[i].pieces[p]);
                copy[made].piece_count++;
            }
        }
        made++;
    }
    workshop_lock_release(&g_index_lock);
    if (made > 1) {
        qsort(copy, made, sizeof(*copy), compare_summaries);
    }
    *count = made;
    return copy;
}

static void snapshot_free(RunSummary *summaries, size_t count) {
    for (size_t i = 0; i < count; i++) {
        summary_clear(&summaries[i]);
    }
    free(summaries);
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

static const char *scope_kind_label(WorkshopLinkScopeKind kind) {
    switch (kind) {
        case WORKSHOP_LINK_SCOPE_COLLECTION:
            return "collection";
        case WORKSHOP_LINK_SCOPE_FILTER:
            return "filter";
        case WORKSHOP_LINK_SCOPE_SUBJECT:
            return "subject";
        case WORKSHOP_LINK_SCOPE_PROJECT:
            return "project";
        case WORKSHOP_LINK_SCOPE_ALL:
        default:
            return "all";
    }
}

/* {"kind","collection_id","filter":{"composer","source","tag"},"subject_id","project_id"}: what the kind does not use
 * is null, so two runs over the same scope have the same text. */
static cJSON *scope_json(const WorkshopLinkScope *scope) {
    cJSON *object = cJSON_CreateObject();
    cJSON *filter = NULL;
    bool is_filter = scope->kind == WORKSHOP_LINK_SCOPE_FILTER;
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
                               scope->kind == WORKSHOP_LINK_SCOPE_COLLECTION ? string_or_null(scope->collection_id)
                                                                               : cJSON_CreateNull())) {
        cJSON_Delete(filter);
        cJSON_Delete(object);
        return NULL;
    }
    cJSON_AddItemToObject(object, "filter", filter);
    if (!cJSON_AddItemToObject(object, "subject_id",
                               scope->kind == WORKSHOP_LINK_SCOPE_SUBJECT ? string_or_null(scope->subject_id)
                                                                            : cJSON_CreateNull()) ||
        !cJSON_AddItemToObject(object, "project_id",
                               scope->kind == WORKSHOP_LINK_SCOPE_PROJECT ? string_or_null(scope->project_id)
                                                                            : cJSON_CreateNull())) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* --- a run ---------------------------------------------------------------- */

typedef struct {
    char id[64];
    cJSON *run;  /* the run.json object, owned by the job */
    char **ids;  /* the scope's piece ids */
    size_t count;
    time_t last_write;
    const UrsatzJob *handle; /* the queue's job handle: progress and cancel, while the job runs */
} Job;

static void job_free(Job *job) {
    cJSON_Delete(job->run);
    workshop_link_ids_free(job->ids, job->count);
    free(job);
}

static bool job_write(const Job *job) {
    char path[PATH_BYTES];
    cJSON *copy = run_file_path(job->id, path, sizeof(path)) ? cJSON_Duplicate(job->run, 1) : NULL;
    bool ok = copy && workshop_format_write(path, WORKSHOP_FORMAT_LINK_RUN, copy, NULL) == URSATZ_OK;
    cJSON_Delete(copy);
    if (ok) {
        index_update(job->run);
    }
    return ok;
}

/* Commits the run's record through a transaction: a new run is a folder holding run.json, a finished one replaces the file. A progress write
 * (job_write) is not a change of the data folder that needs a record: it is an atomic write of what a restart reads as interrupted. */
static bool job_commit(Job *job, bool is_new) {
    char path[PATH_BYTES];
    char dir[PATH_BYTES];
    if (!run_file_path(job->id, path, sizeof(path)) || !run_dir_path(job->id, dir, sizeof(dir))) {
        return false;
    }
    WorkshopTxn *txn = workshop_txn_begin(NULL, is_new ? "links.start" : "links.finish", job->id, 0);
    if (!txn) {
        return false;
    }
    cJSON *copy = cJSON_Duplicate(job->run, 1);
    bool staged = copy != NULL;
    if (staged && is_new) {
        const char *pending = NULL;
        char file[PATH_BYTES * 2];
        staged = workshop_txn_stage_folder(txn, dir, &pending) == URSATZ_OK &&
                 snprintf(file, sizeof(file), "%s/" WORKSHOP_FILE_RUN, pending) < (int)sizeof(file) &&
                 workshop_format_write(file, WORKSHOP_FORMAT_LINK_RUN, copy, NULL) == URSATZ_OK;
    } else if (staged) {
        staged = workshop_txn_stage_json(txn, path, WORKSHOP_FORMAT_LINK_RUN, copy) == URSATZ_OK;
    }
    cJSON_Delete(copy);
    if (!staged) {
        workshop_txn_abort(txn);
        return false;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    bool ok = workshop_txn_commit(txn, &error) == URSATZ_OK;
    if (ok) {
        index_update(job->run);
    }
    return ok;
}

static bool job_set_progress(Job *job, size_t done, size_t total) {
    return set_member(job->run, "done", cJSON_CreateNumber((double)done)) &&
           set_member(job->run, "total", cJSON_CreateNumber((double)total));
}

/* Called by the Analyzer when the pairs are known and after each pair: the queue's progress follows at once, run.json and the index at most once a second. */
static void job_progress(size_t done, size_t total, void *context) {
    Job *job = context;
    if (job->handle && job->handle->progress) {
        job->handle->progress(job->handle->user, "pairs", (unsigned)done, (unsigned)total);
    }
    time_t now = time(NULL);
    if (now != job->last_write && job_set_progress(job, done, total)) {
        job->last_write = now;
        job_write(job);
    }
}

static bool job_cancelled(void *context) {
    const Job *job = context;
    return ursatz_job_cancelled(job->handle);
}

/* Why a piece cannot be added, or NULL when it was. */
static const char *add_piece(MotiveLinkScope *scope, const char *id) {
    if (!workshop_collection_is_valid_id(id)) {
        return "not found";
    }
    /* Another schema, Analyzer version or Ursatz commit: analyze the piece again before the run. The error-triggered
       re-analysis below stays as a fallback. */
    UrsatzStatus ensured = workshop_analysis_ensure_current(kPiecesDir, id);
    if (ensured != URSATZ_OK) {
        return ensured == URSATZ_ERR_NOT_FOUND ? "not found" : "re-analysis failed";
    }

    for (int attempt = 0; attempt < 2; attempt++) {
        char *document = NULL;
        if (workshop_analysis_get_document(kPiecesDir, id, NULL, 0, &document) != URSATZ_OK) {
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
        if (workshop_analysis_reanalyze(kPiecesDir, id) != URSATZ_OK) {
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
    if (!job_commit(job, false)) {
        job_write(job); /* the record could not be committed: the file is written as a progress write is */
    }
}

static void job_fail(Job *job, const char *message) {
    job_finish(job, "failed", message, NULL);
}

/* The run itself, on the worker thread. */
static UrsatzStatus job_run(Job *job) {
    cJSON *skipped = cJSON_GetObjectItemCaseSensitive(job->run, "skipped");
    MotiveLinkScope *scope = analyzer_links_scope_create(workshop_knowledge_snapshot());
    if (!scope) {
        job_fail(job, "out of memory");
        return URSATZ_OK;
    }
    size_t used = 0;
    bool ok = true;
    for (size_t i = 0; ok && i < job->count; i++) {
        if (ursatz_job_cancelled(job->handle)) {
            analyzer_links_scope_destroy(scope);
            return URSATZ_ERR_CANCELLED;
        }
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
        return URSATZ_OK;
    }
    if (used == 0) {
        analyzer_links_scope_destroy(scope);
        job_fail(job, "no analyzable pieces");
        return URSATZ_OK;
    }

    char cache_path[PATH_BYTES + 64];
    snprintf(cache_path, sizeof(cache_path), "%s", WORKSHOP_FILE_LINK_CACHE);
    MotiveLinkCache *cache = NULL;
    if (analyzer_links_cache_load(cache_path, &cache) != URSATZ_OK) {
        /* An unreadable cache is only a cache: start it again. */
        remove(cache_path);
        cache = NULL;
        if (analyzer_links_cache_load(cache_path, &cache) != URSATZ_OK) {
            analyzer_links_scope_destroy(scope);
            job_fail(job, "cache unreadable");
            return URSATZ_OK;
        }
    }

    MotiveLinksOptions options;
    analyzer_links_options_default(&options);
    options.progress = job_progress;
    options.progress_context = job;
    options.cancelled = job_cancelled;
    options.cancel_user = job;

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
    if (run == URSATZ_ERR_CANCELLED) {
        cJSON_Delete(result);
        return URSATZ_ERR_CANCELLED; /* nothing is written: the run is deleted, or marked interrupted at the next open */
    }
    if (run != URSATZ_OK || !result) {
        cJSON_Delete(result);
        job_fail(job, "linking failed");
        return URSATZ_OK;
    }
    job_set_progress(job, pairs, pairs);
    job_finish(job, "done", "", result);
    return URSATZ_OK;
}

/* The queue's job: runs the link run and answers {"ok": true, "run": <the run as it ended>}. */
static UrsatzStatus link_job_run(void *input, const UrsatzJob *handle, WorkshopJobResult *out) {
    Job *job = input;
    job->handle = handle;
    set_member(job->run, "status", cJSON_CreateString("running"));
    job_write(job);
    UrsatzStatus status = job_run(job);
    if (status == URSATZ_OK) {
        cJSON *answer = cJSON_CreateObject();
        if (answer && cJSON_AddBoolToObject(answer, "ok", 1) && cJSON_AddItemToObject(answer, "run", cJSON_Duplicate(job->run, 1))) {
            out->http_status = 200;
            out->tree = answer;
        } else {
            cJSON_Delete(answer);
            out->http_status = 500;
            out->tree = cJSON_Parse("{\"ok\":false,\"error\":\"out of memory\"}");
        }
    } else {
        out->http_status = 499;
        out->tree = cJSON_Parse("{\"ok\":false,\"error\":\"Cancelled\"}");
    }
    return status;
}

static void link_job_free(void *input) {
    job_free(input);
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
        workshop_format_read(path, WORKSHOP_FORMAT_LINK_RUN, true, &run, NULL);
        const cJSON *status = cJSON_GetObjectItemCaseSensitive(run, "status");
        if (cJSON_IsString(status) && (strcmp(status->valuestring, "running") == 0 || strcmp(status->valuestring, "queued") == 0)) {
            char now[40];
            iso_utc_now(now, sizeof(now));
            set_member(run, "status", cJSON_CreateString("failed"));
            set_member(run, "message", cJSON_CreateString("interrupted"));
            set_member(run, "finished_at", cJSON_CreateString(now));
            (void)workshop_format_write(path, WORKSHOP_FORMAT_LINK_RUN, run, NULL);
        }
        if (run) {
            index_update(run); /* the index holds every run the folder holds, as of this open */
        }
        cJSON_Delete(run);
    }
    common_names_free(&names);
}

bool workshop_link_service_init(void) {
    workshop_lock_acquire(&g_index_lock);
    index_clear_locked();
    workshop_lock_release(&g_index_lock);
    LOCK();
    bool ok = (common_dir_make(WORKSHOP_DIR_LINKS, NULL) == URSATZ_OK) && (common_dir_make(WORKSHOP_DIR_CACHE, NULL) == URSATZ_OK);
    g_state.ready = ok;
    if (ok) {
        mark_interrupted();
    }
    UNLOCK();
    return ok;
}

void workshop_link_service_shutdown(void) {
    /* The jobs are stopped by workshop_jobs_stop, which the server calls first. */
    LOCK();
    g_state.ready = false;
    UNLOCK();
    workshop_lock_acquire(&g_index_lock);
    index_clear_locked();
    workshop_lock_release(&g_index_lock);
}

UrsatzStatus workshop_link_run_start(const WorkshopLinkScope *scope, char **out_id, char out_job[WORKSHOP_JOB_ID_BYTES]) {
    if (out_id) {
        *out_id = NULL;
    }
    char **ids = NULL;
    size_t count = 0;
    UrsatzStatus result = workshop_link_scope_resolve(scope, &ids, &count);
    if (result != URSATZ_OK) {
        return result;
    }

    char *id = workshop_new_id("lnk");
    Job *job = id ? calloc(1, sizeof(*job)) : NULL;
    if (!job || strlen(id) >= sizeof(job->id)) {
        free(id);
        free(job);
        workshop_link_ids_free(ids, count);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
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
                 cJSON_AddStringToObject(job->run, "status", "queued") &&
                 cJSON_AddStringToObject(job->run, "message", "") && cJSON_AddNumberToObject(job->run, "done", 0) &&
                 cJSON_AddNumberToObject(job->run, "total", 0) && cJSON_AddArrayToObject(job->run, "skipped");
    if (!built) {
        job_free(job);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    char run_id[sizeof(job->id)];
    snprintf(run_id, sizeof(run_id), "%s", job->id);

    LOCK();
    bool ready = g_state.ready;
    UNLOCK();
    char job_id[WORKSHOP_JOB_ID_BYTES];
    if (!ready) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    } else if (!job_commit(job, true)) { /* the record of a queued run is a transaction: links.start */
        result = workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
    } else {
        /* A second run waits in the queue: the "links" kind runs one job at a time. The queue owns the job from here and frees it; the
           run's id is the job's subject, so a delete finds the job by it (R-645). */
        Job *submitted = job;
        job = NULL;
        if (!workshop_job_submit_subject("links", run_id, link_job_run, submitted, link_job_free, job_id)) {
            char dir[PATH_BYTES];
            if (run_dir_path(run_id, dir, sizeof(dir))) {
                common_dir_remove(dir, NULL);
            }
            index_remove(run_id);
            return workshop_fail(URSATZ_ERR_INTERNAL, "the job queue is not running");
        }
        if (out_job) {
            snprintf(out_job, WORKSHOP_JOB_ID_BYTES, "%s", job_id);
        }
    }
    if (result != URSATZ_OK) {
        job_free(job);
        return result;
    }
    if (out_id) {
        *out_id = common_strdup(run_id);
        if (!*out_id) {
            return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    return URSATZ_OK;
}

/* --- reading runs --------------------------------------------------------- */


UrsatzStatus workshop_link_run_list(cJSON **out_runs) {
    *out_runs = NULL;
    size_t count = 0;
    RunSummary *runs = index_snapshot(&count);
    cJSON *list = cJSON_CreateArray();
    LOCK();
    for (size_t i = 0; list && i < count; i++) {
        const RunSummary *run = &runs[i];
        cJSON *entry = cJSON_CreateObject();
        double done = run->done;
        double total = run->total;
        bool ok = entry && cJSON_AddStringToObject(entry, "id", run->id) &&
                  cJSON_AddItemToObject(entry, "scope", run->scope ? cJSON_Duplicate(run->scope, 1) : cJSON_CreateObject()) &&
                  cJSON_AddStringToObject(entry, "status", run->status) && cJSON_AddStringToObject(entry, "created_at", run->created_at) &&
                  cJSON_AddItemToObject(entry, "finished_at", run->has_finished ? cJSON_CreateString(run->finished_at) : cJSON_CreateNull()) &&
                  cJSON_AddNumberToObject(entry, "done", done) && cJSON_AddNumberToObject(entry, "total", total);
        if (ok) {
            cJSON_AddItemToArray(list, entry);
        } else {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            list = NULL;
        }
    }
    UNLOCK();
    snapshot_free(runs, count);
    if (!list) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_runs = list;
    return URSATZ_OK;
}

UrsatzStatus workshop_link_run_get(const char *id, cJSON **out_run) {
    *out_run = NULL;
    char path[PATH_BYTES];
    if (!id || !run_file_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON *run = NULL;
    workshop_format_read(path, WORKSHOP_FORMAT_LINK_RUN, true, &run, NULL);
    if (!run) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    *out_run = run;
    return URSATZ_OK;
}

UrsatzStatus workshop_link_run_delete(const char *id) {
    char dir[PATH_BYTES];
    if (!id || !run_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    /* A run that is queued or running is cancelled first: its job stops between pieces and pairs and leaves no half-written file. */
    char job_id[WORKSHOP_JOB_ID_BYTES];
    if (workshop_job_find(id, job_id, sizeof(job_id))) {
        workshop_job_cancel(job_id);
        workshop_job_wait(job_id);
    }
    LOCK();
    UrsatzStatus result = URSATZ_OK;
    if (!common_path_is_dir(dir)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    } else {
        /* the lock of this file is taken first, as at the start of a run, so the order against the transaction is the same */
        WorkshopTxn *txn = workshop_txn_begin(NULL, "links.delete", id, 0);
        UrsatzError error;
        memset(&error, 0, sizeof(error));
        if (!txn || workshop_txn_remove(txn, dir) != URSATZ_OK) {
            workshop_txn_abort(txn);
            result = workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
        } else if (workshop_txn_commit(txn, &error) != URSATZ_OK) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "link operation failed");
        } else {
            index_remove(id);
        }
    }
    UNLOCK();
    return result;
}

UrsatzStatus workshop_link_piece_links(const char *piece_id, cJSON **out_links) {
    *out_links = NULL;
    size_t count = 0;
    RunSummary *runs = index_snapshot(&count);
    cJSON *list = cJSON_CreateArray();
    UrsatzStatus result = list ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    CommonNames seen = {0}; /* the scopes whose newest done run was taken */
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
        const RunSummary *summary = &runs[i];
        if (strcmp(summary->status, "done") != 0 || !summary->scope || !summary->scope_text) {
            continue;
        }
        bool newest = !common_names_contains(&seen, summary->scope_text);
        if (newest && !id_list_add(&seen, summary->scope_text)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        /* only a run that linked the piece is read: the index says which */
        bool linked = false;
        for (size_t p = 0; newest && !linked && p < summary->piece_count; p++) {
            linked = strcmp(summary->pieces[p], piece_id) == 0;
        }
        if (!linked) {
            continue;
        }
        char path[PATH_BYTES];
        cJSON *run = NULL;
        if (!run_file_path(summary->id, path, sizeof(path)) ||
            workshop_format_read(path, WORKSHOP_FORMAT_LINK_RUN, true, &run, NULL) != URSATZ_OK) {
            continue;
        }
        const cJSON *links = current_links(run);
        const cJSON *link = NULL;
        cJSON_ArrayForEach(link, links) {
            const char *a = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "a"), "piece");
            const char *b = common_json_string(cJSON_GetObjectItemCaseSensitive(link, "b"), "piece");
            if (strcmp(a, piece_id) != 0 && strcmp(b, piece_id) != 0) {
                continue;
            }
            cJSON *entry = cJSON_CreateObject();
            if (!entry || !cJSON_AddStringToObject(entry, "run", summary->id) ||
                !cJSON_AddItemToObject(entry, "scope", cJSON_Duplicate(summary->scope, 1)) ||
                !cJSON_AddItemToObject(entry, "link", cJSON_Duplicate(link, 1))) {
                cJSON_Delete(entry);
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            cJSON_AddItemToArray(list, entry);
        }
        cJSON_Delete(run);
    }
    common_names_free(&seen);
    snapshot_free(runs, count);
    if (!list || result != URSATZ_OK) {
        cJSON_Delete(list);
        return result == URSATZ_OK ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : result;
    }
    *out_links = list;
    return URSATZ_OK;
}

char *workshop_link_piece_title(const char *piece_id) {
    char *filename = NULL;
    if (!piece_id || !workshop_collection_is_valid_id(piece_id) ||
        workshop_library_get_filename(kPiecesDir, piece_id, &filename) != URSATZ_OK) {
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

/* ---- the links routes' scope and titles (A20261109-47) ---------------------- */

/* A string member, or NULL when it is missing or null. *bad is set when it is some other type. */
static const char *scope_string(const cJSON *object, const char *key, bool *bad) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!item || cJSON_IsNull(item)) {
    return NULL;
  }
  if (!cJSON_IsString(item)) {
    *bad = true;
    return NULL;
  }
  return item->valuestring;
}

/* Reads {"kind", "collection_id", "filter": {...}, "subject_id", "project_id"}. false when it cannot be a scope. */
bool workshop_link_scope_parse(const cJSON *json, WorkshopLinkScope *scope) {
  memset(scope, 0, sizeof(*scope));
  if (!cJSON_IsObject(json)) {
    return false;
  }
  bool bad = false;
  const char *kind = scope_string(json, "kind", &bad);
  if (!kind || bad) {
    return false;
  }
  if (strcmp(kind, "all") == 0) {
    scope->kind = WORKSHOP_LINK_SCOPE_ALL;
  } else if (strcmp(kind, "collection") == 0) {
    scope->kind = WORKSHOP_LINK_SCOPE_COLLECTION;
  } else if (strcmp(kind, "filter") == 0) {
    scope->kind = WORKSHOP_LINK_SCOPE_FILTER;
  } else if (strcmp(kind, "subject") == 0) {
    scope->kind = WORKSHOP_LINK_SCOPE_SUBJECT;
  } else if (strcmp(kind, "project") == 0) {
    scope->kind = WORKSHOP_LINK_SCOPE_PROJECT;
  } else {
    return false;
  }
  scope->collection_id = scope_string(json, "collection_id", &bad);
  scope->subject_id = scope_string(json, "subject_id", &bad);
  scope->project_id = scope_string(json, "project_id", &bad);
  const cJSON *filter = cJSON_GetObjectItemCaseSensitive(json, "filter");
  if (filter && !cJSON_IsNull(filter)) {
    if (!cJSON_IsObject(filter)) {
      return false;
    }
    scope->composer = scope_string(filter, "composer", &bad);
    scope->source = scope_string(filter, "source", &bad);
    scope->tag = scope_string(filter, "tag", &bad);
  }
  return !bad;
}

/* Adds "title" to an object that names a piece in "piece". */
static bool add_title(cJSON *end) {
  const cJSON *piece = cJSON_GetObjectItemCaseSensitive(end, "piece");
  if (!cJSON_IsString(piece)) {
    return true;
  }
  char *title = workshop_link_piece_title(piece->valuestring);
  bool ok = title && cJSON_AddStringToObject(end, "title", title);
  free(title);
  return ok;
}

/* Adds each end's title to a link ({"a", "b", ...}). */
static bool add_link_titles(cJSON *link) {
  return add_title(cJSON_GetObjectItemCaseSensitive(link, "a")) &&
         add_title(cJSON_GetObjectItemCaseSensitive(link, "b"));
}

/* Adds titles to every link end and every group member of a run's "result". */
bool workshop_link_titles_for_run(cJSON *run) {
  cJSON *result = cJSON_GetObjectItemCaseSensitive(run, "result");
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(result, "links")) {
    if (!add_link_titles(item)) {
      return false;
    }
  }
  cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(result, "groups")) {
    cJSON *member = NULL;
    cJSON_ArrayForEach(member, cJSON_GetObjectItemCaseSensitive(item, "members")) {
      if (!add_title(member)) {
        return false;
      }
    }
  }
  return true;
}

bool workshop_link_titles_for_piece_links(cJSON *links) {
  cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, links) {
    if (!add_link_titles(cJSON_GetObjectItemCaseSensitive(entry, "link"))) {
      return false;
    }
  }
  return true;
}

