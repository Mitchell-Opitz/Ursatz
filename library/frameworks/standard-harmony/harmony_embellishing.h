#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_EMBELLISHING_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_EMBELLISHING_H

#include <stdbool.h>

#include "semantics/beat_strength.h"
#include "semantics/non_chord_tone.h"

/*
 * Embellishing tones for standard-harmony-v1 (Embellishing Tones). An embellishing
 * tone is a note that embellishes a note belonging to the chord. In nearly all
 * cases it is the middle note of a three-note gesture whose first and last notes
 * are consonant with the bass (chord tones), and in almost all cases it is not a
 * chord tone itself. They fall into three categories:
 *
 *   by step        PASSING       approached by step, left by step in the same
 *                                direction (ascending or descending)
 *                  NEIGHBOR      approached by step, left by step in the opposite
 *                                direction (an upper or a lower neighbor)
 *   by leap        APPOGGIATURA  approached by leap, left by step in the opposite
 *                                direction
 *                  ESCAPE_TONE   approached by step, left by leap in the opposite
 *                                direction
 *   static notes   SUSPENSION    approached by a static note, left by step down
 *                  RETARDATION   approached by a static note, left by step up
 *                  PEDAL         approached and left by static notes
 *                  ANTICIPATION  a two-note gesture: a chord tone heard early, as a
 *                                non-chord tone, before the note that follows it
 *                                (the same pitch)
 *
 * The subject is the gesture: two or three notes in order, each a diatonic step
 * (harmony_steps.h: the key's scale from a tonic, the octave kept so the direction
 * of motion is known), whether it belongs to the chord sounding with it, and its
 * beat strength. For a three-note gesture the embellishing tone is the middle note;
 * for an anticipation (two notes) it is the first. A step is one diatonic step, and
 * a leap is more than one; chromatic steps are not modeled. The double neighbor and
 * the changing tone are NonChordTone values too, but they are not in this chapter
 * and are not classified here (frameworks/standard-counterpoint).
 */

#define HARMONY_GESTURE_MAX_NOTES 3

typedef struct {
    int note_count; /* 2 (an anticipation) or 3 */
    int steps[HARMONY_GESTURE_MAX_NOTES];
    bool in_chord[HARMONY_GESTURE_MAX_NOTES];
    BeatStrength beat[HARMONY_GESTURE_MAX_NOTES];
} HarmonyGesture;

/* True for two or three notes, each with a valid beat strength. */
bool harmony_gesture_valid(const HarmonyGesture *gesture);

/* The embellishing tone the gesture makes, by how it is approached and left: see
   the list above. A two-note gesture is an anticipation when its notes are the same
   pitch. False (*out untouched) for any other motion, for an invalid gesture, and
   for NULL. Beats and chord membership are not looked at (see
   harmony_embellishing_rules.h). */
bool harmony_embellishing_classify(const HarmonyGesture *gesture, NonChordTone *out);

#endif
