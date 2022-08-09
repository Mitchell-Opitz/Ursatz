#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Tonic prolongation for standard-harmony-v1 (Prolonging Tonic at Phrase
 * Beginnings with V6 and Inverted V7s). To prolong a harmony is to make its
 * influence last longer than a single chord, and phrase beginnings in Western
 * classical music typically prolong tonic to establish the home key. Holding or
 * repeating the chord prolongs it, but the more interesting and common way is to
 * use other chords between instances of the tonic: alternating tonic with V6 or an
 * inverted V7. The tonic's influence is felt more strongly because it is on
 * stronger beats than the V and appears at least once in root position, whereas the
 * V is in a weaker inversion.
 *
 * The progressions of this chapter share three traits: they are three chords long;
 * the first and last chords are I or I6; and the middle chord is V6 or an inverted
 * V7 (V6/5, V4/3, V4/2), which resolves by the principles of the V7 resolution. The
 * four bass lines, and the progressions that go with them (the only ones modeled):
 *
 *   DO_TI_DO   do ti do   I  V6/5  I    (V6 also, though less common). ti in the bass
 *                         resolves to do
 *   DO_RE_MI   do re mi   I  V4/3  I6   the most common V4/3: re in the bass goes to
 *                         mi
 *   DO_RE_DO   do re do   I  V4/3  I    less common: re in the bass may go to do or mi
 *   DO_FA_MI   do fa mi   I  V4/2  I6   fa in the bass is the chordal seventh and
 *                         resolves down to mi
 *
 * A progression is three HarmonyChords (harmony_chord.h): V6 is a triad on degree 5
 * in first inversion, and V6/5, V4/3, and V4/2 are seventh chords on degree 5 in
 * first, second, and third inversion. I is the triad on degree 1 in root position and
 * I6 in first inversion.
 */

typedef enum {
    HARMONY_PROLONGATION_DO_TI_DO = 0,
    HARMONY_PROLONGATION_DO_RE_MI,
    HARMONY_PROLONGATION_DO_RE_DO,
    HARMONY_PROLONGATION_DO_FA_MI,
    HARMONY_PROLONGATION_LINE_COUNT
} HarmonyProlongationLine;

/* True for the four lines above; false for any other value. */
bool harmony_prolongation_line_valid(HarmonyProlongationLine line);

/* "do-ti-do" "do-re-mi" "do-re-do" "do-fa-mi"; NULL for an invalid line. */
const char *harmony_prolongation_line_name(HarmonyProlongationLine line);

/* The three bass scale degrees of the line: 1 7 1, 1 2 3, 1 2 1, 1 4 3. False
   (*out untouched) for an invalid line or NULL. */
bool harmony_prolongation_line_bass(HarmonyProlongationLine line, int out[3]);

/* The line a progression of three chords makes: I, then V6, V6/5, V4/3, or V4/2, then
   I or I6, as listed above. False (*out untouched) for any other three chords, for a
   chord marked cadential or an invalid one, and for NULL. */
bool harmony_prolongation_of(const HarmonyChord chords[3], HarmonyProlongationLine *out);

#endif
