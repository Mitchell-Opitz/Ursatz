#ifndef URSATZ_REALIZATION_REALIZATION_JSON_H
#define URSATZ_REALIZATION_REALIZATION_JSON_H

#include <stdbool.h>

#include "realization/realization.h"
#include "realization/work.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * A realization as JSON (R-627), described by realization/schema/realization.v1.schema.json: the document
 * {"format": "ursatz-realization", "format_version": 1, "work": {...}} holds one work with its movements, each with its meter, tempo,
 * home region, key signatures, markers, played spans, compound parts, lines, scoring assignments and performer parts. Times are
 * rationals {"num", "den"}, as the plan writes them. A member with nothing to say is written null or [], so the text of two equal
 * works is equal: the digest is the FNV-1a 64 of the compact text (realization_digest), the name of this version of the realization
 * that a reading names (R-629).
 *
 * A defaulted meter or tempo map is not written (the movement reads back with none); a tempo is written as its quarter notes per
 * minute, a rational. Material and region ids are the knowledge snapshot's: the reader resolves them and refuses an unknown one.
 */

#define URSATZ_REALIZATION_FORMAT_VERSION 1
#define REALIZATION_JSON_PATH_MAX 128
#define REALIZATION_JSON_MESSAGE_MAX 160

/* Filled on failure; pass NULL when the details are not wanted. */
typedef struct {
    char path[REALIZATION_JSON_PATH_MAX]; /* where the problem is, e.g. "work.movements[0].lines[1].notes[2].id"; "" for the document */
    char message[REALIZATION_JSON_MESSAGE_MAX];
} RealizationJsonError;

/* The work as text: 2-space indent, members in the order of the document above, and a final newline. NULL with *error set on failure. Caller frees. */
char *realization_work_to_json(const Work *work, RealizationJsonError *error);

/* The same document without any space or newline. NULL with *error set on failure. Caller frees. */
char *realization_work_to_compact_json(const Work *work, RealizationJsonError *error);

/*
 * Reads a work. The checks are the group's own (line_add_note, performer_part_add_note, movement_add_assignment and the rest), at the
 * path of the member: an unknown format, a format_version other than 1, a bad or duplicate id, an unknown material, a region that is not
 * tonal, an assignment naming a line or part the movement lacks. REALIZATION_ERR_NULL for no text, no knowledge or no out, BAD_VALUE for
 * malformed or refused content (the error says what), ALLOC. *out is NULL on failure.
 */
RealizationResult realization_work_from_json(const KnowledgeSnapshot *knowledge, const char *text, Work **out, RealizationJsonError *error);

/* The digest of the work: FNV-1a 64 of its compact text, 16 lowercase hexadecimal digits. False (out untouched) when the work cannot be written. */
bool realization_digest(const Work *work, char out[17]);

#endif
