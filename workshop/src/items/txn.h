#ifndef WORKSHOP_ITEMS_TXN_H
#define WORKSHOP_ITEMS_TXN_H

/*
 * The transaction through which every change to the data folder commits (R-643, spec/target-architecture.md 10.2). A change is written as
 * pending entries beside where it will go (".pending-<seq>-<name>"), checked, recorded in the journal (items/journal.h) and only then
 * renamed into place; an entry it removes is renamed aside first (".removed-<seq>-<name>") and deleted after the record. A stop at any
 * step leaves the old state or the new one, never a mix: workshop_open recovers (items/recover.h) from the journal.
 *
 * One transaction is open at a time per process: workshop_txn_begin takes the store's lock and workshop_txn_commit or workshop_txn_abort
 * releases it, so a writer begins its transaction first, before any lock of its own service (a transaction is the outermost lock), and
 * a function that changes more than one kind of file (an item delete drops subject links) is handed the open transaction.
 *
 * Paths are relative to the data folder, which is the process's working directory once a server starts (items/layout.h).
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

typedef struct WorkshopStore WorkshopStore;
typedef struct WorkshopTxn WorkshopTxn;

/* The store is the journal of the data folder and the lock over it; there is one per process, and NULL passed to workshop_txn_begin means it. */

/*
 * Opens a transaction: command is the name of the command (B11 lists them: "item.save", "library.import"...), actor who gave it ("michelle"),
 * target what it changes (an id, or "" for none) and base_version the version it was made against (0 for none). NULL when the journal cannot
 * be read; the lock is held until commit or abort.
 */
WorkshopTxn *workshop_txn_begin(WorkshopStore *store, const char *command, const char *actor, const char *target, long base_version);

/* Makes the folder the caller writes the new entry's files into ("<parent of final_path>/.pending-<seq>-<name>") and gives its path in
 * *out_pending (valid until commit). The folder becomes final_path, which must not exist. */
UrsatzStatus workshop_txn_stage_folder(WorkshopTxn *txn, const char *final_path, const char **out_pending);

/* A folder the caller made already (a composed version's files) becomes the pending folder of final_path: it is renamed to its pending name. */
UrsatzStatus workshop_txn_adopt_folder(WorkshopTxn *txn, const char *folder, const char *final_path, const char **out_pending);

/* Writes data to the pending file of final_path (atomically); it replaces final_path when there is one. */
UrsatzStatus workshop_txn_stage_file(WorkshopTxn *txn, const char *final_path, const void *data, size_t size);

/* Stamps object as format, checks it and stages it as final_path; the object is unchanged afterwards. */
UrsatzStatus workshop_txn_stage_json(WorkshopTxn *txn, const char *final_path, const char *format, cJSON *object);

/* Renames final_path (a file or a folder; an absent one is no-op) to its removed name; it is deleted once the record is committed. */
UrsatzStatus workshop_txn_remove(WorkshopTxn *txn, const char *final_path);

/* Sets the header of an item: item holds id and project_id and the header members (name, type, current_version, created_at, updated_at);
 * commit writes it as item.json with the record's seq as its journal_seq, and the record keeps it so recovery can write it again. */
UrsatzStatus workshop_txn_set_header(WorkshopTxn *txn, const cJSON *item);

/*
 * Checks every staged file by the reader of its format (items/formats.h; a file of a kind the workshop does not own, such as MIDI, for
 * presence and size), appends the record, renames each pending entry into place, writes the headers, deletes the removed entries and
 * updates the index, then releases the lock. URSATZ_ERR_FORMAT naming the file when a staged file does not read: nothing is changed then
 * (the transaction is aborted). After the record is written a failure is INTERNAL and the change is finished by the next open.
 */
UrsatzStatus workshop_txn_commit(WorkshopTxn *txn, UrsatzError *error);

/* Deletes the pending entries, restores the removed ones and releases the lock. NULL is a no-op. */
void workshop_txn_abort(WorkshopTxn *txn);

/* Writes the header of an item (the members of item plus journal_seq = seq) as its item.json, in the project folder item names. Commit and
 * recovery (items/recover.h) use it. */
UrsatzStatus workshop_txn_write_header(const cJSON *item, long long seq, UrsatzError *error);

/* The file name of a pending or removed entry for final_path in transaction seq (kind is "pending" or "removed"). false when it does not fit. */
bool workshop_txn_aside_path(const char *kind, long long seq, const char *final_path, char *out, size_t size);

#ifdef URSATZ_WORKSHOP_TEST_HOOKS
/* The steps of a commit a test can stop at. The process leaves with _exit(70) at the nth occurrence (from 1) of the point, counted since the call. */
typedef enum {
    WORKSHOP_CRASH_NONE = 0,
    WORKSHOP_CRASH_AFTER_STAGE,
    WORKSHOP_CRASH_AFTER_VALIDATE,
    WORKSHOP_CRASH_BEFORE_JOURNAL_SYNC,
    WORKSHOP_CRASH_AFTER_JOURNAL_SYNC,
    WORKSHOP_CRASH_AFTER_RENAME,
    WORKSHOP_CRASH_AFTER_HEADERS,
    WORKSHOP_CRASH_AFTER_INDEX
} WorkshopCrashPoint;

void workshop_test_set_crash_point(WorkshopCrashPoint point, unsigned nth);
#endif

#endif
