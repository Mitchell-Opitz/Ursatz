#ifndef MUSIC_CORPUS_CORPUS_H
#define MUSIC_CORPUS_CORPUS_H

#include "identity/entity_id.h"

/*
 * Corpus (L9, minimal): a named collection with a stable id, grouping
 * references to already-persisted pieces (each reference is the
 * EntityID of a persisted Interpretation/Chord/Key -- see
 * persistence_corpus_membership.h). Corpus itself never stores or
 * embeds piece data; membership is reference-only, the same
 * referenced-not-embedded pattern Interpretation's target/context/
 * evidence fields already use.
 *
 * This in-memory type holds only id and name. Membership (the set of
 * piece references currently in a Corpus) is not an in-memory field --
 * it lives entirely in Persistence (persistence_corpus_membership.h),
 * consistent with this being a minimal, no-aggregation pass: nothing
 * here computes or caches membership, it is queried fresh from storage
 * each time.
 *
 * Names are not required to be unique -- two Corpora may share a name;
 * only id disambiguates.
 */

typedef struct Corpus Corpus;

typedef enum {
    CORPUS_OK = 0,
    CORPUS_ERR_NULL_ID,
    CORPUS_ERR_NULL_NAME,
    CORPUS_ERR_ALLOC
} CorpusResult;

/* Takes ownership of id on success (destroyed by corpus_destroy). name is copied. */
CorpusResult corpus_create(EntityID *id, const char *name, Corpus **out);
void corpus_destroy(Corpus *corpus);

const EntityID *corpus_id(const Corpus *corpus);
const char *corpus_name(const Corpus *corpus);

#endif
