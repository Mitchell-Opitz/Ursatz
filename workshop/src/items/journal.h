#ifndef WORKSHOP_ITEMS_JOURNAL_H
#define WORKSHOP_ITEMS_JOURNAL_H

/*
 * The journal (R-643, spec/target-architecture.md 10.2): journal/journal.jsonl, one line per committed change to the data folder. The first
 * line is {"format": "ursatz-journal", "format_version": 1}; each line after it is a record
 *   {seq, id ("cmd-<16 hex>"), command, actor, at, target, base_version, writes: [{pending, final}], removes: [{removed, final}],
 *    headers: [<item header, with its project_id>]}
 * with paths relative to the data folder. A record is committed once its line is whole, newline included; a last line without its newline
 * is a record that was being written and is not committed. The next sequence number is one more than the last committed record's.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

#define WORKSHOP_JOURNAL_FILE "journal/journal.jsonl"

typedef struct {
    cJSON **records; /* the committed records, in order */
    size_t count;
    long long last_seq; /* the last committed record's seq, 0 when there is none */
    bool torn;          /* the file ends in a line without its newline */
    size_t good_bytes;  /* the bytes up to and including the last newline */
} WorkshopJournal;

/* Reads the journal at path (an absent file is an empty journal). FORMAT, naming the file, for a first line that is not the header or a whole line that is not a record. */
UrsatzStatus workshop_journal_read(const char *path, WorkshopJournal *out, UrsatzError *error);

void workshop_journal_free(WorkshopJournal *journal);

/* Appends record as the next line of the journal at path (made, with its header, when absent): the record's bytes, then between() (may be
 * NULL), then the newline and an fsync. The record is committed when this returns OK. */
UrsatzStatus workshop_journal_append(const char *path, const cJSON *record, void (*between)(void), UrsatzError *error);

/* Cuts a torn last line off the journal at path. */
UrsatzStatus workshop_journal_truncate(const char *path, size_t good_bytes, UrsatzError *error);

#endif
