#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_PIECE_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "imitative_role.h"
#include "ostinato_type.h"
#include "standard-counterpoint/counterpoint_slots.h"
#include "standard-counterpoint/slot_note.h"

/*
 * The subject of every rule in baroque-ground-bass-v1: a piece built on a
 * repeating bass. A piece is a set of voices, each a slotted line (attacks,
 * ties, and rests, as in standard-counterpoint's counterpoint_slots.h) on one
 * common grid, one of which is the bass that carries the ground, plus the
 * imitations the caller identifies in them.
 *
 *   GroundStatement   one statement of an imitation: the voice, the slot where
 *                     it begins (the voice must begin a note there), its role
 *                     (ImitativeRole: the dux leads, the comes follows), and the
 *                     number of notes it has
 *   GroundLimits      the numbers behind the rules that ask a writer to
 *                     "limit" something without saying how much
 *   GroundBassPiece   the voices; which of them is the bass; the number of
 *                     slots in one iteration of the ground and in a bar; the
 *                     number of beats in a bar; the kind of ostinato
 *                     (OstinatoType); the imitation statements (there may be
 *                     none); and the limits (or NULL for the defaults)
 *
 * The statements are listed in the order they begin. A pitch is read as the
 * sounding pitch. Pitches are borrowed, not owned. A bar has one meter for the
 * whole piece; a change of meter within it is not modeled.
 */

#define GROUND_MAX_VOICES 8
#define GROUND_MAX_SLOTS 2048
#define GROUND_MAX_NOTES 2048
#define GROUND_MAX_STATEMENTS 256

typedef struct {
    size_t voice;
    size_t start;
    ImitativeRole role;
    size_t note_count;
} GroundStatement;

/*
 *   max_altered_notes   the most notes of an iteration of the ground that may
 *                       differ in pitch from the first iteration ("exactly, or
 *                       nearly so")
 */
typedef struct {
    size_t max_altered_notes;
} GroundLimits;

static inline GroundLimits ground_default_limits(void) {
    GroundLimits limits = {1};
    return limits;
}

typedef struct {
    const StandardCounterpointSlotLine *voices;
    size_t voice_count;
    size_t bass_voice;
    size_t slots_per_iteration;
    size_t slots_per_bar;
    size_t beats_per_bar;
    OstinatoType type;
    const GroundStatement *statements;
    size_t statement_count;
    const GroundLimits *limits;
} GroundBassPiece;

/* A note of a voice (standard-counterpoint/slot_note.h): the slot it begins on, the slots it lasts (ties included), and its pitch. */
typedef StandardCounterpointSlotNote GroundNote;

/*
 * True when the piece has two to GROUND_MAX_VOICES voices of the same length
 * (at most GROUND_MAX_SLOTS), an attack or tie with a pitch for every sounding
 * slot, a rest with none, and no tie that does not continue the same pitch; a
 * bass voice in range; an iteration, a bar, and a number of beats of at least
 * one slot or beat; a valid ostinato type; and at most GROUND_MAX_STATEMENTS
 * statements, each of a valid role and a voice in range that begins a note at
 * its start slot and has at least one note and enough notes after it, listed
 * in the order they begin. Whether the length is a whole number of iterations
 * is a rule (ground_rules.h), not a condition of validity.
 */
bool ground_piece_valid(const GroundBassPiece *piece);

/* The number of slots of every voice of a valid piece; 0 otherwise. */
size_t ground_piece_slot_count(const GroundBassPiece *piece);

/* The number of whole iterations of the ground in a valid piece (its length
   divided by the iteration, rounded down); 0 otherwise. */
size_t ground_piece_iteration_count(const GroundBassPiece *piece);

/* The limits of the piece: its own, or the defaults. */
GroundLimits ground_piece_limits(const GroundBassPiece *piece);

/* The pitch sounding in `voice` at `slot` (an attack or a tie); NULL for a
   rest, an invalid piece, or an index out of range. */
const PitchSpelling *ground_pitch_at(const GroundBassPiece *piece, size_t voice, size_t slot);

/* True when `voice` begins a new note at `slot`. */
bool ground_attacks_at(const GroundBassPiece *piece, size_t voice, size_t slot);

/* The notes of `voice` in order. Sets *count and returns true; false (both
   untouched) for an invalid piece, a voice out of range, or more notes than
   `capacity`. */
bool ground_voice_notes(const GroundBassPiece *piece, size_t voice, GroundNote *out, size_t capacity,
                        size_t *count);

/* The notes of statement `index`: its voice's notes from the one beginning at
   its start slot, `note_count` of them. Sets *count to the statement's note
   count and returns true; false for an invalid piece or index, or more notes
   than `capacity`. */
bool ground_statement_notes(const GroundBassPiece *piece, size_t index, GroundNote *out, size_t capacity,
                            size_t *count);

#endif
