#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_IDENTIFICATION_H

#include <stdbool.h>
#include <stddef.h>

#include "chord_vocabulary.h"
#include "chord_vocabulary_seventh.h"
#include "key_vocabulary.h"
#include "semantics/beat_strength.h"

/*
 * Weighted chord identification for common-practice-minimal-v1: a new way to choose a chord for a
 * segment of sounding notes, alongside the subset-match functions (triad_identification.h,
 * seventh_identification.h), which are unchanged. Subset matching ignores extra degrees, takes the first
 * match, and fails on two or more non-chord tones in a beat; this weighs what sounds, scores each
 * candidate chord, and reports a best chord, a runner-up and how close they were.
 *
 * Input is a plain per-member struct, like the integers the subset-match functions take: this framework
 * includes no other framework (the caller computes each member's BeatStrength with standard-meter and
 * passes it in), and it reads no NoteEvent. The caller turns an adaptive segment (semantics/
 * sonority_segmentation.h: every sounding note, attacked or held) and its notes into members.
 *
 * WEIGHTS (this file, step 1). The weight of a member is its duration sounding in the segment times a
 * factor: the beat-strength factor of its attack, or the fixed held factor if it was held over from an
 * earlier segment. The bass member (the lowest sounding pitch; every member at that pitch) is then
 * multiplied by the bass weight. A pitch class's weight is the sum over the members on it, and the
 * weights and their total are what scoring reads. Defaults (CommonPracticeWeightedConfig):
 *
 *     strong 1.0, medium 0.7, weak 0.4   (BeatStrength has three values; they are evenly spaced from the
 *                                         downbeat's 1.0 to the weakest 0.4)
 *     held 0.6      bass weight 1.5
 *
 * All arithmetic is IEEE double, summed in member order, so the result is deterministic for a given
 * input in the same build. Callers pass the segment duration in whole notes.
 */

typedef struct {
    const char *note_id;    /* the note's EntityID value; borrowed, may be NULL when labels are not used */
    int pitch;              /* semitone number, C4 = 60 */
    bool held;              /* sounding before the segment began (false: attacked in it) */
    double duration;        /* whole notes sounding inside the segment; must be > 0 */
    BeatStrength strength;  /* beat strength of the attack; not read when held */
    const char *voice_id;   /* the note's Voice EntityID value, or NULL when it has none */
    double onset;           /* absolute onset in whole notes: orders the notes of one voice */
} CommonPracticeMember;

typedef struct {
    double strength_factor[BEAT_STRENGTH_COUNT]; /* indexed by BeatStrength */
    double held_factor;
    double bass_weight;
    double missing_third_penalty;  /* used by scoring */
    double bass_chord_tone_bonus;  /* used by scoring */
    double min_coverage;           /* used by scoring: a best score below it is "no match" */
} CommonPracticeWeightedConfig;

/* The documented defaults: strong 1.0, medium 0.7, weak 0.4, held 0.6, bass 1.5, missing-third penalty
   0.15, bass chord-tone bonus 0.05, min coverage 0.5. */
CommonPracticeWeightedConfig common_practice_weighted_config_default(void);

typedef enum {
    COMMON_PRACTICE_WEIGHTED_OK = 0,
    COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG,
    COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER,
    COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE,
    COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC,
    COMMON_PRACTICE_WEIGHTED_ERR_ALLOC
} CommonPracticeWeightedStatus;

/* The weight of each pitch class (0 = C ... 11 = B), their sum, and the bass. */
typedef struct {
    double pitch_class[12];
    double total;
    int bass_pitch;       /* the lowest sounding pitch, or -1 with no members */
    int bass_pitch_class; /* its pitch class, or -1 */
} CommonPracticeWeights;

/*
 * Weights a segment's members. count may be 0 (all zero, no bass). BAD_CONFIG for a negative or
 * non-finite factor; BAD_MEMBER for a duration that is not positive and finite or an invalid
 * BeatStrength on an attacked member. *out is untouched on any error.
 */
CommonPracticeWeightedStatus common_practice_weight_segment(const CommonPracticeMember *members, size_t count,
                                                             const CommonPracticeWeightedConfig *config,
                                                             CommonPracticeWeights *out);

/*
 * SCORING (step 2). The candidates are the 14 diatonic chords of a MAJOR key: the seven diatonic triads
 * (I ii iii IV V vi vii-dim, chord_vocabulary.h) and the seven diatonic sevenths, each root taking the
 * seventh quality its scale gives (I and IV major7, ii iii vi minor7, V dominant7, vii half-diminished7;
 * chord_vocabulary_seventh.h). Fully-diminished sevenths, chromatic and applied chords are not
 * candidates. The key comes from the caller (a whole-piece key until local key regions exist). A minor
 * key is COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE: this vocabulary is the major-key set, and
 * labelling a minor key is Roman numeral work for standard-tonality.
 *
 *     score = (weight of the chord's pitch classes / total weight)
 *             - missing_third_penalty   if the chord's third has no weight at all
 *             + bass_chord_tone_bonus   if the bass pitch class is a chord tone
 *
 * A score can therefore run from below 0 to 1.05 with the defaults. The candidates are ranked best
 * first. Two scores within TOLERANCE (1e-9) are tied, and a tie goes: to the chord whose root is the
 * bass (root position), then to the triad over the seventh, then to the lower scale degree, so the
 * ranking is total and deterministic.
 */

#define COMMON_PRACTICE_CANDIDATE_COUNT 14

typedef struct {
    int degree;                 /* 1..7: the chord's root as a scale degree */
    bool is_seventh;
    CommonPracticeTriad triad;  /* the triad on this degree (for a seventh, the triad it is built on) */
    CommonPracticeSeventh seventh; /* the seventh quality; meaningful only when is_seventh */
    int root_pitch_class;       /* 0..11 */
    double score;
} CommonPracticeRankedChord;

/* Ranks all 14 candidates for weights in a major key on tonic_pitch_class (0..11), best first. */
CommonPracticeWeightedStatus common_practice_rank_candidates(const CommonPracticeWeights *weights,
                                                              int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                              const CommonPracticeWeightedConfig *config,
                                                              CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT]);

/* The chord's pitch classes as a 12-bit mask (bit pc set), for a key on tonic_pitch_class: its root, third
   and fifth, and its seventh when is_seventh. 0 for an invalid chord or tonic. */
unsigned common_practice_ranked_chord_mask(const CommonPracticeRankedChord *chord, int tonic_pitch_class);

#endif
