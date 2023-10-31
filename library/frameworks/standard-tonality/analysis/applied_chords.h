#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_APPLIED_CHORDS_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_APPLIED_CHORDS_H

#include <stdbool.h>
#include <stddef.h>

#include "chord_input.h"
#include "roman_numeral_adapter.h"

/*
 * Applied chords (the standard-tonality analysis adapter, step 3). Chord identification is diatonic only, so
 * a segment holding a V/V (D F# A in C) arrives as the ii it nearly is, with the F# a leftover tone. This
 * re-checks a segment's diatonic reading against the applied chords and relabels it when the evidence is
 * there. It reads the segment's 12 pitch-class weights (chord_input.h) and its neighbours.
 *
 * CANDIDATES, for each diatonic target x:
 *   major key  x in {ii, iii, IV, V, vi}
 *   minor key  x in {III, iv, V, VI, VII}   (the natural minor's degrees, with V the major dominant)
 * the applied dominant V/x (a major triad on the fifth above x's root), V7/x (the dominant seventh), the
 * applied leading-tone chord vii-diminished/x (a diminished triad on the semitone below x's root) and the
 * fully diminished seventh vii-diminished-7/x. The tonic is not a target (it is not applied) and neither is
 * the diminished degree. Labels use the existing AppliedChordKind (semantics/applied_chord_kind.h):
 * SECONDARY_DOMINANT for V and V7, SECONDARY_LEADING_TONE for the two diminished chords.
 *
 * SCORING is the L2.5 formula (frameworks/common-practice-minimal/weighted_identification.h), with the same
 * defaults, applied to the candidate's pitch classes over the passed weights:
 *
 *     score = (weight of the chord's pitch classes / total weight)
 *             - missing_third_penalty   if the chord's third has no weight at all        (default 0.15)
 *             + bass_chord_tone_bonus   if the bass pitch class is a chord tone          (default 0.05)
 *
 * A candidate is only eligible if its root sounds. The segment's own diatonic chord is scored by the same
 * formula, so the two are comparable. (The constants are a copy: a framework does not include another.)
 *
 * RELABEL. The segment is relabelled as applied only when ALL hold:
 *   1. its CHROMATIC share, the weight on pitch classes outside the key (the major scale; in minor the
 *      natural minor scale with the raised leading tone) over the total, is at least chromatic_min
 *      (default 0.15);
 *   2. the best-scoring applied candidate scores strictly above the diatonic chord (a tie keeps the
 *      diatonic label; among equal candidates the earlier in the order target ascending, V, V7, vii-dim,
 *      vii-dim-7 wins);
 *   3. RESOLUTION: the chord of the next segment that is not a repeat of this one (same root) has the
 *      target's root, or the target's SUBMEDIANT root (a deceptive-style resolution: the major submediant
 *      of the target, nine semitones above its root, is vi of x). Segments are taken in the order given.
 * Otherwise the diatonic label stands: a chromatic chord that does not resolve is not called applied. Only a
 * segment with a diatonic reading is re-checked (it has a chord and a key); a chord outside the vocabulary
 * is not recovered, and modal mixture, the Neapolitan and augmented sixths are not looked for.
 *
 * The applied chord's inversion is its bass above its own root, as for any chord. Its label is the numeral
 * as it reads in the tonicized key, a slash and the target's numeral: V7/V, V6/5/V, vii-dim-6/V.
 */

typedef struct {
    double chromatic_min;          /* 0.15 */
    double missing_third_penalty;  /* 0.15 */
    double bass_chord_tone_bonus;  /* 0.05 */
} StandardTonalityAppliedConfig;

StandardTonalityAppliedConfig standard_tonality_applied_config_default(void);

/* True for a config whose three numbers are finite and non-negative. */
bool standard_tonality_applied_config_valid(const StandardTonalityAppliedConfig *config);

typedef struct {
    StandardTonalityRomanReading reading; /* the applied reading: applied, kind and target set */
    size_t resolution_index;              /* the segment it resolves to, an index into the inputs */
    bool deceptive;                       /* it resolves to the target's submediant, not the target */
} StandardTonalityAppliedResult;

/*
 * Re-checks inputs[index] given its diatonic reading. True (out filled) when it is relabelled as an applied
 * chord; false when the diatonic label stands, or for a NULL argument, a bad config, an index out of range,
 * an input with no numeral or no weights.
 */
bool standard_tonality_recheck_applied_chord(const StandardTonalityChordInput *inputs, size_t count, size_t index,
                                             const StandardTonalityAppliedConfig *config,
                                             StandardTonalityAppliedResult *out);

/*
 * As standard_tonality_build_roman_numerals with the thresholds of `config` (NULL for the defaults). A config
 * that is not valid is STANDARD_TONALITY_ROMAN_ERR_NULL_ARGUMENT.
 */
StandardTonalityRomanStatus standard_tonality_build_roman_numerals_with_config(
    const StandardTonalityChordInput *inputs, size_t count, const StandardTonalityAppliedConfig *config,
    StandardTonalityRomanResult **out);

#endif
