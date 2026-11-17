#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H

#include "model/identity/entity_id.h"
#include "model/interpretation/key.h"
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

/*
 * Builds a real, populated Key claiming that the Context named by
 * context_target_id is tonic_name major/minor under
 * common-practice-minimal-v1 (e.g. tonic_name "C", the major material ->
 * label "C major"). The material is read as major or minor by its third
 * (common_practice_key_mode_of) and stays in the Key's structure; KEY_ERR_BAD_STRUCTURE for a NULL material
 * or one with another third. tonic_name is copied, required, non-empty.
 *
 * Takes ownership of context_target_id on success (destroyed
 * transitively by key_destroy(*out)); on failure the caller still owns
 * context_target_id.
 */
KeyResult common_practice_build_key(const PitchMaterial *material, const char *tonic_name, EntityID *context_target_id,
                                     Key **out);

#endif