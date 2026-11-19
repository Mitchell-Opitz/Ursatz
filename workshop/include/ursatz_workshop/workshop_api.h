#ifndef URSATZ_WORKSHOP_WORKSHOP_API_H
#define URSATZ_WORKSHOP_WORKSHOP_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The Ursatz workshop (R-641): the one process that holds the data folder and the services over it, above the two apps
 * (the Analyzer and the Composer) and below the GUI. An application opens it once with its settings, asks it for what it needs,
 * and closes it. Every call answers with the common status set (<common/status.h>, R-639).
 *
 * The data folder is locked to one workshop for as long as it is open: a second open, in this or another process, is
 * URSATZ_ERR_CONFLICT.
 */

typedef struct Workshop Workshop;

/*
 * The data folder layout and its formats (R-642). The folder holds data-folder.json (format WORKSHOP_FORMAT_DATA_FOLDER); every
 * file the workshop owns carries "format" (one of the names below) and "format_version" (a whole number from 1, the version
 * this build writes and the newest it reads). Files other parts own keep their own versions: the analysis document, the style
 * model, the motive link cache and a composed version's request.json.
 */
#define WORKSHOP_LAYOUT_VERSION 1
#define WORKSHOP_FORMAT_DATA_FOLDER "ursatz-data-folder"
#define WORKSHOP_FORMAT_CONFIG "ursatz-config"
#define WORKSHOP_FORMAT_PROJECT "ursatz-project"
#define WORKSHOP_FORMAT_ITEM "ursatz-item"
#define WORKSHOP_FORMAT_ITEM_VERSION "ursatz-item-version"
#define WORKSHOP_FORMAT_MOTIF_BODY "ursatz-motif-body"
#define WORKSHOP_FORMAT_THEME_BODY "ursatz-theme-body"
#define WORKSHOP_FORMAT_PIECE_BODY "ursatz-piece-body"
#define WORKSHOP_FORMAT_OP_CHECKS "ursatz-op-checks"
#define WORKSHOP_FORMAT_SUBJECT "ursatz-subject"
#define WORKSHOP_FORMAT_LIBRARY_PIECE "ursatz-library-piece"
#define WORKSHOP_FORMAT_COLLECTION "ursatz-collection"
#define WORKSHOP_FORMAT_STYLE "ursatz-style"
#define WORKSHOP_FORMAT_LINK_RUN "ursatz-link-run"
#define WORKSHOP_FORMAT_RENDER "ursatz-render"
#define WORKSHOP_FORMAT_JOURNAL "ursatz-journal" /* the first line of journal/journal.jsonl; the file is lines, not one document */

/*
 * The settings of a workshop. A 0 member, or a NULL one, is unset: the layer below answers. data_dir is the data folder, config_path
 * the settings file (default "<data_dir>/ursatz.json"), port the HTTP port, http_threads the server's request threads and
 * job_workers the threads that run long jobs.
 */
typedef struct {
    const char *data_dir;
    const char *config_path;
    int port;
    int http_threads;
    int job_workers;
} WorkshopConfig;

/*
 * The settings a workshop opens with, from four layers, each over the one before: the defaults (data_dir ".", port 8765,
 * http_threads 16, job_workers 2), the settings file (format "ursatz-config", format_version 1, members port 1 to 65535,
 * http_threads 1 to 64 and job_workers 1 to 8; an absent file gives no settings, a bad one is URSATZ_ERR_FORMAT naming the
 * member), the environment (URSATZ_DATA, URSATZ_PORT, URSATZ_JOB_WORKERS) and flags (the members of the WorkshopConfig the caller
 * parsed, any 0 or NULL one unset). *out is filled with every member set; its strings point into storage (storage_size bytes,
 * the caller's), so it is valid while storage is. URSATZ_ERR_REQUEST for a value out of range or storage too small.
 */
UrsatzStatus workshop_settings_resolve(const WorkshopConfig *flags, WorkshopConfig *out, char *storage, size_t storage_size,
                                       UrsatzError *error);

/*
 * Opens the workshop on the data folder of resolved (a WorkshopConfig with every member set, as workshop_settings_resolve
 * gives it): creates the folder when it is absent, locks it, and starts what the workshop holds. *out is the workshop on
 * URSATZ_OK and NULL otherwise.
 */
