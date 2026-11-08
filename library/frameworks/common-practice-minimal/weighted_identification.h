#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_IDENTIFICATION_H

#include <stdbool.h>
#include <stddef.h>

#include "chord_vocabulary.h"
#include "chord_vocabulary_seventh.h"
#include "key_vocabulary.h"
#include "semantics/analysis_note.h"
#include "semantics/beat_strength.h"
#include "semantics/tone_line.h"
#include "time/time_span.h"

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
 * earlier segment. The bass members (every member on the pitch class of the lowest sounding pitch, octave
 * doublings included) are then multiplied by the bass weight. A pitch class's weight is the sum over the members on it, and the
 * weights and their total are what scoring reads. Defaults (CommonPracticeWeightedConfig):
 *
 *     strong 1.0, medium 0.7, weak 0.4   (BeatStrength has three values; they are evenly spaced from the
 *                                         downbeat's 1.0 to the weakest 0.4)
 *     held 0.6      bass weight 1.1   (chosen on the accuracy harness and the Framework v1 verification: higher
 *                                       values lower chorale root agreement, 1.0 loses Field 5's home key; see
 *                                       the README)
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

/*
 * The member an AnalysisNote makes in a segment spanning `segment`, with the attack's beat strength the caller read. note_id, pitch
 * (midi) and voice_id carry over. held is true when the note starts before the segment; onset is the note's start and duration the
 * overlap of the note with the segment, both in whole notes as double (rational_to_double). False, *out untouched, for a NULL argument,
 * an invalid note, an unknown pitch or a note that does not overlap the segment.
 */
bool common_practice_member_from_analysis_note(const AnalysisNote *note, const TimeSpan *segment, BeatStrength strength,
                                               CommonPracticeMember *out);

typedef struct {
    double strength_factor[BEAT_STRENGTH_COUNT]; /* indexed by BeatStrength */
    double held_factor;
    double bass_weight;
    double missing_third_penalty;  /* used by scoring */
    double bass_chord_tone_bonus;  /* used by scoring */
    double min_coverage;           /* used by scoring: a best score below it is "no match" */
    /* Labels leftover tones in common_practice_identify_weighted_sequence. NULL (the default) keeps
       common_practice_label_leftover_tones. With one, the leftover members that have a voice_id are labelled per
       voice: the voice's members, segment after segment, form a line, each entry carrying the chord mask of the
       segment it sounds in (a held note is an entry in every segment it sounds in: the suspension and pedal rules
       need it against the new chord), and the labels are mapped back to the leftover members. Members without a
       voice_id, and a voice the classifier refuses, keep the framework rule. Line onsets are the member onsets
       rounded to 1/960 of a whole note. */
    NonChordToneClassifier classifier;
    void *classifier_context;
} CommonPracticeWeightedConfig;

/* The documented defaults: strong 1.0, medium 0.7, weak 0.4, held 0.6, bass 1.1, missing-third penalty
   0.15, bass chord-tone bonus 0.05, min coverage 0.5. */
CommonPracticeWeightedConfig common_practice_weighted_config_default(void);

typedef enum {
    COMMON_PRACTICE_WEIGHTED_OK = 0,
    COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG,
    COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER,
    COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE, /* a mode that is neither major nor minor */
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
 *
 * bass_pitch overrides the bass: NULL takes the lowest sounding pitch. With one, bass_pitch and
 * bass_pitch_class of *out are that pitch (even with no members), and the bass weight multiplies the
 * members on its pitch class; when no member sounds it, the weights are unchanged and only the bass is set.
 */
CommonPracticeWeightedStatus common_practice_weight_segment(const CommonPracticeMember *members, size_t count,
                                                             const CommonPracticeWeightedConfig *config,
                                                             CommonPracticeWeights *out, const int *bass_pitch);

/*
 * SCORING (step 2). The candidates are the diatonic chords of the key's own mode.
 *
 * MAJOR. The 14 chords of a major key: the seven diatonic triads (I ii iii IV V vi vii-dim, chord_vocabulary.h)
 * and the seven diatonic sevenths, each root taking the seventh quality its scale gives (I and IV major7, ii iii
 * vi minor7, V dominant7, vii half-diminished7; chord_vocabulary_seventh.h).
 *
 * MINOR. The union of the diatonic triads and sevenths of natural, harmonic and melodic minor, 26 chords, by
 * semitones above the tonic (triads; sevenths):
 *
 *     0   minor                      minor7
 *     2   diminished, minor          half-diminished7, minor7
 *     3   major, augmented           major7
 *     5   minor, major               minor7, dominant7
 *     7   major, minor               dominant7, minor7
 *     8   major                      major7
 *     9   diminished                 half-diminished7
 *     10  major                      dominant7
 *     11  diminished                 diminished7, half-diminished7
 *
 * It includes the augmented III and the fully-diminished vii-dim-7 and leaves out the minor-major and
 * augmented-major sevenths. Minor triads are quality-labeled ("minor", "major", "diminished", "augmented"):
 * a degree numeral depends on the form of the scale, which is standard-tonality's work.
 *
 * Fully-diminished sevenths outside minor, chromatic and applied chords are not candidates. The key comes from
 * the caller (a whole-piece key until local key regions exist).
 *
 *     score = (weight of the chord's pitch classes / total weight)
 *             - missing_third_penalty   if the chord's third has no weight at all
 *             + bass_chord_tone_bonus   if the bass pitch class is a chord tone
 *
 * A score can therefore run from below 0 to 1.05 with the defaults. A candidate is only eligible if its ROOT
 * sounds (has weight): a chord whose root is absent is not a reading the notes support. Without this, a plain
 * C-E-G is fully covered by A minor seventh (A C E G) and every bare triad would tie with the seventh above
 * it. So fewer candidates may be ranked (none when nothing diatonic sounds as a root). The ranking is
 * best first. Two scores within TOLERANCE (1e-9) are tied, and a tie goes: to the chord whose root is the
 * bass (root position), then to the triad over the seventh, then to the lower scale degree, then to the lower
 * root, then (the same root in two forms of minor) to the FORM: the raised form wins for V, V7, vii-dim and
 * vii-dim-7, the natural form everywhere else (minor v over V is not preferred; V over v is; iv over IV;
 * III over III+; ii-dim over ii). The ranking is total and deterministic.
 */

#define COMMON_PRACTICE_MAJOR_CANDIDATE_COUNT 14
#define COMMON_PRACTICE_MINOR_CANDIDATE_COUNT 26
#define COMMON_PRACTICE_CANDIDATE_COUNT COMMON_PRACTICE_MINOR_CANDIDATE_COUNT /* the most any mode ranks */

typedef struct {
    int degree;                 /* 1..7: the chord's root as a scale degree (minor: 6 and 7 also for the raised forms) */
    bool is_seventh;
    CommonPracticeTriad triad;  /* major: the triad on this degree (for a seventh, the triad it is built on).
                                   minor: the triad's quality (COMMON_PRACTICE_TRIAD_MAJOR, _MINOR, _DIMINISHED,
                                   _AUGMENTED), on a seventh the quality of the triad it is built on */
    CommonPracticeSeventh seventh; /* the seventh quality; meaningful only when is_seventh */
    int root_pitch_class;       /* 0..11 */
    double score;
    int tonic_offset;           /* the root's semitones above the tonic, 0..11 */
    int form_rank;              /* 0: preferred form on a tie; 1: the other form of the same root (minor only) */
} CommonPracticeRankedChord;

/* Ranks the eligible candidates (root sounding) for weights in a key on tonic_pitch_class (0..11) in the
   given mode, best first: *out_count of them, at most COMMON_PRACTICE_CANDIDATE_COUNT, may be 0. */
CommonPracticeWeightedStatus common_practice_rank_candidates(const CommonPracticeWeights *weights,
                                                              int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                              const CommonPracticeWeightedConfig *config,
                                                              CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT],
                                                              size_t *out_count);

/* The chord's quality as root-relative semitones, ascending, in out[4]: 3 of them for a triad, 4 for a seventh.
   Returns the count, 0 for an invalid chord. */
size_t common_practice_ranked_chord_intervals(const CommonPracticeRankedChord *chord, int out[4]);

/* The quality name of the chord: "major", "minor", "diminished" or "augmented" for a triad read from its
   intervals, common_practice_seventh_label for a seventh; NULL for NULL or a set outside the vocabulary. */
const char *common_practice_ranked_chord_quality_name(const CommonPracticeRankedChord *chord);

/* Where the bass sits among the chord's tones: 0 root, 1 third, 2 fifth, 3 seventh; -1 when bass_pitch_class
   (0..11) is no chord tone, the chord is invalid or an argument is NULL. */
int common_practice_ranked_chord_inversion(const CommonPracticeRankedChord *chord, int bass_pitch_class);

/* The chord's pitch classes as a 12-bit mask (bit pc set): its root, third and fifth, and its seventh when
   is_seventh. 0 for an invalid chord. tonic_pitch_class (0..11) is only validated: the chord carries its root. */
unsigned common_practice_ranked_chord_mask(const CommonPracticeRankedChord *chord, int tonic_pitch_class);

#endif
