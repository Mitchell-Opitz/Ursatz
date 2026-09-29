#ifndef MUSIC_ANALYSIS_MOTIVE_VARIANT_RECOGNITION_H
#define MUSIC_ANALYSIS_MOTIVE_VARIANT_RECOGNITION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_relation.h"
#include "analysis/motive_sketch.h"

/*
 * Variant recognizer (L8 analysis): names the chain of motive relations
 * (analysis/motive_relation.h) that turns one motive sketch into another, with its distance and
 * which dimensions the variant kept and changed. The inputs are plain sketches, notes the analysis
 * chain already produces (R-006). The search allocates its beam on the heap for the length of one
 * call.
 */

typedef enum {
    MOTIVE_DIMENSION_RHYTHM,
    MOTIVE_DIMENSION_INTERVALS,
    MOTIVE_DIMENSION_CONTOUR,
    MOTIVE_DIMENSION_HARMONY,
    MOTIVE_DIMENSION_COUNT
} MotiveDimension;

/*
 * Two sketches compared per dimension (R-134). similarity is 0 to 1, kept is true when it is 1,
 * changed when not. rhythm     the share of neighbouring note pairs whose inter-onset ratio and
 * duration ratio match (over the longer sketch); kept when onsets and durations scale by one common
 * factor intervals  the share of steps that match: scale steps by each sketch's own scale,
 * semitones without one contour    the share of directions that match harmony    the share of
 * chords, by position, with the same degree, alteration, inversion and seventh (onsets are not
 * compared) A dimension neither sketch has (pitches in a rhythm sketch, chords) is 1; one sketch
 * having it and not the other is 0.
 */
typedef struct {
    double similarity[MOTIVE_DIMENSION_COUNT];
    bool kept[MOTIVE_DIMENSION_COUNT];
    bool changed[MOTIVE_DIMENSION_COUNT];
} MotiveDimensions;

void motive_variant_compare(const MotiveSketch *a, const MotiveSketch *b, MotiveDimensions *out);

/* The mean of the four similarities: how near one sketch is to another. */
double motive_variant_nearness(const MotiveDimensions *dimensions);

#define MOTIVE_RECOGNIZE_MAX_DEPTH 6
#define MOTIVE_RECOGNIZE_MAX_BEAM 256

typedef struct {
    size_t max_depth; /* 1 to MOTIVE_RECOGNIZE_MAX_DEPTH, default 4 */
    size_t beam;      /* 1 to MOTIVE_RECOGNIZE_MAX_BEAM, default 64 */
    bool contour;     /* compare contour forms (R-148), default false */
    /*
     * Optional filter (R-152), NULL by default. When set, every proposal at every depth is passed to admit before it is
     * applied and a step it returns false for is skipped, so found means a chain of admitted steps. A caller that replays
     * chains through its own operation table passes the steps that table can take.
     */
    bool (*admit)(const MotiveRelationStep *step, const void *context);
    const void *admit_context;
} MotiveRecognizeOptions;

void motive_recognize_options_default(MotiveRecognizeOptions *options);

/*
 * The chain from the source to the target. found is true when a chain of at most max_depth steps
 * turns the source into the target (replayed with motive_relation_apply it gives the target);
 * distance is cost / (cost + 4) over the chain's summed relation costs (R-142), exact. kept,
 * changed and similarity compare the target with the source per dimension (R-134). When no chain is
 * found, found is false, distance is 1 and steps hold the chain to the closest sketch the search
 * reached (step_count 0 when the source itself was the closest).
 */
typedef struct {
    bool found;
    MotiveRelationStep steps[MOTIVE_RECOGNIZE_MAX_DEPTH];
    size_t step_count;
    ExactRational distance;
    bool kept[MOTIVE_DIMENSION_COUNT];
    bool changed[MOTIVE_DIMENSION_COUNT];
    double similarity[MOTIVE_DIMENSION_COUNT];
} MotiveRecognition;

typedef enum {
    MOTIVE_RECOGNIZE_OK = 0,   /* the search ran; see out->found */
    MOTIVE_RECOGNIZE_INVALID,  /* a sketch or an option is not well formed, or out is NULL */
    MOTIVE_RECOGNIZE_NO_MEMORY /* the beam could not be allocated */
} MotiveRecognizeResult;

/*
 * Searches the relation chains from source to target, breadth first by depth up to max_depth. Equal
 * sketches give one exact step and distance 0. At each state every relation's proposer gives at
 * most 16 parameter sets, inferred from the state and the target where it can be, else a range of
 * 16 or fewer. A state equal to the target ends the search at that depth: of the chains of that
 * depth the lowest cost wins, then the first in relation order. The best `beam` states of each
 * depth are kept by nearness to the target (mean of the four similarities, ties by relation order),
 * except at depth 1: the states one relation from the source are all kept, so every chain of two
 * relations whose parameters the proposers give is found at any beam (the cost of a depth-2 miss is
 * measured in docs/modules/motive-variant-recognition.md); identical states are merged. A rhythm sketch (unknown pitches) takes only the relations that
 * touch time, a sketch without a scale takes no tonal parameters. Deterministic.
 *
 * With options->contour the source and the target are normalized to their contour first
 * (motive_sketch_contour_normalize) and every state is normalized right after its relation applies
 * (a state whose normalization fails is dropped), so found means replaying the steps with
 * motive_relation_apply then motive_sketch_contour_normalize after each step gives the target. A
 * sketch with unknown pitches is INVALID in contour mode.
 *
 * Both sketches are compared as given: a harmony relation adds the best implied chord at 0 to a
 * sketch without chords (motive_relation.h), so a caller that compares harmony gives both sketches
 * their chords.
 */
MotiveRecognizeResult motive_variant_recognize(const MotiveSketch *source,
                                               const MotiveSketch *target,
                                               const MotiveRecognizeOptions *options,
                                               MotiveRecognition *out);

#endif
