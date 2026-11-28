#include "base/command_context.h"
#include "base/events_hub.h"
#include "base/last_error.h"
#include "txn.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include <ursatz_workshop/workshop_api.h>

#include "items/formats.h"
#include "items/journal.h"
#include "items/layers.h"
#include "items/layout.h"

#define PATH_BYTES 1024

typedef struct {
    char *pending;
    char *final;
} Write;

typedef struct {
    char *removed;
    char *final;
} Remove;

struct WorkshopTxn {
    long long seq;
    char *command;
    char *actor;
    char *target;
    long base_version;
    Write *writes;
    size_t write_count;
    Remove *removes;
    size_t remove_count;
    cJSON *headers; /* an array of item objects */
};

struct WorkshopStore {
    pthread_mutex_t lock;
    long long next_seq; /* 0 until the journal is read */
};

static WorkshopStore g_store = {PTHREAD_MUTEX_INITIALIZER, 0};

/* --- the stops of the tests ----------------------------------------------------------------------------------------------------- */

#ifdef URSATZ_WORKSHOP_TEST_HOOKS
static WorkshopCrashPoint g_crash_point = WORKSHOP_CRASH_NONE;
static unsigned g_crash_nth = 0;
static unsigned g_crash_seen = 0;

void workshop_test_set_crash_point(WorkshopCrashPoint point, unsigned nth) {
    g_crash_point = point;
    g_crash_nth = nth;
    g_crash_seen = 0;
}

static void crash_at(WorkshopCrashPoint point) {
    if (point == g_crash_point && ++g_crash_seen == g_crash_nth) {
        _exit(70);
    }
}

static void crash_before_sync(void) {
    crash_at(WORKSHOP_CRASH_BEFORE_JOURNAL_SYNC);
}
#define CRASH(point) crash_at(WORKSHOP_CRASH_##point)
#define BETWEEN_HOOK crash_before_sync
#else
#define CRASH(point) ((void)0)
#define BETWEEN_HOOK NULL
#endif

/* --- names -------------------------------------------------------------------------------------------------------------------------- */

bool workshop_txn_aside_path(const char *kind, long long seq, const char *final_path, char *out, size_t size) {
    const char *slash = strrchr(final_path, '/');
    int written = slash ? snprintf(out, size, "%.*s/.%s-%lld-%s", (int)(slash - final_path), final_path, kind, seq, slash + 1)
                        : snprintf(out, size, ".%s-%lld-%s", kind, seq, final_path);
    return written > 0 && (size_t)written < size;
}

static char *copy_text(const char *text) {
    return common_strdup(text ? text : "");
}

/* --- begin -------------------------------------------------------------------------------------------------------------------------- */

WorkshopTxn *workshop_txn_begin(WorkshopStore *store, const char *command, const char *target, long base_version) {
    /* A record names who gave the command it belongs to: a write that is not part of one has no one to name (R-692). */
    const WorkshopCommandContext *context = workshop_command_context();
    if (!context || context->actor[0] == '\0') {
        workshop_fail(URSATZ_ERR_INTERNAL, "a write needs a command");
        return NULL;
    }
    WorkshopStore *use = store ? store : &g_store;
    pthread_mutex_lock(&use->lock);
    if (use->next_seq == 0) {
        WorkshopJournal journal;
        UrsatzError error;
        memset(&error, 0, sizeof(error));
        if (workshop_journal_read(WORKSHOP_JOURNAL_FILE, &journal, &error) != URSATZ_OK) {
            pthread_mutex_unlock(&use->lock);
            return NULL;
        }
        if (journal.torn) {
            (void)workshop_journal_truncate(WORKSHOP_JOURNAL_FILE, journal.good_bytes, NULL);
        }
        use->next_seq = journal.last_seq + 1;
        workshop_journal_free(&journal);
    }
    /* A command in progress names its records, gives their actor and the version it was made against (base/command_context.h). */
    const char *actor = context->actor;
    command = context->command[0] != '\0' ? context->command : command;
    base_version = context->base_version >= 0 ? context->base_version : base_version;
    WorkshopTxn *txn = calloc(1, sizeof(*txn));
    if (!txn) {
        pthread_mutex_unlock(&use->lock);
        return NULL;
    }
    txn->seq = use->next_seq;
    txn->command = copy_text(command);
    txn->actor = copy_text(actor);
    txn->target = copy_text(target);
    txn->base_version = base_version;
    txn->headers = cJSON_CreateArray();
    if (!txn->command || !txn->actor || !txn->target || !txn->headers) {
        free(txn->command);
        free(txn->actor);
        free(txn->target);
        cJSON_Delete(txn->headers);
        free(txn);
        pthread_mutex_unlock(&use->lock);
        return NULL;
    }
    return txn;
}

