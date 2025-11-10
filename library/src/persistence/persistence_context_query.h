#ifndef MUSIC_PERSISTENCE_CONTEXT_QUERY_H
#define MUSIC_PERSISTENCE_CONTEXT_QUERY_H

#include <stddef.h>

#include "persistence/persistence.h"

/*
 * Reads back the persisted interpretations that belong to one context (an
 * Interpretation's optional context_id, interpretation/interpretation.h).
 * By convention a piece is a context: the interpretations an analysis of a
 * piece produced carry that piece's id as their context_id, so a context id
 * is how a caller finds "everything persisted about this piece".
 *
 * Results are ids in the order the interpretations were stored, which is the
 * order the analysis produced them (piece order). An unknown or empty
 * context is not an error: it yields zero ids. Ids are returned as owned
 * strings; free the list with persistence_free_id_list.
 *
 * Read-only: nothing here writes to the store.
 */

typedef enum {
    PERSISTENCE_QUERY_OK = 0,
    PERSISTENCE_QUERY_ERR_NULL_ARG,
    PERSISTENCE_QUERY_ERR_DB,
    PERSISTENCE_QUERY_ERR_ALLOC
} PersistenceQueryResult;

/*
 * Ids of the Chords whose embedded Interpretation has this context_id, in
 * store order -- each usable with persistence_load_chord. On success
 * *out_ids is a newly allocated array of *out_count owned strings (NULL
 * when *out_count is 0).
 */
PersistenceQueryResult persistence_list_chord_ids_for_context(PersistenceHandle *handle, const char *context_id,
                                                              char ***out_ids, size_t *out_count);

/*
 * Ids of the stored Interpretations with this context_id and this claim
 * value type (Interpretation's claim Value's type string, e.g.
 * "cadence-detection-top-candidate"), in store order -- each usable with
 * persistence_load_interpretation. Same output convention as above.
 */
PersistenceQueryResult persistence_list_interpretation_ids_for_context(PersistenceHandle *handle,
                                                                       const char *context_id,
                                                                       const char *claim_value_type,
                                                                       char ***out_ids, size_t *out_count);

/*
 * The raw claim content of each stored Interpretation with this context_id
 * and claim value type, in store order. A claim's content is opaque to
 * persistence (an Interpretation's claim Value); this returns the bytes
 * verbatim so a reader that knows the format of a claim type can decode it
 * without touching the Value type itself. On success *out_contents is a newly
 * allocated array of *out_count entries (NULL when *out_count is 0); free it
 * with persistence_free_claim_contents.
 */
typedef struct {
    void *bytes;
    size_t size;
} PersistenceClaimContent;

PersistenceQueryResult persistence_list_claim_contents_for_context(PersistenceHandle *handle,
                                                                   const char *context_id,
                                                                   const char *claim_value_type,
                                                                   PersistenceClaimContent **out_contents,
                                                                   size_t *out_count);

void persistence_free_claim_contents(PersistenceClaimContent *contents, size_t count);

/* Frees a list returned by either id function above. Safe on NULL. */
void persistence_free_id_list(char **ids, size_t count);

#endif
