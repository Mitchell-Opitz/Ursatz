#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_NON_CHORD_TONES_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_NON_CHORD_TONES_H

/*
 * The fallback when no classifier is injected. The non-chord-tone owner is motive_tone_roles
 * (src/analysis/motive_tone_roles.h); frameworks cannot call kernel L8, so the owner arrives as a
 * NonChordToneClassifier in CommonPracticeWeightedConfig (docs/analytic-job-owners.md).
 */

#include <stdbool.h>
#include <stddef.h>

#include "semantics/non_chord_tone.h"
#include "weighted_identification.h"

/*
 * Non-chord-tone labels for common-practice-minimal-v1: once a chord is chosen for each segment, the
 * members whose pitch class is not in it ("leftover tones") are labeled with the shared NonChordTone enum
 * (semantics/non_chord_tone.h) by how they are approached and left. Only existing NonChordTone values are
 * used, and anything that does not clearly fit one is left UNCLASSIFIED: no guess is made.
 *
 * A tone's neighbors. Approach and departure are the notes before and after it:
 *   - With a voice id, the same voice's notes: the previous and the next by onset (the whole sequence is
 *     searched, so a neighbor in another segment, or the same one, is found).
 *   - Without one, the NEAREST PITCH in the adjacent segments: the member of the previous segment, and the
 *     member attacked in the next segment, whose pitch is closest to the tone's (other than the tone's own
 *     note). Two equally near neighbors on either side are ambiguous and give no neighbor.
 * A step is 1 or 2 semitones; a leap is more than 2. A repeated pitch is neither.
 *
 * The rules (t is the tone, a its approach, d its departure):
 *   passing        step in, step out, in the same direction
 *   neighbor       step in, step out, in opposite directions
 *   appoggiatura   leap in, step out, in the opposite direction to the leap
 *   escape tone    step in, leap out, in the opposite direction to the step (as NonChordTone documents it)
 *   suspension     held over from the previous segment (its attack was earlier), then d is a step DOWN
 *   anticipation   attacked here, and its pitch sounds again in the next segment, where its pitch class is
 *                  a tone of that segment's chord
 *   pedal          the same note is the bass (lowest sounding pitch) of three or more consecutive segments
 *                  including this one
 * A tone that matches more than one rule, or none, is unclassified. A retardation (held, resolving up), a
 * double neighbor or a changing tone is not recognised, so those are unclassified.
 */

/* One segment of the sequence: its members (a view; not owned). */
typedef struct {
    const CommonPracticeMember *members;
    size_t member_count;
    const char *id; /* the segment's Sonority EntityID value, for the Chords built over it */
    long beat;      /* segments sharing a non-zero beat lie inside that one beat; 0 when unknown or spanning beats */
} CommonPracticeSegment;

typedef struct {
    size_t member_index;  /* into the segment's members */
    bool classified;
    NonChordTone tone;    /* meaningful only when classified */
} CommonPracticeToneLabel;

typedef enum {
    COMMON_PRACTICE_TONES_OK = 0,
    COMMON_PRACTICE_TONES_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_TONES_ERR_BAD_INDEX,
    COMMON_PRACTICE_TONES_ERR_ALLOC
} CommonPracticeToneStatus;

/*
 * Labels the leftover tones of segments[index]. chord_pitch_class_masks[i] is the chosen chord's pitch
 * classes for segment i as a 12-bit mask (bit pc set), or 0 for a segment with no chord, in which case it
 * has no leftover tones and gives no anticipation target. On OK, *out_labels is a newly allocated array of
 * *out_count labels (NULL when there are none), in member order, that the caller frees.
 */
CommonPracticeToneStatus common_practice_label_leftover_tones(const CommonPracticeSegment *segments,
                                                               const unsigned *chord_pitch_class_masks,
                                                               size_t segment_count, size_t index,
                                                               CommonPracticeToneLabel **out_labels,
                                                               size_t *out_count);

#endif
