#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H

#include <stdbool.h>

#include "model/pitch/pitch_material.h"

/*
 * Key vocabulary for common-practice-minimal-v1: major and natural
 * minor only. Deliberately excludes harmonic minor and melodic minor
 * variants -- see README.md.
 */

typedef enum {
    COMMON_PRACTICE_KEY_MAJOR = 0,
    COMMON_PRACTICE_KEY_MINOR
} CommonPracticeKeyMode;

/* A key as the vocabulary gives it: the tonic's pitch class (0 = C ... 11 = B) and the pitch material of the key (borrowed). */
typedef struct {
    int tonic_pitch_class;
    const PitchMaterial *material;
} KeyStructure;

/*
 * The key tonic_name major/minor under common-practice-minimal-v1 (e.g. tonic_name "C", the major material -> C major). The material is read
 * as major or minor by its third (common_practice_key_mode_of) and stays in the structure. False (out untouched) for a NULL material or one
 * with another third, an empty or unreadable tonic_name (a letter A-G then '#' or 'b' accidentals) or a NULL out.
 */
bool common_practice_build_key(const PitchMaterial *material, const char *tonic_name, KeyStructure *out);

#endif
