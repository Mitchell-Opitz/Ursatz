#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CHORD_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CHORD_H

#include <stdbool.h>

/*
 * A chord as standard-harmony-v1 reads it: plain data, not a Roman numeral and
 * not pitches. The caller says which chord is meant; this module never finds
 * it. A chord is named by the scale degree of its root, counted from the tonic
 * of the key (1 to 7), its inversion (0 root position, 1 first inversion, 2
 * second, 3 third, which only a seventh chord has), and whether it is a seventh
 * chord (a triad with a seventh above the root added, as in V7). Which seventh
 * it is follows from the degree and the mode, and a chord's function is the same
 * with or without the seventh.
 *
 * The one degree whose chord depends on the mode is 7. In a major key the chord
 * on 7 is the diminished vii (its root is the leading tone). In a minor key
 * there are two: vii (the root is the raised, leading-tone seventh degree) and
 * VII (the root is the lowered seventh degree, a whole step below the tonic).
 * `leading_tone` marks the first; it is meaningful only for degree 7 and is
 * ignored in a major key, where degree 7 is always the leading tone.
 *
 * The tonic triad in second inversion (the notes of I over its fifth) is not always a
 * tonic chord. After a predominant and before V, it is the cadential 6/4: not a
 * standalone triad but an embellishment of V, the combination of two embellishing
 * tones, a sixth and a fourth above the bass sol. It is labeled by how it functions
 * (as V, with its figures 6-4 and their resolution to 5-3), not by its spelling, and
 * has dominant function, not tonic. `cadential` marks it; the caller says which
 * second inversion tonic chords are one.
 */

typedef enum {
    HARMONY_MODE_MAJOR = 0,
    HARMONY_MODE_MINOR,
    HARMONY_MODE_COUNT
} HarmonyMode;

typedef struct {
    int degree;
    int inversion;
    bool leading_tone;
    bool seventh;
    bool cadential;
} HarmonyChord;

/* True for the two modes above; false for any other value. */
bool harmony_mode_valid(HarmonyMode mode);

/* "major" "minor"; NULL for an invalid mode. */
const char *harmony_mode_name(HarmonyMode mode);

/* True for a degree from 1 to 7, an inversion from 0 to 3 (3 only for a seventh
   chord), a leading_tone that is false unless the degree is 7, and a cadential
   that is false unless the chord is the triad on degree 1 in second inversion.
   False for NULL. */
bool harmony_chord_valid(const HarmonyChord *chord);

#endif
