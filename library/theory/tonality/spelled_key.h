#ifndef URSATZ_THEORY_TONALITY_SPELLED_KEY_H
#define URSATZ_THEORY_TONALITY_SPELLED_KEY_H

#include <stdbool.h>

#include "model/pitch/pitch_material.h"
#include "model/pitch/pitch_spelling.h"
#include "model/time/musical_time.h"

/*
 * SpelledKey: the neutral key: a tonic spelled as letter and accidental and the pitch material of the key (R-242). `material` is a
 * pointer into the knowledge snapshot that outlives the key; the key's quality (major, minor) is read from it (pitch_material_third,
 * common_practice_key_mode_of). A key written as a literal may leave it NULL where the code that reads it does not. id is borrowed
 * and may be NULL.
 *
 * SpelledKeyEvent places a SpelledKey at a time (borrowed) for key-change input.
 */
typedef struct {
    const char *id;
    PitchLetter tonic_letter;
    int tonic_accidental;
    const PitchMaterial *material;
} SpelledKey;

typedef struct {
    MusicalTime time;
    SpelledKey key;
} SpelledKeyEvent;

/* 0 = C to 11 = B through pitch_class_of_letter; -1 for NULL. */
int spelled_key_tonic_pitch_class(const SpelledKey *key);

/* Same tonic spelling and material (the same entry, by id; two NULL materials are the same); ids of the keys are ignored. False for NULL. */
bool spelled_key_equals(const SpelledKey *a, const SpelledKey *b);

/*
 * The key a signature of `fifths` sharps (negative: flats) names in `material`: the tonic is `fifths` steps along the line of fifths
 * from C for a major material and three steps further for a minor one (the relative minor), and `material` is in the key. False
 * beyond plus or minus 7, for a NULL material or one with no signature, or for NULL out; *out is then untouched. id is set to NULL.
 */
bool spelled_key_from_fifths(int fifths, const PitchMaterial *material, SpelledKey *out);

#endif
