#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_EXPOSITION_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_EXPOSITION_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "semantics/fugue_role.h"
#include "semantics/voice_part.h"
#include "standard-counterpoint/counterpoint_slots.h"

/*
 * The subject of every rule in high-baroque-fugue-v1: the opening of a fugue.
 * A fugue is a set of independent voices, each a slotted line (attacks, ties,
 * and rests, as in standard-counterpoint's counterpoint_slots.h) on one common
 * grid, plus the statements the caller identifies in them. Voices are listed
 * from the highest to the lowest, so a four-voice fugue is soprano, alto,
 * tenor, bass (SATB). Voices are numbered 0 upwards in that order, not in
 * the order they enter.
 *
 *   FugueStatement    a passage of one voice with a role (FugueRole): the voice,
 *                     the slot where it begins (the voice must begin a note
 *                     there), the role, the number of notes it has, and, for a
 *                     countersubject, which one it is (0 for the first, 1 for
 *                     the second, and so on: statements with the same number
 *                     are statements of the same countersubject). The number
 *                     of any other role is ignored
 *   FugueLimits       the numbers behind the rules that ask a writer to
 *                     "limit" something without saying how much
 *   FugueExposition   the voices, the statements, the number of slots in a
 *                     bar, the tonic of the key (or NULL when it is not known;
 *                     the rules that need it hold vacuously), whether the
 *                     countersubject is declared invertible at the octave,
 *                     and the limits (or NULL for the defaults)
 *
 * The statements are listed in the order they begin. A pitch is read as the
 * sounding pitch. Pitches are borrowed, not owned.
 */

#define FUGUE_MAX_VOICES 8
#define FUGUE_MAX_SLOTS 512
#define FUGUE_MAX_NOTES 512
#define FUGUE_MAX_STATEMENTS 64

typedef struct {
    size_t voice;
    size_t start;
    FugueRole role;
    size_t note_count;
    size_t number;
} FugueStatement;

/*
 *   max_altered_intervals   the most intervals of an answer that may differ
 *                           in generic size from the subject's (the answer is
 *                           altered as little as possible)
 */
typedef struct {
    size_t max_altered_intervals;
} FugueLimits;

static inline FugueLimits fugue_default_limits(void) {
    FugueLimits limits = {2};
    return limits;
}

typedef struct {
    const StandardCounterpointSlotLine *voices;
    size_t voice_count;
    const FugueStatement *statements;
    size_t statement_count;
    size_t slots_per_bar;
    const PitchSpelling *tonic;
    bool invertible;
    const FugueLimits *limits;
} FugueExposition;

/* A note of a voice: the slot it begins on, the slots it lasts (ties
   included), and its pitch. */
typedef struct {
    size_t start;
    size_t length;
    const PitchSpelling *pitch;
} FugueNote;

/*
 * True when the exposition has two to FUGUE_MAX_VOICES voices of the same
 * length (at most FUGUE_MAX_SLOTS), an attack or tie with a pitch for every
 * sounding slot, a rest with none, and no tie that does not continue the
 * same pitch; one to FUGUE_MAX_STATEMENTS statements, each of a valid role
 * and a voice in range that begins a note at its start slot and has at least
 * one note and enough notes after it, listed in the order they begin; and a
 * bar of at least one slot.
 */
bool fugue_exposition_valid(const FugueExposition *exposition);

/* The number of slots of every voice of a valid exposition; 0 otherwise. */
size_t fugue_exposition_slot_count(const FugueExposition *exposition);

/* The limits of the exposition: its own, or the defaults. */
FugueLimits fugue_exposition_limits(const FugueExposition *exposition);

/* The pitch sounding in `voice` at `slot` (an attack or a tie); NULL for a
   rest, an invalid exposition, or an index out of range. */
const PitchSpelling *fugue_pitch_at(const FugueExposition *exposition, size_t voice, size_t slot);

/* True when `voice` begins a new note at `slot`. */
bool fugue_attacks_at(const FugueExposition *exposition, size_t voice, size_t slot);

/* The notes of `voice` in order. Sets *count and returns true; false (both
   untouched) for an invalid exposition, a voice out of range, or more notes
   than `capacity`. */
bool fugue_voice_notes(const FugueExposition *exposition, size_t voice, FugueNote *out, size_t capacity,
                       size_t *count);

/* The notes of statement `index`: its voice's notes from the one beginning at
   its start slot, `note_count` of them. Sets *count to the statement's note
   count and returns true; false for an invalid exposition or index, or more
   notes than `capacity`. */
bool fugue_statement_notes(const FugueExposition *exposition, size_t index, FugueNote *out, size_t capacity,
                           size_t *count);

/* The voice part (soprano, alto, tenor, bass) of voice `voice` in an
   exposition of four voices. False (*out untouched) for any other number of
   voices or a voice out of range. */
bool fugue_voice_part(const FugueExposition *exposition, size_t voice, VoicePart *out);

#endif
