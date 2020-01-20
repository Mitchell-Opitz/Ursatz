#ifndef MUSIC_CORPUS_PERSISTENCE_CORPUS_H
#define MUSIC_CORPUS_PERSISTENCE_CORPUS_H

#include <stddef.h>

#include "corpus/corpus.h"
#include "persistence/persistence.h"

/*
 * Corpus persistence: stores/reloads a Corpus (corpus/corpus.h) into
 * the `corpora` table. The table is created on demand (CREATE TABLE IF
 * NOT EXISTS) the first time this module touches a handle, the same
 * idempotent-schema pattern persistence.c uses, since Corpus lives in
 * a separate module from Persistence and its table cannot be part of
 * persistence.c's own up-front schema without persistence/ depending
 * on corpus/ (which the architecture guard forbids).
 *
 * Membership (piece references) is a separate table/module --
 * persistence_corpus_membership.h.
 */

typedef enum {
    PERSISTENCE_CORPUS_OK = 0,
    PERSISTENCE_CORPUS_ERR_NULL_ARG,
    PERSISTENCE_CORPUS_ERR_DB,
    PERSISTENCE_CORPUS_ERR_ALLOC
} PersistenceCorpusResult;

PersistenceCorpusResult persistence_store_corpus(PersistenceHandle *handle, const Corpus *corpus);

PersistenceCorpusResult persistence_load_corpus(PersistenceHandle *handle, const char *corpus_id, Corpus **out_corpus);

/*
 * Lists every Corpus currently stored. On success, *out_corpora is a
 * newly allocated array of *out_count owned Corpus pointers (caller
 * destroys each with corpus_destroy and frees the array); *out_count
 * may be 0 with *out_corpora left as NULL (no corpora stored is not an
 * error).
 */
PersistenceCorpusResult persistence_list_corpora(PersistenceHandle *handle, Corpus ***out_corpora,
                                                  size_t *out_count);

#endif