static void txn_free(WorkshopTxn *txn) {
    for (size_t i = 0; i < txn->write_count; i++) {
        free(txn->writes[i].pending);
        free(txn->writes[i].final);
    }
    for (size_t i = 0; i < txn->remove_count; i++) {
        free(txn->removes[i].removed);
        free(txn->removes[i].final);
    }
    free(txn->writes);
    free(txn->removes);
    free(txn->command);
    free(txn->actor);
    free(txn->target);
    cJSON_Delete(txn->headers);
    free(txn);
}

/* --- staging ------------------------------------------------------------------------------------------------------------------------ */

static UrsatzStatus add_write(WorkshopTxn *txn, const char *pending, const char *final_path) {
    for (size_t i = 0; i < txn->write_count; i++) {
        if (strcmp(txn->writes[i].final, final_path) == 0) {
            return URSATZ_OK; /* staged again: the pending file was written over, one rename puts it in place */
        }
    }
    Write *grown = realloc(txn->writes, (txn->write_count + 1) * sizeof(*grown));
    if (!grown) {
        return URSATZ_ERR_ALLOC;
    }
    txn->writes = grown;
    grown[txn->write_count].pending = common_strdup(pending);
    grown[txn->write_count].final = common_strdup(final_path);
    if (!grown[txn->write_count].pending || !grown[txn->write_count].final) {
        free(grown[txn->write_count].pending);
        free(grown[txn->write_count].final);
        return URSATZ_ERR_ALLOC;
    }
    txn->write_count++;
    return URSATZ_OK;
}

