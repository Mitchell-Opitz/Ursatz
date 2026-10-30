#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "events/note_event.h"
#include "identity/entity_id.h"
#include "time/musical_time.h"

#define FNV1A_OFFSET_BASIS 14695981039346656037ULL

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

/* a/b compared with c/d for positive denominators: negative, zero or positive. */
int fraction_compare(long long a, long long b, long long c, long long d);

/* A new EntityID with the same value as id; NULL on failure. */
EntityID *entity_id_copy(const EntityID *id);

/* The end (start plus duration) of the latest-ending note, or 0/1 with no notes. The caller destroys it. NULL on
   failure. */
MusicalTime *notes_last_end(const NoteEvent *const *events, size_t count);

/* Folds length bytes into a 64-bit FNV-1a hash; start from FNV1A_OFFSET_BASIS. */
uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length);

#endif
