#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-markings-v1: Theory Framework for the conventional markings of
 * Western staff notation other than pitch placement -- dynamics,
 * articulations, tempo indications, and structural markers (fermata,
 * repeats, endings, D.C./D.S.). Built as a Framework extension module
 * physically outside src/theory/, the same shape as common-practice-
 * minimal and standard-notation; this module never modifies src/theory/
 * or src/rules/. Pitch placement (staff, clefs, accidentals) is
 * standard-notation-v1's job.
 *
 * The marking types themselves (Dynamics, Articulation, TempoMarking,
 * StructuralMarker) carry opaque strings; this framework gives the Italian
 * vocabulary its meaning -- order, grouping, direction, traversal.
 * See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_markings.c.
 */

/*
 * Returns the single shared standard-markings-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_markings_framework(void);

#endif
