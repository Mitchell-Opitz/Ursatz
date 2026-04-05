#ifndef MUSIC_PERSISTENCE_PERSISTENCE_PHRASE_OCCURRENCE_H
#define MUSIC_PERSISTENCE_PERSISTENCE_PHRASE_OCCURRENCE_H

#include <stddef.h>

#include "persistence/persistence.h"
#include "structure/phrase_occurrence.h"

/*
 * Stores and loads PhraseOccurrences (structure/phrase_occurrence.h) in the `phrase_occurrences` table, with the region event ids in
 * order in the child table `phrase_occurrence_events`. A stored PhraseOccurrence is a descriptive record: it promotes nothing to a Rule
 * and carries no authority.
 *
 * References by EntityID only. The Phrase an occurrence belongs to (phrase_id) and the event ids of its
 * region are text values, not foreign keys, and are not checked against any other table: the Phrase may be stored
 * later, in another repository, or never. The only foreign key is from `phrase_occurrence_events` to its own parent.
 *
 * The region ids reload in the order they were stored. Storing writes the parent row and the whole list in one
 * savepoint, so a store that fails partway leaves no rows. Storing an id that already exists is
 * PERSISTENCE_PHRASE_OCCURRENCE_ERR_DUPLICATE_ID and changes nothing (the older repositories report a duplicate key as
 * the generic ERR_DB; this one names it). Loading an id that is not stored is PERSISTENCE_PHRASE_OCCURRENCE_ERR_NOT_FOUND and
 * *out is untouched.
 *
 * persistence_list_phrase_occurrences_by_phrase_id returns every occurrence of one Phrase id in insertion order (the order
 * stored, stable across calls and reopens): *out is a newly allocated array of newly created PhraseOccurrences, *out_count
 * long; the caller phrase_occurrence_destroy()s each and free()s the array. No match is OK with *out NULL and *out_count 0.
 * On any other result the outputs are untouched.
 */

typedef enum {
    PERSISTENCE_PHRASE_OCCURRENCE_OK = 0,
    PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG,
    PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB,
    PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC,
    PERSISTENCE_PHRASE_OCCURRENCE_ERR_NOT_FOUND,
    PERSISTENCE_PHRASE_OCCURRENCE_ERR_DUPLICATE_ID
} PersistencePhraseOccurrenceResult;

PersistencePhraseOccurrenceResult persistence_store_phrase_occurrence(PersistenceHandle *handle,
                                                          const PhraseOccurrence *phrase_occurrence);

PersistencePhraseOccurrenceResult persistence_load_phrase_occurrence(PersistenceHandle *handle, const char *phrase_occurrence_id,
                                                         PhraseOccurrence **out_phrase_occurrence);

PersistencePhraseOccurrenceResult persistence_list_phrase_occurrences_by_phrase_id(PersistenceHandle *handle, const char *phrase_id,
                                                                  PhraseOccurrence ***out_phrase_occurrences,
                                                                  size_t *out_count);

#endif
