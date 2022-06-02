#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_PATTERNS_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_PATTERNS_H

#include <stdbool.h>
#include <stddef.h>

#include "semantics/scale_direction.h"
#include "semantics/sequence_kind.h"

/*
 * The sequential harmonizations of a bass scale encoded by
 * scale-harmonization-v1, as the Harmonizing the Scale with Sequences chapter of
 * Open Music Theory (Gotham) describes them. A pattern is a repeating unit of
 * stages over a bass that runs by step in a direction. Each stage says whether
 * the bass moves to the next scale note there, which intervals (generic sizes
 * above the bass, octaves removed) the upper voice may make, and whether the
 * upper voice is suspended: held from the stage before while the bass moves.
 *
 *   PARALLEL_CHORDS          a first inversion chord on each bass note, in both
 *                            directions: the upper voice is a third, a sixth, or
 *                            an octave above the bass at every stage, the same
 *                            at each
 *   FIVE_SIX_ASCENDING       over each bass note, a fifth and then a sixth
 *   FIVE_SIX_DESCENDING      the same alternation note by note, descending
 *   FIVE_SIX_DESCENDING_ALTERNATING
 *                            descending, the fifth on one bass note and the sixth
 *                            on the next
 *   SEVEN_SIX_DESCENDING     a chain of suspensions: at each new bass note the upper
 *                            voice is held (a seventh), then resolves down a step
 *                            (a sixth); the chain descends with the scale, so the
 *                            pattern is the parallel chords with the top line
 *                            delayed
 *
 * The ascending 7-6 and 2-3 patterns (which need a leap up an octave to
 * restart), the cycles of fifths, and the ascending 4-3 chain are not encoded.
 */

#define SCALE_MAX_UNIT 4

typedef enum {
    SCALE_PATTERN_PARALLEL_CHORDS = 0,
    SCALE_PATTERN_FIVE_SIX_ASCENDING,
    SCALE_PATTERN_FIVE_SIX_DESCENDING,
    SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING,
    SCALE_PATTERN_SEVEN_SIX_DESCENDING,
    SCALE_PATTERN_COUNT
} ScalePatternId;

/* One stage of a unit: whether the bass moves by a step at this stage, a bit
   mask of the intervals above the bass the upper voice may make (bit (1u << n)
   for a generic size n from 1 to 8), and whether the upper voice is suspended. */
typedef struct {
    bool bass_moves;
    unsigned intervals;
    bool suspended;
} ScalePatternStage;

typedef struct {
    ScalePatternId id;
    const char *name;
    SequenceKind kind;
    ScaleDirection direction;
    bool both_directions;
    size_t unit_length;
    ScalePatternStage unit[SCALE_MAX_UNIT];
} ScalePattern;

/* True for the five patterns above; false for any other value. */
bool scale_pattern_valid(ScalePatternId id);

/* "parallel-chords" "five-six-ascending" "five-six-descending"
   "five-six-descending-alternating" "seven-six-descending"; NULL for an invalid
   pattern. */
const char *scale_pattern_name(ScalePatternId id);

/* The pattern, owned by this module; NULL for an invalid pattern. */
const ScalePattern *scale_pattern(ScalePatternId id);

/* True when the pattern can be harmonized in `direction`: its own, or either for
   the parallel chords. */
bool scale_pattern_allows_direction(const ScalePattern *pattern, ScaleDirection direction);

#endif
