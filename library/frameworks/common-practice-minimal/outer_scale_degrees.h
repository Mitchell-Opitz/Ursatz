#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_OUTER_SCALE_DEGREES_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_OUTER_SCALE_DEGREES_H

#include <stddef.h>

#include "analysis/outer_voice.h"
#include "interpretation/key.h"
#include "interpretation/scale_degree.h"
#include "key_vocabulary.h"

/*
 * The scale degree of the soprano and the bass in each segment, under common-practice-minimal-v1. The
 * outer lines (analysis/outer_voice.h) are theory-neutral pitch heights; here each present note becomes a
 * ScaleDegree (interpretation/scale_degree.h) relative to the caller's key, a framework-relative claim
 * whose FrameworkReference is this framework's (the Registry requires it of every ScaleDegree).
 *
 * Entry i is segment i: the chord segments are the same adaptive segments the lines and the chord
 * identification run over (weighted_chord_identification.h), so degrees line up with chords one to one. A
 * degree is given for EVERY segment, whether or not a chord matched it: it depends on the key, not on the
 * chord. An ABSENT entry in a line (a rest, or a register-guarded segment) has no ScaleDegree: that
 * line's pointer is NULL, which is different from a degree of whatever note was highest.
 *
 * The key is the caller's (a whole-piece key until local key regions exist). Key exposes a label and its
 * Interpretation only, so the tonic pitch class (0 = C ... 11 = B) and the mode come beside it; the Key
 * supplies the context id. The mode may be major or natural minor.
 *
 * Labels. A degree is "1" to "7" with a "b" or "#" for a chromatic alteration, by the pitch class above
 * the tonic:
 *     major          1 b2 2 b3 3 4 #4 5 b6 6 b7 7
 *     natural minor  1 b2 2 3 #3 4 #4 5 6 #6 7 #7
 * so in minor the diatonic degrees are the natural minor scale's own (3, 6 and 7 are the flat forms), and
 * the raised sixth and seventh of the melodic and harmonic forms are altered (#6, #7). A pitch is its
 * position under the importer's MIDI tuning: a position that is not a whole number of semitones is
 * unsupported.
 *
 * Each ScaleDegree's Interpretation: target is the NOTE's EntityID (the carrier of the pitch), context is
 * the Key's Interpretation id, the claim is the label, and the confidence is 1/1 (a pitch to a degree is a
 * deterministic mapping). Its pitch is the PitchIdentity of the note's pitch.
 */

typedef struct {
    ScaleDegree *soprano; /* owned; NULL when the soprano is absent in the segment */
    ScaleDegree *bass;    /* owned; NULL when the bass is absent in the segment */
} CommonPracticeOuterDegrees;

typedef struct {
    CommonPracticeOuterDegrees *items; /* owned; count entries; NULL when count is 0 */
    size_t count;
} CommonPracticeOuterDegreeList;

typedef enum {
    COMMON_PRACTICE_OUTER_OK = 0,
    COMMON_PRACTICE_OUTER_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_OUTER_ERR_BAD_TONIC,
    COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_MODE,
    COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_PITCH,
    COMMON_PRACTICE_OUTER_ERR_ALLOC
} CommonPracticeOuterStatus;

/* On OK, *out owns its degrees (common_practice_outer_degrees_destroy); otherwise *out is untouched. */
CommonPracticeOuterStatus common_practice_outer_scale_degrees(const OuterVoiceLines *lines, const Key *key,
                                                               int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                               CommonPracticeOuterDegreeList *out);

void common_practice_outer_degrees_destroy(CommonPracticeOuterDegreeList *list);

/* The label of the pitch class `pitch_class` (0..11) in a key on tonic_pitch_class and mode, per the table
   above; NULL for an invalid argument or an unsupported mode. */
const char *common_practice_scale_degree_label(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode);

#endif
