#ifndef URSATZ_READING_READING_JSON_H
#define URSATZ_READING_READING_JSON_H

#include <stdbool.h>

#include "reading/reading.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * A reading as JSON (R-629), described by reading/schema/reading.v1.schema.json:
 *   {"format": "ursatz-reading", "format_version": 1, "source": {"kind", "name", "producer_key", "stages", "knowledge_hash"},
 *    "target": {"work", "movement", "digest"}, "regions": [...], "chords": [...], "cadences": [...], "groups": [...],
 *    "motive_occurrences": [...], "meters": [...], "lines": [...], "voice_faults": [...]}
 * Each claim is {"id", "start", "end", "confidence": {"num", "den"} or null, "evidence": [note ids]} and the members of its kind. Times are
 * rationals {"num", "den"}. A region names its material by its K1 id and its strength by name; a chord reading its root, its chord
 * type by id, its bass, its members, the material it is borrowed from, its role and the chord it elides; a cadence its types by
 * cadence_type_name; a group its parent as the index of an earlier group or null. The detail (the chain a reading was built from) is
 * never written: a reading read from a file has none.
 */

#define URSATZ_READING_FORMAT_VERSION 1
#define READING_JSON_PATH_MAX 128
#define READING_JSON_MESSAGE_MAX 160

/* Filled on failure; pass NULL when the details are not wanted. */
typedef struct {
    char path[READING_JSON_PATH_MAX];
    char message[READING_JSON_MESSAGE_MAX];
} ReadingJsonError;

/* The reading as text: 2-space indent, members in the order above, a final newline. A reading whose target digest is "" is refused with
   "the reading names no realization version". NULL with *error set on failure. Caller frees. */
char *reading_to_json(const Reading *reading, ReadingJsonError *error);

/*
 * Reads a reading. Refusals at their paths: an unknown format or version, an unknown material or chord type id, an unknown cadence type
 * name, a parent index that is not an earlier group, and what reading_add_* refuses (a bad or repeated id, a span that ends before it
 * starts). REALIZATION_ERR_NULL for no text, no knowledge or no out, BAD_VALUE for malformed or refused content, ALLOC. *out is NULL on
 * failure.
 */
RealizationResult reading_from_json(const KnowledgeSnapshot *knowledge, const char *text, Reading **out, ReadingJsonError *error);

#endif
