#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_VOCABULARY_H

#include "identity/entity_id.h"
#include "interpretation/key.h"

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
 * common-practice-minimal-v1 (e.g. tonic_name "C", mode MAJOR ->
 * label "C major"). tonic_name is copied, required, non-empty.
 *
 * Takes ownership of context_target_id on success (destroyed
 * transitively by key_destroy(*out)); on failure the caller still owns
 * context_target_id.
 */
KeyResult common_practice_build_key(CommonPracticeKeyMode mode, const char *tonic_name, EntityID *context_target_id,
                                     Key **out);

#endif