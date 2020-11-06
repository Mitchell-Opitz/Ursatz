#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-tonality-v1: Theory Framework for the tonal conventions of
 * Western music -- major scales, scale degrees and their names, which
 * key a key signature belongs to, and the circle of fifths. Built as a
 * Framework extension module physically outside src/theory/, the same
 * shape as standard-notation; this module never modifies src/theory/ or
 * src/rules/, and does not depend on any other framework module.
 *
 * Scope is the pitch-relationship rules of tonal music. How accidentals
 * are written and ordered in a key signature is notation, and lives in
 * standard-notation. See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_tonality.c.
 */

/*
 * Returns the single shared standard-tonality-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_tonality_framework(void);

#endif
