#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Intervallic enharmonic equivalence rules for standard-tonality-v1.
 * Intervals of different size and quality can sound the same:
 *
 *   SEMITONES   intervals with the same number of half steps are
 *               enharmonically equivalent: a major second and a
 *               diminished third are both 2, an augmented fourth and a
 *               diminished fifth both 6, a perfect unison and a
 *               diminished second both 0
 *   NOTES       to identify an interval whose bottom note has no
 *               ordinary key signature, replace its notes with
 *               enharmonic equivalents -- E-double-flat to A-flat is
 *               D to G-sharp, an augmented fourth, read from the key of
 *               D
 *
 * Half steps are 12-TET (tonal_pitch.h). The equivalence is a rule here,
 * not a property of PitchSpelling, which keeps enharmonic spellings
 * distinct.
 */

typedef enum {
    STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_SEMITONES = 0,
    STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_NOTES,
    STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT
} StandardTonalityIntervalEnharmonicRule;

const Rule *standard_tonality_interval_enharmonic_rule(StandardTonalityIntervalEnharmonicRule which);
const Constraint *standard_tonality_interval_enharmonic_constraint(StandardTonalityIntervalEnharmonicRule which);

/* SEMITONES: the quality of the interval of `size` with the given number
   of half steps (compound sizes include their octaves). False (output
   untouched) for a size below 1, a NULL output, or a count that needs
   more than a triply augmented or diminished quality. */
bool standard_tonality_interval_at_semitones(int size, int semitones, StandardTonalityIntervalQuality *quality);

/* SEMITONES: true when the two intervals have the same number of half
   steps and are not the same size and quality. False if either quality
   does not fit its size. */
bool standard_tonality_intervals_enharmonic(int size_a, StandardTonalityIntervalQuality quality_a, int size_b,
                                            StandardTonalityIntervalQuality quality_b);

/* NOTES: true when two spellings sound the same (12-TET) and are
   spelled differently. False for NULL. */
bool standard_tonality_spellings_enharmonic(const PitchSpelling *a, const PitchSpelling *b);

/*
 * NOTES: the size and quality of the interval from `lower` up to
 * `upper`, found by respelling both notes enharmonically: `lower`
 * becomes the plainest spelling of its pitch (natural first, then
 * sharp, then flat), and `upper` moves the same number of letters, keeping
 * its sound, so the size does not change. The major-scale method then
 * runs from the new bottom note. False (outputs untouched) for NULL, an
 * upper note below the lower, or a respelling needing more than a
 * double accidental.
 */
bool standard_tonality_interval_quality_via_enharmonic(const PitchSpelling *lower, const PitchSpelling *upper,
                                                       int *size, StandardTonalityIntervalQuality *quality);

#endif