UrsatzStatus workshop_txn_stage_folder(WorkshopTxn *txn, const char *final_path, const char **out_pending) {
    char pending[PATH_BYTES];
    if (!workshop_txn_aside_path("pending", txn->seq, final_path, pending, sizeof(pending)) || common_dir_make_parent(pending, NULL) != URSATZ_OK) {
        return URSATZ_ERR_IO;
    }
    common_dir_remove(pending, NULL);
    if (common_dir_create(pending, NULL) != URSATZ_OK) {
        return URSATZ_ERR_IO;
    }
    UrsatzStatus added = add_write(txn, pending, final_path);
    if (added != URSATZ_OK) {
        return added;
    }
    if (out_pending) {
        *out_pending = txn->writes[txn->write_count - 1].pending;
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_txn_adopt_folder(WorkshopTxn *txn, const char *folder, const char *final_path, const char **out_pending) {
    char pending[PATH_BYTES];
    if (!workshop_txn_aside_path("pending", txn->seq, final_path, pending, sizeof(pending)) || common_dir_make_parent(pending, NULL) != URSATZ_OK) {
        return URSATZ_ERR_IO;
    }
    common_dir_remove(pending, NULL);
    if (rename(folder, pending) != 0) {
        return URSATZ_ERR_IO;
    }
    UrsatzStatus added = add_write(txn, pending, final_path);
    if (added != URSATZ_OK) {
        return added;
    }
    if (out_pending) {
        *out_pending = txn->writes[txn->write_count - 1].pending;
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_txn_stage_file(WorkshopTxn *txn, const char *final_path, const void *data, size_t size) {
    char pending[PATH_BYTES];
    if (!workshop_txn_aside_path("pending", txn->seq, final_path, pending, sizeof(pending)) || common_dir_make_parent(pending, NULL) != URSATZ_OK) {
        return URSATZ_ERR_IO;
    }
    UrsatzStatus written = common_file_write_atomic(pending, data, size, NULL);
    if (written != URSATZ_OK) {
        return written;
    }
    return add_write(txn, pending, final_path);
}

UrsatzStatus workshop_txn_stage_json(WorkshopTxn *txn, const char *final_path, const char *format, cJSON *object) {
    if (!workshop_format_stamp(object, format)) {
        return URSATZ_ERR_ALLOC;
    }
    UrsatzStatus status = workshop_format_check(object, format, final_path, NULL);
    char *text = status == URSATZ_OK ? cJSON_Print(object) : NULL;
    workshop_format_strip(object);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!text) {
        return URSATZ_ERR_ALLOC;
    }
    status = workshop_txn_stage_file(txn, final_path, text, strlen(text));
    free(text);
    return status;
}

UrsatzStatus workshop_txn_remove(WorkshopTxn *txn, const char *final_path) {
    if (!common_path_exists(final_path)) {
        return URSATZ_OK;
    }
    char removed[PATH_BYTES];
    if (!workshop_txn_aside_path("removed", txn->seq, final_path, removed, sizeof(removed))) {
        return URSATZ_ERR_IO;
    }
    common_dir_remove(removed, NULL);
    Remove *grown = realloc(txn->removes, (txn->remove_count + 1) * sizeof(*grown));
    if (!grown) {
        return URSATZ_ERR_ALLOC;
    }
    txn->removes = grown;
    grown[txn->remove_count].removed = common_strdup(removed);
    grown[txn->remove_count].final = common_strdup(final_path);
    if (!grown[txn->remove_count].removed || !grown[txn->remove_count].final) {
        free(grown[txn->remove_count].removed);
        free(grown[txn->remove_count].final);
        return URSATZ_ERR_ALLOC;
    }
    if (rename(final_path, removed) != 0) {
        free(grown[txn->remove_count].removed);
        free(grown[txn->remove_count].final);
        return URSATZ_ERR_IO;
    }
    txn->remove_count++;
    return URSATZ_OK;
}

UrsatzStatus workshop_txn_set_header(WorkshopTxn *txn, const cJSON *item) {
    cJSON *header = cJSON_Duplicate(item, 1);
    if (!header) {
        return URSATZ_ERR_ALLOC;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(header, "versions");
    cJSON_DeleteItemFromObjectCaseSensitive(header, "variant_of");
    cJSON_DeleteItemFromObjectCaseSensitive(header, "journal_seq");
    /* a newer header of the same item replaces the one set before it */
    const char *id = common_json_string(header, "id");
    int index = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, txn->headers) {
        if (strcmp(common_json_string(entry, "id"), id) == 0) {
            cJSON_DeleteItemFromArray(txn->headers, index);
            break;
        }
        index++;
    }
    cJSON_AddItemToArray(txn->headers, header);
    return URSATZ_OK;
}

const char *workshop_txn_read_path(const WorkshopTxn *txn, const char *final_path) {
    for (size_t i = 0; txn && i < txn->write_count; i++) {
        if (strcmp(txn->writes[i].final, final_path) == 0) {
            return txn->writes[i].pending;
        }
    }
    return final_path;
}

/* --- abort -------------------------------------------------------------------------------------------------------------------------- */

static void unlock(void) {
    pthread_mutex_unlock(&g_store.lock);
}

void workshop_txn_abort(WorkshopTxn *txn) {
    if (!txn) {
        return;
    }
    for (size_t i = 0; i < txn->write_count; i++) {
        common_dir_remove(txn->writes[i].pending, NULL);
    }
    for (size_t i = 0; i < txn->remove_count; i++) {
        if (common_path_exists(txn->removes[i].removed)) {
            rename(txn->removes[i].removed, txn->removes[i].final);
        }
    }
    txn_free(txn);
    unlock();
}

/* --- commit ------------------------------------------------------------------------------------------------------------------------- */

static bool is_body_format(const char *name) {
    return strcmp(name, WORKSHOP_FORMAT_MOTIF_BODY) == 0 || strcmp(name, WORKSHOP_FORMAT_THEME_BODY) == 0 ||
           strcmp(name, WORKSHOP_FORMAT_PIECE_BODY) == 0 || strcmp(name, WORKSHOP_FORMAT_HARMONY_BODY) == 0;
}

/* The parent of path's parent folder, "<dir>/realization.json" in out (the realization a readings/<name>.json is about). */
static bool realization_beside(const char *path, char *out, size_t size) {
    char folder[PATH_BYTES];
    snprintf(folder, sizeof(folder), "%s", path);
    char *slash = strrchr(folder, '/'); /* the file */
    if (!slash) {
        return false;
    }
    *slash = '\0';
    slash = strrchr(folder, '/'); /* the readings folder */
    if (!slash) {
        return false;
    }
    *slash = '\0';
    return common_path_join(out, size, folder, WORKSHOP_FILE_REALIZATION);
}

/* Checks a layer file: a realization must read, a reading must read and name the realization beside it (the one this transaction staged, the one in the
 * same pending folder, or the one already in place). */
static UrsatzStatus validate_layer(const WorkshopTxn *txn, const char *actual, const char *final_path, UrsatzError *error) {
    if (workshop_layers_is_realization_path(final_path)) {
        return workshop_layers_check_realization(actual, final_path, error);
    }
    char realization_final[PATH_BYTES];
    char realization_actual[PATH_BYTES];
    if (!realization_beside(final_path, realization_final, sizeof(realization_final)) || !realization_beside(actual, realization_actual, sizeof(realization_actual))) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s names no realization beside it", final_path);
    }
    const char *staged = workshop_txn_read_path(txn, realization_final);
    const char *use = strcmp(staged, realization_final) != 0 ? staged : common_path_exists(realization_actual) ? realization_actual : realization_final;
    return workshop_layers_check_reading(actual, final_path, use, error);
}

/* Checks one staged file as it will be at final_path. */
static UrsatzStatus validate_file(const WorkshopTxn *txn, const char *actual, const char *final_path, UrsatzError *error) {
    if (workshop_layers_is_realization_path(final_path) || workshop_layers_is_reading_path(final_path)) {
        return validate_layer(txn, actual, final_path, error);
    }
    const char *format = workshop_format_of_path(final_path);
    if (format) {
        cJSON *object = NULL;
        UrsatzStatus status = workshop_format_read(actual, format[0] == '\0' ? NULL : format, false, &object, error);
        if (status == URSATZ_OK && format[0] == '\0' && !is_body_format(common_json_string(object, "format"))) {
            status = ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s is not a body file", final_path);
        }
        cJSON_Delete(object);
        if (status != URSATZ_OK && error) {
            /* name the file the data folder will hold, not its pending name */
            char reason[256];
            snprintf(reason, sizeof(reason), "%.250s", error->message);
            ursatz_error_set(error, status, final_path, "%s was staged and does not read: %.200s", final_path, reason);
        }
        return status;
    }
    struct stat info;
    if (stat(actual, &info) != 0 || info.st_size <= 0) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s was staged empty or missing", final_path);
    }
    return URSATZ_OK;
}

static UrsatzStatus validate_entry(const WorkshopTxn *txn, const char *actual, const char *final_path, UrsatzError *error) {
    struct stat info;
    if (stat(actual, &info) != 0) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s was staged and is missing", final_path);
    }
    if (!S_ISDIR(info.st_mode)) {
        return validate_file(txn, actual, final_path, error);
    }
    CommonNames names = {0};
    UrsatzStatus status = common_dir_list(actual, &names, error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char child[PATH_BYTES];
        char virtual_path[PATH_BYTES];
        if (!common_path_join(child, sizeof(child), actual, names.items[i]) ||
            !common_path_join(virtual_path, sizeof(virtual_path), final_path, names.items[i])) {
            status = ursatz_error_set(error, URSATZ_ERR_IO, final_path, "a path under %s is too long", final_path);
        } else {
            status = validate_entry(txn, child, virtual_path, error);
        }
    }
    common_names_free(&names);
    return status;
}

