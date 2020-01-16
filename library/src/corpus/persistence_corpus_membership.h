#ifndef MUSIC_CORPUS_PERSISTENCE_CORPUS_MEMBERSHIP_H
#define MUSIC_CORPUS_PERSISTENCE_CORPUS_MEMBERSHIP_H

#include <stddef.h>

#include "persistence/persistence.h"

/*
 * Corpus membership: the set of persisted-piece references currently
 * grouped by a Corpus, stored durably in the `corpus_members` table
 * (corpus_id, piece_ref) with a uniqueness constraint on the pair.
 *
 * piece_ref is an opaque caller-supplied string -- the EntityID value
 * of an already-persisted Interpretation/Chord/Key -- the same
 * reference-by-id-only, no-existence-validation treatment
 * FrameworkReference already gets in persistence_interpretation.h.
 * This module never resolves piece_ref to the piece it names.
 *
 * Fail-fast, no silent no-ops: adding a reference already present in a
 * Corpus is an error, and removing a reference not present is an
 * error, consistent with the library's existing fail-fast convention
 * (e.g. persistence_open never returns a partial handle).
 */

typedef enum {
    PERSISTENCE_CORPUS_MEMBERSHIP_OK = 0,
    PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG,
    PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB,
    PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALLOC,
    PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALREADY_MEMBER,
    PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NOT_MEMBER
} PersistenceCorpusMembershipResult;

PersistenceCorpusMembershipResult corpus_add_piece_ref(PersistenceHandle *handle, const char *corpus_id,
                                                        const char *piece_ref);

PersistenceCorpusMembershipResult corpus_remove_piece_ref(PersistenceHandle *handle, const char *corpus_id,
                                                           const char *piece_ref);

/*
 * Lists every piece_ref currently in the given Corpus. On success,
 * *out_refs is a newly allocated array of *out_count newly allocated,
 * NUL-terminated strings (caller frees each string and the array);
 * *out_count may be 0 with *out_refs left as NULL (an empty Corpus is
 * not an error).
 */
PersistenceCorpusMembershipResult corpus_list_piece_refs(PersistenceHandle *handle, const char *corpus_id,
                                                          char ***out_refs, size_t *out_count);

#endif
