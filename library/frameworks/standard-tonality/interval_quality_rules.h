#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_QUALITY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_QUALITY_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Interval quality rules for standard-tonality-v1. A quality makes an
 * interval specific when combined with a size
 * (interval_size_rules.h) and, with it, describes how the interval
 * sounds. The five qualities are augmented (A, +), major (ma), perfect
 * (P), minor (mi), and diminished (d, o); an interval can also be
 * doubly or triply augmented or diminished:
 *
 *   CLASSES        unisons, fourths, fifths, and octaves are perfect
 *                  intervals; seconds, thirds, sixths, and sevenths
 *                  are major or minor. As applied today: other
 *                  theorists, places, and periods have applied the
 *                  qualities to different sizes
 *   MAJOR_SCALE_METHOD
 *                  to identify an interval, take the bottom note as the
 *                  tonic of a major scale: a top note in that scale
 *                  makes the interval perfect (unison, fourth, fifth,
 *                  octave) or major (second, third, sixth, seventh); a
 *                  top note not in it makes it minor, augmented, or
 *                  diminished
 *   AUGMENTED      an augmented interval is a half step larger than a
 *                  perfect or major interval
 *   DIMINISHED     a diminished interval is a half step smaller than a
 *                  perfect or minor interval; a major interval made a
 *                  half step smaller is minor
 *   FURTHER_ALTERED
 *                  a half step beyond augmented is doubly augmented, one
 *                  more is triply augmented; a half step beyond
 *                  diminished is doubly diminished, then triply
 *   COMPOUND       a compound interval has the quality of its simple
 *                  counterpart (A to C is a minor third, A to the C an
 *                  octave higher a minor tenth)
 *
 * Measured from the lower note up. Half steps (12-TET semitones, tonal_
 * pitch.h) appear only as the engine under the major-scale method: the
 * major-scale method itself is available as
 * standard_tonality_interval_quality_by_major_scale and the two agree
 * wherever the major scale of the bottom note exists.
 */

typedef enum {
    STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED = 0,
    STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED,
    STANDARD_TONALITY_QUALITY_DIMINISHED,
    STANDARD_TONALITY_QUALITY_MINOR,
    STANDARD_TONALITY_QUALITY_PERFECT,
    STANDARD_TONALITY_QUALITY_MAJOR,
    STANDARD_TONALITY_QUALITY_AUGMENTED,
    STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED,
    STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED,
    STANDARD_TONALITY_QUALITY_COUNT
} StandardTonalityIntervalQuality;

typedef enum {
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_CLASSES = 0,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_MAJOR_SCALE_METHOD,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_AUGMENTED,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_DIMINISHED,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_FURTHER_ALTERED,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COMPOUND,
    STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT
} StandardTonalityIntervalQualityRule;

const Rule *standard_tonality_interval_quality_rule(StandardTonalityIntervalQualityRule which);
const Constraint *standard_tonality_interval_quality_constraint(StandardTonalityIntervalQualityRule which);

/* "diminished", "minor", "perfect", "major", "augmented", and "doubly" /
   "triply" forms ("doubly augmented"); NULL for an invalid quality. */
const char *standard_tonality_interval_quality_name(StandardTonalityIntervalQuality quality);

/* "d" "mi" "P" "ma" "A" for the five qualities. Doubly and triply
   altered intervals double and triple the letter ("dd", "AAA"), a
   convention rather than the source's. NULL for an invalid quality. */
const char *standard_tonality_interval_quality_abbreviation(StandardTonalityIntervalQuality quality);

/* CLASSES: true for the augmented family / the diminished family
   (including doubly and triply). */
bool standard_tonality_interval_quality_is_augmented(StandardTonalityIntervalQuality quality);
bool standard_tonality_interval_quality_is_diminished(StandardTonalityIntervalQuality quality);

/* CLASSES: true when the size (simple or compound) is a unison, fourth,
   fifth, or octave: it takes perfect, not major or minor. False for the
   others and for a size below 1. */
bool standard_tonality_interval_size_is_perfect_class(int size);

/* CLASSES: true when the quality is possible for the size: perfect only
   for the perfect class, major and minor only for the others, and the
   augmented and diminished families for both. */
bool standard_tonality_interval_quality_fits_size(int size, StandardTonalityIntervalQuality quality);

/* The half steps of the perfect or major interval of a simple size 1..8:
   0 2 4 5 7 9 11 12. 0 outside 1..8 (also the unison's own value, so
   check the size). */
int standard_tonality_major_interval_semitones(int simple_size);

/* MAJOR_SCALE_METHOD / AUGMENTED / DIMINISHED: the size and quality of
   the interval from `lower` up to `upper`. False (outputs untouched)
   for NULL, a NULL output, an upper note below the lower, or an
   interval beyond triply augmented or diminished. */
bool standard_tonality_interval_quality(const PitchSpelling *lower, const PitchSpelling *upper, int *size,
                                        StandardTonalityIntervalQuality *quality);

/*
 * MAJOR_SCALE_METHOD: the same, found as the method says: build the
 * major scale on `lower` and compare `upper` with its note a size
 * above (a note in the scale is perfect or major; a note a half step
 * lower is minor, or diminished for a perfect-class size; and so on).
 * False for the same reasons, and also when `lower` has no major scale
 * (an imaginary key whose scale needs more than a double accidental --
 * such intervals are identified by inversion or enharmonic equivalence
 * instead).
 */
bool standard_tonality_interval_quality_by_major_scale(const PitchSpelling *lower, const PitchSpelling *upper,
                                                       int *size, StandardTonalityIntervalQuality *quality);

/* MAJOR_SCALE_METHOD: true when `upper` is in the major scale of
   `lower` (so the interval is perfect or major, at the right size).
   False for NULL, an upper note below the lower, or no major scale. */
bool standard_tonality_interval_top_in_major_scale(const PitchSpelling *lower, const PitchSpelling *upper);

/*
 * AUGMENTED / DIMINISHED / FURTHER_ALTERED: moves a quality by `semitones`
 * half steps along its size's ladder (diminished, minor, major,
 * augmented for major/minor sizes; diminished, perfect, augmented for
 * perfect sizes; doubly and triply beyond): +1 on a perfect or major
 * interval is augmented, -1 on a major interval is minor, -1 on a perfect
 * or minor one is diminished. False (output untouched) for a quality that
 * does not fit the size, a NULL output, or a result beyond triply
 * augmented or diminished.
 */
bool standard_tonality_interval_quality_shift(int size, StandardTonalityIntervalQuality quality, int semitones,
                                              StandardTonalityIntervalQuality *out);

/* The half steps in an interval of the size and quality (compound sizes
   add 12 per octave). False (output untouched) for a quality that does
   not fit the size, a size below 1, or a NULL output. */
bool standard_tonality_interval_semitones(int size, StandardTonalityIntervalQuality quality, int *semitones);

#endif
