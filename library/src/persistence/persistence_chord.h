#ifndef MUSIC_PERSISTENCE_CHORD_H
#define MUSIC_PERSISTENCE_CHORD_H

#include "interpretation/chord.h"
#include "persistence/persistence.h"

/*
 * Chord persistence: stores/reloads a Chord (interpretation/chord.h)
 * into the `chords` table, a thin FK'd wrapper over the shared
 * `interpretations` table (persistence_interpretation.h) that holds
 * the embedded Interpretation.
 *
 * Chord itself exposes no id of its own beyond its embedded
 * Interpretation's id (chord_interpretation(chord) -> interpretation_id
 * (...)); this module uses that same EntityID value as both the
 * `interpretations` row key and the `chords` row key, rather than
 * inventing a second identifier Chord doesn't have.
 */

typedef enum {
    PERSISTENCE_CHORD_OK = 0,
    PERSISTENCE_CHORD_ERR_NULL_ARG,
    PERSISTENCE_CHORD_ERR_DB,
    PERSISTENCE_CHORD_ERR_ALLOC
} PersistenceChordResult;

PersistenceChordResult persistence_store_chord(PersistenceHandle *handle, const Chord *chord);

/* chord_id is the Chord's embedded Interpretation's own EntityID value. */
PersistenceChordResult persistence_load_chord(PersistenceHandle *handle, const char *chord_id, Chord **out_chord);

#endif
