#ifndef WORKSHOP_ITEMS_RECOVER_H
#define WORKSHOP_ITEMS_RECOVER_H

/*
 * Recovery of the data folder after a stop (R-643): run by workshop_open before any service starts. From the journal (items/journal.h) it
 *   - renames into place every pending entry (".pending-<seq>-<name>") a committed record lists, and deletes the pending entries of a
 *     record that was never committed;
 *   - deletes every removed entry (".removed-<seq>-<name>") of a committed record and renames back those of one that was not;
 *   - rewrites an item header whose journal_seq is older than the last committed record that sets it;
 *   - truncates a last journal line that has no newline;
 *   - deletes leftover "*.tmp-<pid>" files, the old "*.stage-*" folders and "source.upload" files;
 *   - rebuilds cache/index/.
 */

#include <stddef.h>

#include <common/status.h>

typedef struct {
    long pending_completed;
    long pending_deleted;
    long removed_restored;
    long removed_deleted;
    long headers_rewritten;
    long torn_lines;
    long temporaries_deleted;
} WorkshopRecoverReport;

/* data_dir is the folder (absolute or not); the records' paths are relative to it. */
UrsatzStatus workshop_recover(const char *data_dir, WorkshopRecoverReport *report, UrsatzError *error);

/* The report as one line of text, "" when nothing had to be recovered. */
void workshop_recover_summary(const WorkshopRecoverReport *report, char *out, size_t size);

#endif