UrsatzStatus workshop_open(const WorkshopConfig *resolved, Workshop **out, UrsatzError *error);

/*
 * Migrates a data folder of the GUI before the workshop (layout 0: pieces/, materials/, subjects/, collections/, styles/, links/) to
 * layout 1 (R-642). The folder must not be open by a workshop. It first asks the data check (items/data_check.h) and refuses a folder the
 * GUI's one-time migrations never ran on, with the check's message. A dry run writes nothing and reports every planned move and
 * rewrite. A real run copies the whole folder to backup_dir (NULL: "<data_dir>-backup-<YYYYMMDDTHHMMSSZ>" beside it), compares file
 * count and sizes and stops on a difference, builds layout 1 in <data_dir>/.migrate-1/, rewrites every body as format 1 (the mode, motif
 * key and progression-fill members written new), renames a Library piece's ursatz_commit to producer_key (R-554), moves the motif
 * render caches to cache/renders, moves the old top-level entries into <data_dir>/.pre-1/ and the new ones into place, and writes
 * data-folder.json last. A run that stopped before the last moves leaves .migrate-1/ without its .complete marker: it is deleted and the
 * migration runs again; one that stopped during the last moves is finished from where it stopped. A layout 1 folder is a no-op, and an
 * empty or absent folder is made layout 1. *report (caller deletes; may be NULL) is {dry_run, data_dir, from_layout, to_layout, changed,
 * backup, counts {...}, moves [{from, to}]}.
 */
typedef struct {
    bool dry_run;
    const char *backup_dir;
} WorkshopMigrateOptions;

UrsatzStatus workshop_migrate(const char *data_dir, const WorkshopMigrateOptions *options, cJSON **report, UrsatzError *error);

/*
 * Queries and commands (R-646). Every read of the GUI is a query and every change a command; the GUI and, later, the command line give
 * a name and a JSON object and get a reply. A query changes no file outside cache/. A command (workshop_command) is
 *   {"command": <name>, "actor": <text>, "target": <id or null>, "base_version": <n, absent for none>, "params": {...}}
 * and is journaled under its name with its actor; the commands of an item that has versions need base_version (the version the caller
 * read) and are refused with URSATZ_ERR_CONFLICT, "changed since you read it", when it is not the item's current one. An unknown name,
 * a missing actor or a params member of the wrong type is URSATZ_ERR_REQUEST naming its path. The names are in
 * workshop/src/coordinator/dispatch.c.
 *
 * The reply is what the caller asked for: json (a tree the caller owns, the members of the answer) or bytes (a file, with its
 * content_type and, for a download, filename). A command's reply holds journal_seq (the record it wrote; 0 for a command that
 * writes none or whose job writes it later). The reply is also filled when the status is not URSATZ_OK and the answer has members
 * (a refused delete lists what uses the item; a stale base version gives current_version).
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    const char *name;
} WorkshopBytes;

typedef struct {
    cJSON *json;
    uint8_t *bytes;
    size_t size;
    char content_type[64];
    char filename[128];
} WorkshopReply;

void workshop_reply_destroy(WorkshopReply *reply);

/*
 * bytes is the file a few queries read (composer.import-motif: a MIDI file); NULL for the others.
 */
UrsatzStatus workshop_query(Workshop *workshop, const char *name, const cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply,
                            UrsatzError *error);

/* Stops what the workshop holds, releases the data folder and frees the workshop. NULL is a no-op. */
void workshop_close(Workshop *workshop);

/*
 * The versions the workshop was built with, as a JSON object (caller deletes): composer_version, api_version, producer_key,
 * composition_key and plan_format_version from composer_api_versions, analyzer_version and analyzer_producer_key from the Analyzer,
 * and data_dir.
 */
UrsatzStatus workshop_status(Workshop *workshop, cJSON **out, UrsatzError *error);

/* What the open recovered after a stop in the middle of a change (R-643): one line of counts, "" when there was nothing to recover. Valid until
 * workshop_close. */
const char *workshop_recovery_summary(const Workshop *workshop);

/* The settings the workshop opened with. Valid until workshop_close. */
const char *workshop_data_dir(const Workshop *workshop);
int workshop_http_threads(const Workshop *workshop);
int workshop_job_workers(const Workshop *workshop);
int workshop_port(const Workshop *workshop);

#endif
