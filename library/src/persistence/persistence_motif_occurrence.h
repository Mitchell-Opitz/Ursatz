#ifndef MUSIC_PERSISTENCE_PERSISTENCE_MOTIF_OCCURRENCE_H
#define MUSIC_PERSISTENCE_PERSISTENCE_MOTIF_OCCURRENCE_H

#include <stddef.h>

#include "persistence/persistence.h"
#include "structure/motif_occurrence.h"

/*
 * Stores and loads MotifOccurrences (structure/motif_occurrence.h) in the `motif_occurrences` table, with the region event ids in
 * order in the child table `motif_occurrence_events`. A stored MotifOccurrence is a descriptive record: it promotes nothing to a Rule
 * and carries no authority.
 *
 * References by EntityID only. The Motif an occurrence belongs to (motif_id) and the event ids of its
 * region are text values, not foreign keys, and are not checked against any other table: the Motif may be stored
 * later, in another repository, or never. The only foreign key is from `motif_occurrence_events` to its own parent.
 *
 * The region ids reload in the order they were stored. Storing writes the parent row and the whole list in one
 * savepoint, so a store that fails partway leaves no rows. Storing an id that already exists is
 * PERSISTENCE_MOTIF_OCCURRENCE_ERR_DUPLICATE_ID and changes nothing (the older repositories report a duplicate key as
 * the generic ERR_DB; this one names it). Loading an id that is not stored is PERSISTENCE_MOTIF_OCCURRENCE_ERR_NOT_FOUND and
 * *out is untouched.
 *
 * persistence_list_motif_occurrences_by_motif_id returns every occurrence of one Motif id in insertion order (the order
 * stored, stable across calls and reopens): *out is a newly allocated array of newly created MotifOccurrences, *out_count
 * long; the caller motif_occurrence_destroy()s each and free()s the array. No match is OK with *out NULL and *out_count 0.
 * On any other result the outputs are untouched.
 */

typedef enum {
    PERSISTENCE_MOTIF_OCCURRENCE_OK = 0,
    PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG,
    PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB,
    PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC,
    PERSISTENCE_MOTIF_OCCURRENCE_ERR_NOT_FOUND,
    PERSISTENCE_MOTIF_OCCURRENCE_ERR_DUPLICATE_ID
} PersistenceMotifOccurrenceResult;

PersistenceMotifOccurrenceResult persistence_store_motif_occurrence(PersistenceHandle *handle,
                                                          const MotifOccurrence *motif_occurrence);

PersistenceMotifOccurrenceResult persistence_load_motif_occurrence(PersistenceHandle *handle, const char *motif_occurrence_id,
                                                         MotifOccurrence **out_motif_occurrence);

PersistenceMotifOccurrenceResult persistence_list_motif_occurrences_by_motif_id(PersistenceHandle *handle, const char *motif_id,
                                                                  MotifOccurrence ***out_motif_occurrences,
                                                                  size_t *out_count);

#endif