static cJSON *record_of(const WorkshopTxn *txn) {
    char at[32];
    common_utc_now(at);
    unsigned long long a = (unsigned long long)time(NULL) * 1000003ULL + (unsigned long long)txn->seq * 7919ULL;
    unsigned long long b = (unsigned long long)getpid() * 2654435761ULL ^ ((unsigned long long)txn->seq << 17);
    char id[32];
    snprintf(id, sizeof(id), "cmd-%08llx%08llx", a & 0xffffffffULL, b & 0xffffffffULL);
    cJSON *record = cJSON_CreateObject();
    cJSON *writes = record ? cJSON_CreateArray() : NULL;
    cJSON *removes = record ? cJSON_CreateArray() : NULL;
    bool ok = record && writes && removes && cJSON_AddNumberToObject(record, "seq", (double)txn->seq) &&
              cJSON_AddStringToObject(record, "id", id) && cJSON_AddStringToObject(record, "command", txn->command) &&
              cJSON_AddStringToObject(record, "actor", txn->actor) && cJSON_AddStringToObject(record, "at", at) &&
              cJSON_AddStringToObject(record, "target", txn->target) && cJSON_AddNumberToObject(record, "base_version", (double)txn->base_version);
    for (size_t i = 0; ok && i < txn->write_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry && cJSON_AddStringToObject(entry, "pending", txn->writes[i].pending) &&
             cJSON_AddStringToObject(entry, "final", txn->writes[i].final);
        if (ok) {
            cJSON_AddItemToArray(writes, entry);
        } else {
            cJSON_Delete(entry);
        }
    }
    for (size_t i = 0; ok && i < txn->remove_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry && cJSON_AddStringToObject(entry, "removed", txn->removes[i].removed) &&
             cJSON_AddStringToObject(entry, "final", txn->removes[i].final);
        if (ok) {
            cJSON_AddItemToArray(removes, entry);
        } else {
            cJSON_Delete(entry);
        }
    }
    cJSON *headers = ok ? cJSON_Duplicate(txn->headers, 1) : NULL;
    if (ok && headers && cJSON_AddItemToObject(record, "writes", writes) && cJSON_AddItemToObject(record, "removes", removes) &&
        cJSON_AddItemToObject(record, "headers", headers)) {
        return record;
    }
    cJSON_Delete(headers);
    if (record && !cJSON_GetObjectItemCaseSensitive(record, "writes")) {
        cJSON_Delete(writes);
    }
    if (record && !cJSON_GetObjectItemCaseSensitive(record, "removes")) {
        cJSON_Delete(removes);
    }
    cJSON_Delete(record);
    return NULL;
}

