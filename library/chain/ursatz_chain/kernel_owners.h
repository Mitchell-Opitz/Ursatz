#ifndef URSATZ_CHAIN_KERNEL_OWNERS_H
#define URSATZ_CHAIN_KERNEL_OWNERS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "ursatz_chain/chain_inputs.h"
#include "semantics/tone_line.h"
#include "weighted_chord_identification.h"

/*
 * The callbacks that put the kernel owners (R-243) behind the frameworks: the chain, which may include L8, hands them to the weighted
 * chord identifier, the melody stage and the motive stage (AnalysisChainConfig.inject_kernel_owners).
 *
 *   non-chord tones   motive_tone_roles (analysis/motive_tone_roles.h) as a NonChordToneClassifier
 *   motive relations  motive_variant_recognize (analysis/motive_variant_recognition.h) as a StandardFormRelationMatcher
 */

/* A NonChordToneClassifier: motive_tone_roles_line. context is unused. */
bool chain_classify_tones(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context);

/* The key a motive's generic (degree) shapes are read in; the context of chain_match_motive_relation. */
typedef struct {
    bool has_key;
    int tonic_pitch_class; /* 0..11 */
    bool minor;
} ChainRelationContext;

/*
 * A StandardFormRelationMatcher. Both shapes become MotiveSketches (note k at the sum of the first k onset gaps, lasting its gap, the last
 * note the last gap; a scale from the context when the shape is generic and the context has a key); motive_variant_recognize runs at
 * depth 1 with only exact, transpose (real and tonal), invert and retrograde admitted. Equal sketches are EXACT; a transposition is REAL, or
 * TONAL when its tonal parameter is set. score is the mean similarity of the four dimensions of the recognition (1 for exact and
 * for a transposition). False when no such single step turns the prototype into the candidate, or a shape makes no valid sketch.
 * context is a ChainRelationContext, or NULL for none.
 */
bool chain_match_motive_relation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                 StandardFormMotiveKind *kind, double *score, void *context);

/*
 * Relabels the non-chord tones of a melody line with a classifier: each note's chord mask is the chord of its first segment (results, one
 * per segment; none for a segment without a match), and standard_counterpoint_melody_nct_labels_with decides the labels, which replace
 * the nct and nct_type the line holds. False on an allocation failure; the line is then unchanged.
 */
bool chain_relabel_melody_line(ChainMelodyLine *melody, const CommonPracticeWeightedResult *results, size_t segment_count,
                               NonChordToneClassifier classifier, void *context);

#endif
