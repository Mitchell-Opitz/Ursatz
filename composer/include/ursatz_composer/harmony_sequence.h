#ifndef URSATZ_COMPOSER_HARMONY_SEQUENCE_H
#define URSATZ_COMPOSER_HARMONY_SEQUENCE_H

#include <scale_patterns.h>

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/harmony_bass.h>

/*
 * The sequence operator: a Fill operator that offers a harmonic sequence as the skeleton of the start of
 * a continuation, writing the harmony and the bass together. The sequences are the patterns Ursatz's
 * scale-harmonization framework encodes: parallel chords (both directions), 5-6 ascending, 5-6
 * descending (note by note and alternating) and 7-6 descending. The pattern says where the bass moves and
 * which intervals the upper voice makes above it; here a stage's chord is the one those intervals make
 * with the bass, a root position triad for a fifth and a first inversion triad for a sixth or a third,
 * on a diatonic scale degree (so in a minor key the natural minor). A suspended stage is the chord it
 * resolves to, held. Descending fifths and descending thirds are not encoded by the framework, so they
 * are not offered.
 *
 * A target is a Constrained harmony lane span that does not start its owner node, as for the schema
 * operator (harmony_schema.h), with the same bass lane, key and meter requirements. Sequences have no
 * metrical strengths, so they are offered in every meter the harmonic rhythm supports. A sequence is
 * repeated for a number of times between min_repeats and max_repeats, the most that fits leaving room
 * for the cadence, and is cut to a whole number of bars; a sequence that cannot fit has no candidate.
 * The start degree is drawn with the seeded stream, and a start whose chords include a diminished triad
 * in root position is not offered. What is left of the target stays open with its cadence. The label of a
 * candidate is "sequence:<pattern>" with "-ascending" or "-descending" for the parallel chords.
 */

#define HARMONY_SEQUENCE_OPERATOR_NAME "harmony-sequence"

/* A bit of HarmonySequenceParams.patterns: the pattern with this ScalePatternId. */
#define HARMONY_SEQUENCE_BIT(id) (1u << (unsigned)(id))
#define HARMONY_SEQUENCE_ALL ((1u << SCALE_PATTERN_COUNT) - 1u)

/*
 * patterns is the set of patterns offered; min_repeats and max_repeats how many times the pattern's unit
 * is played (at least 1); preference is added to the score of a sequence's candidate, in thousandths, as
 * for schemas; bass is the bass range and subdivision of the bass it writes.
 */
typedef struct {
    unsigned patterns;
    unsigned min_repeats;
    unsigned max_repeats;
    unsigned preference;
    HarmonyBassParams bass;
} HarmonySequenceParams;

/* The defaults: every pattern, 2 to 4 repeats, preference 250, and the bass operator's defaults. */
HarmonySequenceParams harmony_sequence_params_default(void);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params
 * must outlive it (NULL means the defaults).
 */
FillResult harmony_sequence_register(FillOperatorRegistry *registry, const HarmonySequenceParams *params);

#endif
