#ifndef MUSIC_SEMANTICS_ANALYSIS_KEY_H
#define MUSIC_SEMANTICS_ANALYSIS_KEY_H

#include <stdbool.h>

#include "model/pitch/pitch_material.h"
#include "model/pitch/pitch_spelling.h"
#include "model/time/musical_time.h"

/*
 * AnalysisKey: the neutral key framework adapters accept (R-242): a tonic spelled as letter and accidental and the pitch material of
 * the key. `material` is a pointer into the knowledge snapshot that outlives the key; the key's quality (major, minor) is read from
 * it (pitch_material_third, common_practice_key_mode_of). A key written as a literal may leave it NULL where the code that reads it
 * does not. id is borrowed and may be NULL.
 *
 * AnalysisKeyEvent places an AnalysisKey at a time (borrowed) for key-change input.
 */
typedef struct {
    const char *id;
    PitchLetter tonic_letter;
    int tonic_accidental;
    const PitchMaterial *material;
} AnalysisKey;

typedef struct {
    MusicalTime time;
    AnalysisKey key;
} AnalysisKeyEvent;

/* 0 = C to 11 = B through pitch_class_of_letter; -1 for NULL. */
int analysis_key_tonic_pitch_class(const AnalysisKey *key);

/* Same tonic spelling and material (the same entry, by id; two NULL materials are the same); ids of the keys are ignored. False for NULL. */
bool analysis_key_equals(const AnalysisKey *a, const AnalysisKey *b);

/*
 * The key a signature of `fifths` sharps (negative: flats) names in `material`: the tonic is `fifths` steps along the line of fifths
 * from C for a major material and three steps further for a minor one (the relative minor), and `material` is in the key. False
 * beyond plus or minus 7, for a NULL material or one with no signature, or for NULL out; *out is then untouched. id is set to NULL.
 */
bool analysis_key_from_fifths(int fifths, const PitchMaterial *material, AnalysisKey *out);

#endif
