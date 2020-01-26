#ifndef MUSIC_CORPUS_CORPUS_API_H
#define MUSIC_CORPUS_CORPUS_API_H

#include <stddef.h>

/*
 * Corpus public API: the stable surface external consumers (ursatz-gui's
 * backend, Generator) link against to use Corpus, exposing exactly the
 * operations corpus-minimal already implements internally
 * (corpus.h, persistence_corpus.h, persistence_corpus_membership.h).
 * This is the same kind of boundary Persistence already exposes via
 * persistence.h for Analyzer to link against.
 *
 * CorpusStoreHandle is opaque and deliberately does not expose
 * PersistenceHandle: Analyzer and Generator consume Corpus as peers and
 * must be able to link against it without also pulling in Persistence's
 * own types. Everything below the "opaque handle in, plain data out"
 * boundary (PersistenceHandle, the internal Corpus type) stays inside
 * corpus/*.c.
 *
 * No behavior beyond corpus-minimal is added here: creating a Corpus,
 * listing Corpora, and adding/removing/listing piece refs fail exactly
 * when the wrapped internals do (see corpus.h and
 * persistence_corpus_membership.h for the underlying fail-fast rules).
 */

typedef struct CorpusStoreHandle CorpusStoreHandle;

typedef enum {
    CORPUS_API_OK = 0,
    CORPUS_API_ERR_NULL_ARG,
    CORPUS_API_ERR_INVALID_ID,
    CORPUS_API_ERR_INVALID_NAME,
    CORPUS_API_ERR_OPEN_FAILED,
    CORPUS_API_ERR_DB,
    CORPUS_API_ERR_ALLOC,
    CORPUS_API_ERR_ALREADY_MEMBER,
    CORPUS_API_ERR_NOT_MEMBER
} CorpusApiResult;

/*
 * One Corpus's id and name, as plain owned strings -- the public API
 * exposes this data, not the internal Corpus type or its accessors.
 */
typedef struct {
    char *id;
    char *name;
} CorpusApiEntry;

/*
 * Opens (creating if absent) a Corpus store at path. On any failure,
 * *out is left untouched and no handle is allocated, mirroring
 * persistence_open's fail-fast contract.
 */
CorpusApiResult corpus_api_open(const char *path, CorpusStoreHandle **out);

/* Closes the store and frees handle. */
void corpus_api_close(CorpusStoreHandle *handle);

/* Creates a Corpus with caller-supplied id and name and persists it. */
CorpusApiResult corpus_api_create_corpus(CorpusStoreHandle *handle, const char *id, const char *name);

/*
 * Lists every Corpus currently stored. On success, *out_entries is a
 * newly allocated array of *out_count CorpusApiEntry values (each with
 * owned id/name strings); *out_count may be 0 with *out_entries left as
 * NULL (no corpora stored is not an error). Free the result with
 * corpus_api_free_corpus_list.
 */
CorpusApiResult corpus_api_list_corpora(CorpusStoreHandle *handle, CorpusApiEntry **out_entries, size_t *out_count);

/* Frees an entry array returned by corpus_api_list_corpora. Safe on NULL/0. */
void corpus_api_free_corpus_list(CorpusApiEntry *entries, size_t count);

/*
 * Adds/removes a piece reference (an already-persisted Interpretation/
 * Chord/Key's EntityID value) to/from a Corpus. Fail-fast: adding a
 * reference already present, or removing one not present, is an error
 * (CORPUS_API_ERR_ALREADY_MEMBER / CORPUS_API_ERR_NOT_MEMBER).
 */
CorpusApiResult corpus_api_add_piece_ref(CorpusStoreHandle *handle, const char *corpus_id, const char *piece_ref);
CorpusApiResult corpus_api_remove_piece_ref(CorpusStoreHandle *handle, const char *corpus_id, const char *piece_ref);

/*
 * Lists every piece_ref currently in the given Corpus. On success,
 * *out_refs is a newly allocated array of *out_count newly allocated,
 * NUL-terminated strings (caller frees each string and the array);
 * *out_count may be 0 with *out_refs left as NULL (an empty Corpus is
 * not an error).
 */
CorpusApiResult corpus_api_list_piece_refs(CorpusStoreHandle *handle, const char *corpus_id, char ***out_refs,
                                            size_t *out_count);

#endif