/* Writes the header of an item (item.json with the record's seq) into the folder the item is in. */
UrsatzStatus workshop_txn_write_header(const cJSON *item, long long seq, UrsatzError *error) {
    char path[PATH_BYTES];
    const char *id = common_json_string(item, "id");
    const char *project = common_json_string(item, "project_id");
    if (!workshop_layout_item_file(project, id, WORKSHOP_FILE_ITEM, path, sizeof(path))) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, id, "the header of %s names no project", id);
    }
    cJSON *header = cJSON_Duplicate(item, 1);
    if (!header) {
        return URSATZ_ERR_ALLOC;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(header, "project_id");
    cJSON_DeleteItemFromObjectCaseSensitive(header, "journal_seq");
    cJSON_AddNumberToObject(header, "journal_seq", (double)seq);
    UrsatzStatus status = workshop_format_write(path, WORKSHOP_FORMAT_ITEM, header, error);
    cJSON_Delete(header);
    return status;
}

/* The index of the data folder: cache/index/ holds what the journal has committed so far. It is rebuilt on open, never read as a source of truth. */
static void write_index(long long seq) {
    char text[64];
    int length = snprintf(text, sizeof(text), "%lld\n", seq);
    if (common_dir_make(WORKSHOP_DIR_INDEX, NULL) == URSATZ_OK) {
        (void)common_file_write_atomic(WORKSHOP_DIR_INDEX "/journal-seq", text, (size_t)length, NULL);
    }
}

UrsatzStatus workshop_txn_commit(WorkshopTxn *txn, UrsatzError *error) {
    if (txn->write_count == 0 && txn->remove_count == 0 && cJSON_GetArraySize(txn->headers) == 0) {
        /* nothing was staged: no record */
        txn_free(txn);
        unlock();
        return URSATZ_OK;
    }
    CRASH(AFTER_STAGE);
    UrsatzStatus status = URSATZ_OK;
    for (size_t i = 0; status == URSATZ_OK && i < txn->write_count; i++) {
        status = validate_entry(txn, txn->writes[i].pending, txn->writes[i].final, error);
    }
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        return status;
    }
    CRASH(AFTER_VALIDATE);

    cJSON *record = record_of(txn);
    if (!record) {
        workshop_txn_abort(txn);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    status = workshop_journal_append(WORKSHOP_JOURNAL_FILE, record, BETWEEN_HOOK, error);
    cJSON_Delete(record);
    if (status != URSATZ_OK) {
        /* a record that is not whole is torn off by the next begin; nothing was renamed */
        workshop_txn_abort(txn);
        return status;
    }
    g_store.next_seq = txn->seq + 1;
    CRASH(AFTER_JOURNAL_SYNC);

    /* The change is committed: from here a failure is finished by the next open. */
    UrsatzStatus finish = URSATZ_OK;
    for (size_t i = 0; i < txn->write_count; i++) {
        if (rename(txn->writes[i].pending, txn->writes[i].final) != 0 && finish == URSATZ_OK) {
            finish = ursatz_error_set(error, URSATZ_ERR_INTERNAL, txn->writes[i].final, "cannot move %s into place", txn->writes[i].final);
        } else {
            char parent[PATH_BYTES];
            snprintf(parent, sizeof(parent), "%s", txn->writes[i].final);
            char *slash = strrchr(parent, '/');
            (void)common_dir_sync(slash ? (*slash = '\0', parent) : ".", NULL);
        }
        CRASH(AFTER_RENAME);
    }
    const cJSON *header = NULL;
    cJSON_ArrayForEach(header, txn->headers) {
        UrsatzStatus written = workshop_txn_write_header(header, txn->seq, error);
        if (written != URSATZ_OK && finish == URSATZ_OK) {
            finish = written;
        }
    }
    CRASH(AFTER_HEADERS);
    for (size_t i = 0; i < txn->remove_count; i++) {
        common_dir_remove(txn->removes[i].removed, NULL);
    }
    write_index(txn->seq);
    CRASH(AFTER_INDEX);
    workshop_command_context_set_last_seq(txn->seq);
    workshop_events_after_commit(txn->command, txn->target, txn->headers);
    txn_free(txn);
    unlock();
    return finish;
}
