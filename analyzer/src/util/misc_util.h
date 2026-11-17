#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "model/time/musical_time.h"

#define FNV1A_OFFSET_BASIS 14695981039346656037ULL

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

/* A new EntityID with the same value as id; NULL on failure. */
EntityID *entity_id_copy(const EntityID *id);

/* The end (start plus duration) of the latest-ending note, or 0/1 with no notes, written to *out. False when a sum
   overflows (*out is untouched). */
bool notes_last_end(const NoteEvent *const *events, size_t count, MusicalTime *out);

/* Folds length bytes into a 64-bit FNV-1a hash; start from FNV1A_OFFSET_BASIS. */
uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length);

#endif
