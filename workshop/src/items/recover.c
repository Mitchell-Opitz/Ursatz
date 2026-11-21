#include "recover.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include <ursatz_workshop/workshop_api.h>

#include "items/formats.h"
#include "items/journal.h"
#include "items/layout.h"
#include "items/txn.h"

#define PATH_BYTES 1024

typedef struct {
    const char *root;
    long long last_seq;
    WorkshopRecoverReport *report;
} Scan;

static bool under(const char *root, const char *relative, char *out, size_t size) {
    return common_path_join(out, size, root, relative);
}

/* Parses ".pending-<seq>-<rest>" or ".removed-<seq>-<rest>"; *rest points into name. */
static bool parse_aside(const char *name, const char *kind, long long *seq, const char **rest) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), ".%s-", kind);
    size_t length = strlen(prefix);
    if (strncmp(name, prefix, length) != 0) {
        return false;
    }
    const char *digits = name + length;
    char *end = NULL;
    long long value = strtoll(digits, &end, 10);
    if (end == digits || *end != '-' || end[1] == '\0') {
        return false;
    }
    *seq = value;
    *rest = end + 1;
    return true;
}

/* "<anything>.tmp-<pid>": an atomic write that stopped before its rename. */
static bool is_temporary(const char *name) {
    const char *marker = strstr(name, ".tmp-");
    if (!marker || marker == name) {
        return false;
    }
    for (const char *c = marker + 5; *c; c++) {
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
    }
    return marker[5] != '\0';
}

static void scan(Scan *state, const char *folder) {
    CommonNames names = {0};
    if (common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        return;
    }
    for (size_t i = 0; i < names.count; i++) {
        const char *name = names.items[i];
        char path[PATH_BYTES];
        long long seq = 0;
        const char *rest = NULL;
        if (!common_path_join(path, sizeof(path), folder, name)) {
            continue;
        }
        if (parse_aside(name, "pending", &seq, &rest)) {
            /* a pending entry the record pass did not complete: its record was never committed (or does not list it) */
            common_dir_remove(path, NULL);
            state->report->pending_deleted++;
        } else if (parse_aside(name, "removed", &seq, &rest)) {
            if (seq > state->last_seq) {
                char original[PATH_BYTES];
                if (common_path_join(original, sizeof(original), folder, rest) && !common_path_exists(original) &&
                    rename(path, original) == 0) {
                    state->report->removed_restored++;
                }
            } else {
                common_dir_remove(path, NULL);
                state->report->removed_deleted++;
            }
        } else if (is_temporary(name) || strstr(name, ".stage-") != NULL || strcmp(name, "source.upload") == 0) {
            common_dir_remove(path, NULL);
            state->report->temporaries_deleted++;
        } else if (strncmp(name, ".pre-", 5) != 0 && strncmp(name, ".migrate-", 9) != 0 && strcmp(name, "journal") != 0) {
            struct stat info;
            if (lstat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
                scan(state, path);
            }
        }
    }
    common_names_free(&names);
}

/* The record pass: the pending entries of committed records go into place, their removed entries go. */
static void complete_records(const char *root, const WorkshopJournal *journal, WorkshopRecoverReport *report) {
    for (size_t r = 0; r < journal->count; r++) {
        const cJSON *record = journal->records[r];
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(record, "writes")) {
            char pending[PATH_BYTES];
            char final_path[PATH_BYTES];
            if (!under(root, common_json_string(entry, "pending"), pending, sizeof(pending)) ||
                !under(root, common_json_string(entry, "final"), final_path, sizeof(final_path)) || !common_path_exists(pending)) {
                continue;
            }
            if (common_path_is_dir(pending) && common_path_exists(final_path)) {
                common_dir_remove(pending, NULL); /* a folder put into place already */
                report->pending_deleted++;
            } else if (rename(pending, final_path) == 0) {
                report->pending_completed++;
            }
        }
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(record, "removes")) {
            char removed[PATH_BYTES];
            if (under(root, common_json_string(entry, "removed"), removed, sizeof(removed)) && common_path_exists(removed)) {
                common_dir_remove(removed, NULL);
                report->removed_deleted++;
            }
        }
    }
}

