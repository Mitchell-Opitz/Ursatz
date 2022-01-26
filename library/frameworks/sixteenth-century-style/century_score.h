#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_SCORE_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_SCORE_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "standard-counterpoint/counterpoint_slots.h"

/*
 * The subject of every rule in sixteenth-century-style-v1: a polyphonic
 * score of any number of voices. The style works on an unbarred tactus,
 * so a score is a set of voices (slotted lines, as in standard-counterpoint's
 * counterpoint_slots.h: attacks, ties, and rests) on one common grid, plus
 * the length of the tactus in slots:
 *
 *   voices            the voices, every one with the same number of slots
 *   voice_count       1 to SIXTEENTH_CENTURY_MAX_VOICES
 *   slots_per_tactus  the slots in one tactus (the half note of the modern
 *                     editions); an even number of at least two
 *   tonic             the final of the mode, or NULL when it is not known
 *                     (the rules that need it hold vacuously)
 *   phrygian          true for a Phrygian mode, where the leading note is
 *                     not raised
 *
 * A pitch is read as the sounding pitch. Pitches are borrowed, not owned.
 *
 * Positions are measured from the first slot. Two tactus make a bar, so a
 * slot is on a STRONG tactus when it starts an even-numbered tactus, on a
 * WEAK tactus when it starts an odd-numbered one, and SUB tactus when it
 * falls between tactus starts.
 */

#define SIXTEENTH_CENTURY_MAX_VOICES 8
#define SIXTEENTH_CENTURY_MAX_SLOTS 512
#define SIXTEENTH_CENTURY_MAX_NOTES 512

typedef struct {
    const StandardCounterpointSlotLine *voices;
    size_t voice_count;
    size_t slots_per_tactus;
    const PitchSpelling *tonic;
    bool phrygian;
} SixteenthCenturyScore;

typedef enum {
    SIXTEENTH_CENTURY_POSITION_STRONG = 0,
    SIXTEENTH_CENTURY_POSITION_WEAK,
    SIXTEENTH_CENTURY_POSITION_SUB
} SixteenthCenturyPosition;

/* A note of a voice: the slot it begins on, the slots it lasts (ties
   included), and its pitch. */
typedef struct {
    size_t start;
    size_t length;
    const PitchSpelling *pitch;
} SixteenthCenturyNote;

/* True when the score has one to MAX_VOICES voices of the same length (at
   most MAX_SLOTS), an even tactus of at least two slots, an attack or tie
   with a pitch for every sounding slot, a rest with none, and no tie that
   does not continue the same pitch. */
bool sixteenth_century_score_valid(const SixteenthCenturyScore *score);

/* The number of slots of every voice of a valid score; 0 for an invalid one. */
size_t sixteenth_century_score_slot_count(const SixteenthCenturyScore *score);

/* The position of `slot` in a valid score. Slots past the end are measured
   the same way. */
SixteenthCenturyPosition sixteenth_century_position_at(const SixteenthCenturyScore *score, size_t slot);

/* The pitch sounding in `voice` at `slot` (an attack or a tie); NULL for a
   rest, an invalid score, or an index out of range. */
const PitchSpelling *sixteenth_century_pitch_at(const SixteenthCenturyScore *score, size_t voice, size_t slot);

/* True when `voice` begins a new note at `slot`. */
bool sixteenth_century_attacks_at(const SixteenthCenturyScore *score, size_t voice, size_t slot);

/* The notes of `voice` in order. Sets *count and returns true; false (both
   untouched) for an invalid score, a voice out of range, or more notes than
   `capacity`. */
bool sixteenth_century_voice_notes(const SixteenthCenturyScore *score, size_t voice,
                                   SixteenthCenturyNote *out, size_t capacity, size_t *count);

#endif
