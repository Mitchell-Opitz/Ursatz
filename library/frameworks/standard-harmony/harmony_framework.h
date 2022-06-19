#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-harmony-v1: Theory Framework for the basics of harmony in common-
 * practice Western music, after Open Music Theory (Introduction to Harmony,
 * Cadences, and Phrase Endings): the three harmonic functions (tonic,
 * predominant, dominant), the phrase model (the usual order of the functions in
 * a phrase), the cadences that end a phrase (perfect and imperfect authentic,
 * half, and the subverted cadence), how to hear them, and how to write the bass
 * and soprano of each. Built as a Framework extension module physically outside
 * src/theory/, the same shape as standard-tonality; this module never modifies
 * src/theory/ or src/rules/.
 *
 * Chords are read as plain data (the scale degree of the root, the inversion),
 * not parsed from Roman numerals, so this module depends on no other framework.
 * Roman numerals are frameworks/standard-tonality's, and voicing the inner
 * voices is frameworks/standard-voicing's. See README.md for the full scope
 * statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) plus
 * rules/rule.h and rules/constraint.h. Enforced by
 * test_architecture_standard_harmony.c.
 */

/*
 * Returns the single shared standard-harmony-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not destroy
 * it. Returns NULL only if the one-time construction failed (allocation
 * failure).
 */
const Framework *standard_harmony_framework(void);

#endif
