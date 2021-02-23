#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Triad structure rules for standard-tonality-v1. A chord is any
 * combination of three or more pitch classes sounding together; a triad
 * is a three-note chord whose notes can be arranged in thirds:
 *
 *   STACKED_IN_THIRDS   stacked in its most compact form (a snowperson),
 *                       a triad has a lowest note, a middle note a
 *                       generic third above it, and a highest note a
 *                       generic fifth above it
 *   LINES_OR_SPACES     a stacked triad's notes are always all on lines
 *                       or all on spaces
 *   ROOT_THIRD_FIFTH    in that form the lowest note is the root, the
 *                       middle the third, the highest the fifth (the
 *                       form is root position, taken up in Inversion)
 *   QUALITIES           four qualities, set by the intervals from the root
 *                       to the third and to the fifth: major (major
 *                       third, perfect fifth), minor (minor third,
 *                       perfect fifth), diminished (minor third,
 *                       diminished fifth), augmented (major third,
 *                       augmented fifth)
 *   NAMING              major and minor triads are named for the quality
 *                       of their third (both fifths are perfect);
 *                       diminished and augmented for the quality of
 *                       their fifth (the diminished third is minor, the
 *                       augmented third major)
 *
 * Measured with the interval rules (interval_quality_rules.h). Spelling
 * and identification are triad_spelling_rules.h and
 * triad_identification_rules.h.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_MAJOR = 0,
    STANDARD_TONALITY_TRIAD_MINOR,
    STANDARD_TONALITY_TRIAD_DIMINISHED,
    STANDARD_TONALITY_TRIAD_AUGMENTED,
    STANDARD_TONALITY_TRIAD_QUALITY_COUNT
} StandardTonalityTriadQuality;

typedef enum {
    STANDARD_TONALITY_TRIAD_ROOT = 0,
    STANDARD_TONALITY_TRIAD_THIRD,
    STANDARD_TONALITY_TRIAD_FIFTH,
    STANDARD_TONALITY_TRIAD_MEMBER_COUNT
} StandardTonalityTriadMember;

typedef enum {
    STANDARD_TONALITY_TRIAD_RULE_STACKED_IN_THIRDS = 0,
    STANDARD_TONALITY_TRIAD_RULE_LINES_OR_SPACES,
    STANDARD_TONALITY_TRIAD_RULE_ROOT_THIRD_FIFTH,
    STANDARD_TONALITY_TRIAD_RULE_QUALITIES,
    STANDARD_TONALITY_TRIAD_RULE_NAMING,
    STANDARD_TONALITY_TRIAD_RULE_COUNT
} StandardTonalityTriadRule;

const Rule *standard_tonality_triad_rule(StandardTonalityTriadRule which);
const Constraint *standard_tonality_triad_constraint(StandardTonalityTriadRule which);

/* "major" "minor" "diminished" "augmented"; NULL for an invalid quality. */
const char *standard_tonality_triad_quality_name(StandardTonalityTriadQuality quality);

/* "root" "third" "fifth"; NULL for an invalid member. */
const char *standard_tonality_triad_member_name(StandardTonalityTriadMember member);

/* ROOT_THIRD_FIFTH: the generic size from the root to the member: 1, 3, 5.
   0 for an invalid member. */
int standard_tonality_triad_member_size(StandardTonalityTriadMember member);

/* QUALITIES: the interval qualities of the third and the fifth above the
   root of a triad of the quality. False (outputs untouched) for an
   invalid quality or a NULL output. */
bool standard_tonality_triad_intervals(StandardTonalityTriadQuality quality,
                                       StandardTonalityIntervalQuality *third,
                                       StandardTonalityIntervalQuality *fifth);

/* QUALITIES: the triad quality with the given third and fifth, or false
   (output untouched) when the pair is not one of the four (a major third
   with a diminished fifth, for example) or *out is NULL. */
bool standard_tonality_triad_quality_from_intervals(StandardTonalityIntervalQuality third,
                                                    StandardTonalityIntervalQuality fifth,
                                                    StandardTonalityTriadQuality *out);

/* NAMING: the interval the triad is named for: its third (major, minor)
   or its fifth (diminished, augmented). On true *size is 3 or 5 and
   *quality that interval's quality. False for an invalid quality or NULL
   outputs. */
bool standard_tonality_triad_naming_interval(StandardTonalityTriadQuality quality, int *size,
                                             StandardTonalityIntervalQuality *interval_quality);

/* NAMING: true for major and minor / for diminished and augmented. */
bool standard_tonality_triad_named_for_third(StandardTonalityTriadQuality quality);
bool standard_tonality_triad_named_for_fifth(StandardTonalityTriadQuality quality);

/* STACKED_IN_THIRDS: true when `third` is a generic third and `fifth` a
   generic fifth above `root` (accidentals and qualities aside). False for
   NULL or a note below the one under it. */
bool standard_tonality_is_triad_stack(const PitchSpelling *root, const PitchSpelling *third,
                                      const PitchSpelling *fifth);

/* LINES_OR_SPACES: true when the three notes are all on lines or all on
   spaces (their staff places have the same parity), in any clef. False
   for NULL. */
bool standard_tonality_triad_on_lines_or_spaces(const PitchSpelling *a, const PitchSpelling *b,
                                                const PitchSpelling *c);

/* QUALITIES: the quality of a stacked triad. False (output untouched) for
   NULL, a stack that is not a triad stack, or an interval pair that is
   not one of the four qualities. */
bool standard_tonality_triad_stack_quality(const PitchSpelling *root, const PitchSpelling *third,
                                           const PitchSpelling *fifth, StandardTonalityTriadQuality *quality);

#endif