/* The header pass: the last committed record that sets an item's header is the header, when the file is older. */
static void rewrite_headers(const char *root, const WorkshopJournal *journal, WorkshopRecoverReport *report) {
    CommonNames done = {0};
    for (size_t r = journal->count; r > 0; r--) {
        const cJSON *record = journal->records[r - 1];
        long long seq = 0;
        common_json_long(record, "seq", &seq);
        const cJSON *header = NULL;
        cJSON_ArrayForEach(header, cJSON_GetObjectItemCaseSensitive(record, "headers")) {
            const char *id = common_json_string(header, "id");
            const char *project = common_json_string(header, "project_id");
            char dir[PATH_BYTES];
            char relative[PATH_BYTES];
            char path[PATH_BYTES];
            if (common_names_contains(&done, id)) {
                continue; /* a later record set it */
            }
            common_names_add(&done, id);
            if (!workshop_layout_item_dir(project, id, relative, sizeof(relative)) || !under(root, relative, dir, sizeof(dir)) ||
                !common_path_is_dir(dir) || !workshop_layout_item_file(project, id, WORKSHOP_FILE_ITEM, relative, sizeof(relative)) ||
                !under(root, relative, path, sizeof(path))) {
                continue; /* the item is gone: a later record removed it */
            }
            cJSON *stored = NULL;
            long long stored_seq = -1;
            if (workshop_format_read(path, WORKSHOP_FORMAT_ITEM, false, &stored, NULL) == URSATZ_OK) {
                if (!common_json_long(stored, "journal_seq", &stored_seq)) {
                    stored_seq = 0;
                }
            }
            cJSON_Delete(stored);
            if (stored_seq < seq) {
                cJSON *item = cJSON_Duplicate(header, 1);
                if (item) {
                    cJSON_DeleteItemFromObjectCaseSensitive(item, "project_id");
                    cJSON_DeleteItemFromObjectCaseSensitive(item, "journal_seq");
                    cJSON_AddNumberToObject(item, "journal_seq", (double)seq);
                    if (workshop_format_write(path, WORKSHOP_FORMAT_ITEM, item, NULL) == URSATZ_OK) {
                        report->headers_rewritten++;
                    }
                    cJSON_Delete(item);
                }
            }
        }
    }
    common_names_free(&done);
}

UrsatzStatus workshop_recover(const char *data_dir, WorkshopRecoverReport *report, UrsatzError *error) {
    memset(report, 0, sizeof(*report));
    char journal_path[PATH_BYTES];
    if (!under(data_dir, WORKSHOP_JOURNAL_FILE, journal_path, sizeof(journal_path))) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, data_dir, "the data folder path is too long");
    }
    WorkshopJournal journal;
    UrsatzStatus status = workshop_journal_read(journal_path, &journal, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (journal.torn) {
        status = workshop_journal_truncate(journal_path, journal.good_bytes, error);
        report->torn_lines++;
    }
    if (status == URSATZ_OK) {
        complete_records(data_dir, &journal, report);
        Scan state = {data_dir, journal.last_seq, report};
        scan(&state, data_dir);
        rewrite_headers(data_dir, &journal, report);
        char index[PATH_BYTES];
        char marker[PATH_BYTES];
        char text[64];
        if (under(data_dir, WORKSHOP_DIR_INDEX, index, sizeof(index)) && under(data_dir, WORKSHOP_DIR_INDEX "/journal-seq", marker, sizeof(marker))) {
            common_dir_remove(index, NULL);
            if (journal.last_seq > 0 && common_dir_make(index, NULL) == URSATZ_OK) {
                int length = snprintf(text, sizeof(text), "%lld\n", journal.last_seq);
                (void)common_file_write_atomic(marker, text, (size_t)length, NULL);
            }
        }
    }
    workshop_journal_free(&journal);
    return status;
}

void workshop_recover_summary(const WorkshopRecoverReport *report, char *out, size_t size) {
    long total = report->pending_completed + report->pending_deleted + report->removed_restored + report->removed_deleted +
                 report->headers_rewritten + report->torn_lines + report->temporaries_deleted;
    if (total == 0) {
        snprintf(out, size, "%s", "");
        return;
    }
    snprintf(out, size,
             "recovered the data folder: %ld pending entries completed and %ld deleted, %ld removed entries restored and %ld deleted, %ld item "
             "headers rewritten, %ld torn journal lines cut, %ld leftovers deleted",
             report->pending_completed, report->pending_deleted, report->removed_restored, report->removed_deleted, report->headers_rewritten,
             report->torn_lines, report->temporaries_deleted);
}
