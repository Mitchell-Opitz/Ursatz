#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_FRAMEWORK_H

#include "theory/framework.h"

/*
 * baroque-ground-bass-v1: Theory Framework for the Baroque ground bass (in
 * the manner of the trio sonatas of Purcell): a bass line that repeats
 * throughout, with variety created above it by changes of texture and
 * rhythm and by imitation between the upper parts. Built as a Framework
 * extension module physically outside src/theory/, the same shape as
 * standard-counterpoint; this module never modifies src/theory/ or
 * src/rules/.
 *
 * The one dependency on another framework is one way: this module reads the
 * pitch arithmetic, intervals, and slot types of
 * frameworks/standard-counterpoint, and that framework knows nothing of this
 * one. See README.md for the full scope statement.
 *
 * Dependency ceiling: canonical L0-L6 interfaces, theory/framework.h,
 * rules/rule.h, rules/constraint.h, and standard-counterpoint/. Enforced by
 * test_architecture_baroque_ground_bass.c.
 */

/*
 * Returns the single shared baroque-ground-bass-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not destroy
 * it. Returns NULL only if the one-time construction failed (allocation
 * failure).
 */
const Framework *baroque_ground_bass_framework(void);

#endif
