#ifndef MUSIC_PITCH_PITCH_CLASS_H
#define MUSIC_PITCH_PITCH_CLASS_H

#include <stdbool.h>

#include "pitch/pitch_identity.h"

/*
 * PitchClass: an equivalence class of PitchIdentity under a declared
 * relation (e.g. octave equivalence, enharmonic equivalence under some
 * tuning, or any other relation a caller defines).
 *
 * The relation itself is NOT hard-coded here: it is supplied at
 * construction as a PitchClassEquivalenceFn, matching this platform's
 * "no single theory baked in" principle and the same
 * caller-supplies-the-relation pattern core/equality.h already uses for
 * CanonicalizeFn/SemanticEqualsFn. This type only provides the plug-in
 * point and the representative-element storage; it does not itself
 * decide what "equivalent" means for any particular musical theory.
 *
 * A PitchClassEquivalenceFn MUST be a true equivalence relation
 * (reflexive, symmetric, transitive) over PitchIdentity for
 * pitch_class_equals to behave sensibly; this type has no way to verify
 * that at runtime -- callers are responsible for supplying a relation
 * that actually is one, and should property-test it (see
 * tests/pitch/test_pitch_class.c for the pattern).
 *
 * Two PitchClass values are compared only when they were constructed
 * with the SAME equivalence function (by pointer identity):
 * pitch_class_equals returns false for a pair built under different
 * relations, since comparing "equivalent under relation A" against
 * "equivalent under relation B" is not a meaningful question this type
 * can answer.
 */

typedef struct PitchClass PitchClass;

typedef bool (*PitchClassEquivalenceFn)(const PitchIdentity *a, const PitchIdentity *b);

typedef enum {
    PITCH_CLASS_OK = 0,
    PITCH_CLASS_ERR_NULL_REPRESENTATIVE,
    PITCH_CLASS_ERR_NULL_EQUIVALENCE,
    PITCH_CLASS_ERR_ALLOC
} PitchClassResult;

/* Takes ownership of representative on success (destroyed by pitch_class_destroy). */
PitchClassResult pitch_class_create(PitchIdentity *representative, PitchClassEquivalenceFn equivalence,
                                     PitchClass **out);
void pitch_class_destroy(PitchClass *pitch_class);

const PitchIdentity *pitch_class_representative(const PitchClass *pitch_class);
PitchClassEquivalenceFn pitch_class_equivalence(const PitchClass *pitch_class);

/*
 * Value equality: true when a and b were built under the same
 * equivalence function AND that function holds between their
 * representatives. False if the two were built under different
 * equivalence functions, even if their representatives happen to be
 * identical.
 */
bool pitch_class_equals(const PitchClass *a, const PitchClass *b);

#endif