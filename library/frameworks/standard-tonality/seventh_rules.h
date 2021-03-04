#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "triad_rules.h"

/*
 * Seventh chord structure rules for standard-tonality-v1. A seventh
 * chord is a four-note chord whose notes can be arranged in thirds:
 *
 *   STACKED_IN_THIRDS      stacked in its most compact form (an extra-long
 *                          snowperson), a seventh chord has a lowest
 *                          note, a note a generic third above it, one a
 *                          generic fifth above, and one a generic
 *                          seventh above
 *   LINES_OR_SPACES        a stacked seventh chord's notes are always all
 *                          on lines or all on spaces
 *   ROOT_THIRD_FIFTH_SEVENTH
 *                          in that form the lowest note is the root, the
 *                          next the third, then the fifth, and the
 *                          highest the seventh (the "chordal" seventh,
 *                          as distinct from the seventh scale degree)
 *   QUALITIES              five common qualities, set by two things: the
 *                          quality of the triad of root, third, and
 *                          fifth, and the quality of the seventh from
 *                          the root: major-major (major triad, major
 *                          seventh), major-minor (major triad, minor
 *                          seventh), minor-minor (minor triad, minor
 *                          seventh), half-diminished (diminished triad,
 *                          minor seventh), fully diminished (diminished
 *                          triad, diminished seventh)
 *   NAMING                 for the first three the first word names the
 *                          triad and the second the seventh; the two
 *                          diminished-triad qualities differ only in
 *                          the seventh
 *   ALTERNATE_NAMES        the major-major seventh chord is also called
 *                          the major seventh chord, the major-minor the
 *                          dominant seventh chord, the minor-minor the
 *                          minor seventh chord, and the fully diminished
 *                          the diminished seventh chord; the half-
 *                          diminished has no alternate name
 *
 * Measured with the interval rules (interval_quality_rules.h) and the
 * triad rules (triad_rules.h). Spelling and identification are
 * seventh_spelling_rules.h and seventh_identification_rules.h; chord
 * symbols are chord_symbol_rules.h.
 *
 * Chromatically altered seventh chords (an augmented triad with a
 * seventh, a minor triad with a major seventh) are outside the five and
 * are a later chapter's: the quality functions report no answer for
 * them.
 */

typedef enum {
    STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR = 0,
    STANDARD_TONALITY_SEVENTH_MAJOR_MINOR,
    STANDARD_TONALITY_SEVENTH_MINOR_MINOR,
    STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED,
    STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED,
    STANDARD_TONALITY_SEVENTH_QUALITY_COUNT
} StandardTonalitySeventhQuality;

typedef enum {
    STANDARD_TONALITY_SEVENTH_ROOT = 0,
    STANDARD_TONALITY_SEVENTH_THIRD,
    STANDARD_TONALITY_SEVENTH_FIFTH,
    STANDARD_TONALITY_SEVENTH_SEVENTH,
    STANDARD_TONALITY_SEVENTH_MEMBER_COUNT
} StandardTonalitySeventhMember;

typedef enum {
    STANDARD_TONALITY_SEVENTH_RULE_STACKED_IN_THIRDS = 0,
    STANDARD_TONALITY_SEVENTH_RULE_LINES_OR_SPACES,
    STANDARD_TONALITY_SEVENTH_RULE_ROOT_THIRD_FIFTH_SEVENTH,
    STANDARD_TONALITY_SEVENTH_RULE_QUALITIES,
    STANDARD_TONALITY_SEVENTH_RULE_NAMING,
    STANDARD_TONALITY_SEVENTH_RULE_ALTERNATE_NAMES,
    STANDARD_TONALITY_SEVENTH_RULE_COUNT
} StandardTonalitySeventhRule;

const Rule *standard_tonality_seventh_rule(StandardTonalitySeventhRule which);
const Constraint *standard_tonality_seventh_constraint(StandardTonalitySeventhRule which);

/* NAMING: "major-major seventh" "major-minor seventh" "minor-minor
   seventh" "half-diminished seventh" "fully diminished seventh"; NULL for
   an invalid quality. */
const char *standard_tonality_seventh_quality_name(StandardTonalitySeventhQuality quality);

/* ALTERNATE_NAMES: "major seventh" "dominant seventh" "minor seventh"
   (NULL: the half-diminished has none) "diminished seventh". NULL for an
   invalid quality as well. */
const char *standard_tonality_seventh_quality_alternate_name(StandardTonalitySeventhQuality quality);

/* "root" "third" "fifth" "seventh"; NULL for an invalid member. */
const char *standard_tonality_seventh_member_name(StandardTonalitySeventhMember member);

/* ROOT_THIRD_FIFTH_SEVENTH: the generic size from the root to the member:
   1, 3, 5, 7. 0 for an invalid member. */
int standard_tonality_seventh_member_size(StandardTonalitySeventhMember member);

/* QUALITIES: the triad quality and the seventh's interval quality of a
   seventh chord of the quality. False (outputs untouched) for an invalid
   quality or a NULL output. */
bool standard_tonality_seventh_parts(StandardTonalitySeventhQuality quality, StandardTonalityTriadQuality *triad,
                                     StandardTonalityIntervalQuality *seventh);

/* QUALITIES: the seventh chord quality with the given triad quality and
   seventh, or false (output untouched) when the pair is not one of the
   five (a minor triad with a major seventh, for example) or *out is
   NULL. */
bool standard_tonality_seventh_quality_from_parts(StandardTonalityTriadQuality triad,
                                                  StandardTonalityIntervalQuality seventh,
                                                  StandardTonalitySeventhQuality *out);

/* STACKED_IN_THIRDS: true when `third`, `fifth`, and `seventh` are a
   generic third, fifth, and seventh above `root` (accidentals and
   qualities aside). False for NULL or a note below the one under it. */
bool standard_tonality_is_seventh_stack(const PitchSpelling *root, const PitchSpelling *third,
                                        const PitchSpelling *fifth, const PitchSpelling *seventh);

/* LINES_OR_SPACES: true when the four notes are all on lines or all on
   spaces, in any clef. False for NULL. */
bool standard_tonality_seventh_on_lines_or_spaces(const PitchSpelling *a, const PitchSpelling *b,
                                                  const PitchSpelling *c, const PitchSpelling *d);

/* QUALITIES: the quality of a stacked seventh chord. False (output
   untouched) for NULL, a stack that is not a seventh chord stack, or a
   triad and seventh that are not one of the five qualities. */
bool standard_tonality_seventh_stack_quality(const PitchSpelling *root, const PitchSpelling *third,
                                             const PitchSpelling *fifth, const PitchSpelling *seventh,
                                             StandardTonalitySeventhQuality *quality);

#endif
