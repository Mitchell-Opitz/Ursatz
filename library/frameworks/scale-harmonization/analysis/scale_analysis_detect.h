#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_ANALYSIS_SCALE_ANALYSIS_DETECT_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_ANALYSIS_SCALE_ANALYSIS_DETECT_H

#include "galant_analysis_input.h"
#include "interpretation/interpretation.h"
#include "relationship/relationship.h"
#include "scale_patterns.h"
#include "sequence_kind.h"

/*
 * Sequence detection (the scale-harmonization adapter): harmonic sequences found in the outer voices and Roman numerals of a run of segments,
 * and their kind assigned by scale-harmonization's patterns and rules (none added). The input is galant-schemas' neutral segment struct
 * (galant_analysis_input.h): scale-harmonization declares galant-schemas as a dependency, and the two adapters read the same struct. The adapter
 * includes no other framework and no kernel analysis.
 *
 * A SEQUENCE is a MODEL of one or more chord segments repeated, transposed by a constant diatonic interval, at least config.minimum_units times
 * (3: the model and two copies). The CHORD SEGMENTS are those with a bass degree; segments are consecutive in the input (one that has no bass
 * ends a run). Degrees are read in the home key (the segment's key tonic added), so a sequence may cross keys.
 *
 * KIND. The sequence is checked against each pattern of scale_patterns.h with scale-harmonization's own rules (scale_sequence_check: direction,
 * bass by step, intervals above the bass, repeats, suspended), the soprano as the upper voice; a pattern whose hard rules hold over whole units
 * gives the kind (parallel chords, 5-6, 7-6) and the claim is RULE-BACKED. The stages are the segments, a unit the pattern's unit_length of them,
 * starting on the pattern's first stage. For the spelled pitches the rules need, the degrees are placed in a C-major frame: the bass moves to the
 * nearest octave of its next degree, the soprano sits above it (a unison an octave above).
 *   cycle of fifths  scale-harmonization encodes no pattern for the cycle of fifths, so it has no rule to check: a model of TWO chords whose
 *                    roots (from the Roman numerals, applied chords included) each descend a fifth, repeated with the same bass interval in
 *                    each unit, is reported as SEQUENCE_KIND_CYCLE_OF_FIFTHS with rule_backed = false, to be replaced by the framework's rules
 *                    when it encodes them. Ascending fifths and other root motions have no kind and are not reported
 *
 * COMPARING UNITS. Unit k is compared to unit 0 by its root-motion pattern (the diatonic steps between its chords' roots, when every chord has a
 * numeral) and its bass-interval pattern; the unit share is the share of units that match. The soprano pattern (its steps inside a unit) matching
 * across the units raises the confidence by 0.1.
 *
 * CONFIDENCE = unit share x (1 - mean uncertainty of the segments) + soprano bonus, at most 1.
 *
 * OVERLAPS. Sequences that share a segment are resolved: the one with more units, then higher confidence, then rule-backed, then the earlier, is kept.
 * A run longer than SCALE_ANALYSIS_MAX_STAGES stages is cut there.
 *
 * OUTPUT, per sequence: a Relationship over the segments (from the first to the last, predicate "harmonic-sequence") and an Interpretation
 * (FrameworkReference scale-harmonization, scalar confidence = 1 - uncertainty) targeting its id, with the claim "scale-harmonization/sequence":
 * the SequenceKind, the pattern (or -1), whether it is rule-backed, the direction, the unit length and count, the transposition in diatonic
 * steps between units, and the stage list with the segment refs and the degrees (bass, soprano and root, in the home key).
 */

#define SCALE_SEQUENCE_CLAIM_TYPE "scale-harmonization/sequence"
#define SCALE_ANALYSIS_MAX_STAGES 32

typedef struct {
    unsigned minimum_units;
} ScaleDetectConfig;

/* minimum_units 3. */
ScaleDetectConfig scale_detect_config_default(void);

typedef struct {
    char segment_id[GALANT_ANALYSIS_ID_MAX];
    int bass_degree;
    int bass_alteration;
    int has_soprano;
    int soprano_degree;
    int soprano_alteration;
    int has_root;
    int root_degree;
    int root_alteration;
} ScaleStageClaim;

typedef struct {
    int kind;       /* SequenceKind */
    char kind_name[24];
    int pattern;    /* ScalePatternId, -1 when no pattern encodes it */
    int rule_backed;
    int direction;  /* ScaleDirection; -1 for the cycle of fifths */
    int unit_length;
    int units;
    int stage_count;
    int transposition_steps; /* the bass, in diatonic steps, from the start of one unit to the next (up positive) */
    ScaleStageClaim stages[SCALE_ANALYSIS_MAX_STAGES];
    int first_segment; /* the index in the input */
    int last_segment;
    double unit_share;
    double soprano_bonus;
    double mean_uncertainty;
    double confidence;
} ScaleSequenceClaim;

typedef struct {
    ScaleSequenceClaim claim;
    double uncertainty;
    Relationship *relationship;     /* owned */
    Interpretation *interpretation; /* owned */
} ScaleSequenceMatch;

typedef struct {
    ScaleSequenceMatch *items; /* in order of the first segment; NULL when none */
    size_t count;
} ScaleSequenceMatches;

/* Detects the sequences. `config` may be NULL for the defaults; minimum_units must be at least 2. *out is untouched on failure. */
GalantAnalysisStatus scale_sequence_detect(const GalantSegmentList *segments, const ScaleDetectConfig *config, ScaleSequenceMatches *out);

void scale_sequence_matches_destroy(ScaleSequenceMatches *matches);

/* Reads the claim back out of a sequence Interpretation. False if it is not one. */
bool scale_sequence_claim_read(const Interpretation *interpretation, ScaleSequenceClaim *out);

#endif
