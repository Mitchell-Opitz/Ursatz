#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-notation-v1: Theory Framework for the conventions of Western
 * staff notation -- how notes are placed on a staff, how clefs assign
 * pitches to lines and spaces, ledger lines, stem direction, and the
 * drawing of seconds. Built as a Framework extension module physically
 * outside src/theory/, the same shape as common-practice-minimal; this
 * module never modifies src/theory/ or src/rules/.
 *
 * Scope is deliberately limited to rules about WHERE and HOW notation is
 * placed. Hand-drawing instructions (clef shapes, notehead size and tilt,
 * stem length, beam thickness) are engraving craft, not theory, and are
 * excluded. See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_notation.c.
 */

/*
 * Returns the single shared standard-notation-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_notation_framework(void);

#endif
