#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_APPLIED_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_APPLIED_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_chord.h"
#include "semantics/applied_chord_kind.h"

/*
 * Applied chords for standard-harmony-v1 (Tonicization). Tonicization is the process of making
 * a non-tonic chord sound like a temporary tonic. It is done with chromatic chords called
 * applied chords, a secondary dominant (V or V7) or a secondary leading-tone chord (vii
 * diminished, vii diminished 7, or rarely vii half diminished 7) borrowed from the temporary
 * key (AppliedChordKind, semantics/applied_chord_kind.h). It is not a modulation: nothing here
 * tracks a temporary key, and the label is the whole result.
 *
 * An applied chord is a kind, a quality, and the target, the diatonic chord being tonicized,
 * given as its scale degree in the home key (an HarmonyAppliedChord is not a HarmonyChord: it
 * is chromatic). Its label has a slash: the chord before it is the applied chord's identity in
 * the temporary key and the one after it the target, read with "of": V7/V is "five seven of
 * five".
 *
 * Any major or minor triad can be tonicized, so every diatonic chord can be a target except
 * the diminished and augmented ones, and the tonic (V of I is just V):
 *
 *   major   ii, iii, IV, V, vi              (not I, and not vii diminished)
 *   minor   III, iv, V, VI, VII             (not i, and not ii diminished or vii diminished)
 *
 * The qualities, by kind:
 *
 *   SECONDARY_DOMINANT       MAJOR_TRIAD, DOMINANT_SEVENTH
 *   SECONDARY_LEADING_TONE   DIMINISHED_TRIAD, FULLY_DIMINISHED_SEVENTH, HALF_DIMINISHED_SEVENTH
 *                            (rare in classical music)
 */

typedef enum {
    HARMONY_APPLIED_MAJOR_TRIAD = 0,
    HARMONY_APPLIED_DOMINANT_SEVENTH,
    HARMONY_APPLIED_DIMINISHED_TRIAD,
    HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH,
    HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH,
    HARMONY_APPLIED_QUALITY_COUNT
} HarmonyAppliedQuality;

typedef struct {
    AppliedChordKind kind;
    HarmonyAppliedQuality quality;
    int target;
} HarmonyAppliedChord;

/* True for the five qualities above; false for any other value. */
bool harmony_applied_quality_valid(HarmonyAppliedQuality quality);

/* "major-triad" "dominant-seventh" "diminished-triad" "fully-diminished-seventh"
   "half-diminished-seventh"; NULL for an invalid quality. */
const char *harmony_applied_quality_name(HarmonyAppliedQuality quality);

/* True for the three seventh chord qualities; false for the triads and an invalid value. */
bool harmony_applied_quality_is_seventh(HarmonyAppliedQuality quality);

/* The kind a quality belongs to: major triad and dominant seventh are secondary dominants, the
   others secondary leading-tone chords. False (*out untouched) for an invalid quality or NULL. */
bool harmony_applied_quality_kind(HarmonyAppliedQuality quality, AppliedChordKind *out);

/* True when the scale degree can be tonicized in the mode (the lists above). */
bool harmony_applied_target_valid(HarmonyMode mode, int degree);

/* True for a valid mode, a kind and quality that belong together, and a target that can be
   tonicized in the mode. False for NULL. */
bool harmony_applied_valid(HarmonyMode mode, const HarmonyAppliedChord *chord);

/* The label, for example "V7/V", "V/vi", "viio7/V", or "vii%7/ii" (o for diminished, % for half
   diminished), with the target as its Roman numeral in the mode, written to `out` with its
   terminating zero. False (*out untouched) for an invalid chord or mode, a NULL pointer, or a
   buffer too small. */
bool harmony_applied_label(HarmonyMode mode, const HarmonyAppliedChord *chord, char *out, size_t size);

/* The semitones above the tonic of the scale degree (1 to 7) in the mode: major 0 2 4 5 7 9 11,
   natural minor 0 2 3 5 7 8 10. -1 for an invalid mode or degree. */
int harmony_applied_scale_semitone(HarmonyMode mode, int degree);

/*
 * Analyzing tonicization. A chromatic chord is an applied chord when it is a major triad or a
 * dominant seventh (V or V7), or a diminished triad, fully diminished seventh, or half-diminished
 * seventh (vii diminished), whose target, the chord it is V or vii of, is a diatonic chord that
 * can be tonicized. Given the chord's root, as semitones above the tonic (any integer, taken
 * modulo 12), and its quality: the quality names the kind; the target's root is a fifth below
 * the root of a dominant (the root is its V) or a semitone above the root of a leading-tone
 * chord (the root is its vii); the target is the diatonic chord of that root, its Roman numeral
 * in the home key. False (*out untouched) when the root is not that of a tonicizable diatonic
 * chord (for example the root of V itself, which tonicizes the tonic and is just V), for an
 * invalid mode or quality, and for NULL.
 *
 * Whether a chromatic note belongs to the chord or is an embellishing tone is the analyst's.
 */
bool harmony_applied_analyze(HarmonyMode mode, int root_semitone, HarmonyAppliedQuality quality,
                             HarmonyAppliedChord *out);

#endif
