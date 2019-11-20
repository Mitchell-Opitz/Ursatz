#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_FRAMEWORK_H

#include "theory/framework.h"

/*
 * common-practice-minimal-v1: the first real Theory Framework instance
 * populated in this codebase. It is a Framework extension module, built
 * physically outside src/theory/ per docs/modules/theory.md ("built as
 * extensions on top of Framework/Rule/Constraint after this module,
 * never as changes to the L7 kernel itself") -- this module never
 * modifies src/theory/ or src/rules/.
 *
 * Scope: basic diatonic triad identification and major/natural-minor
 * key naming for Western common-practice tonal harmony. See README.md
 * in this directory for the full scope statement, including what is
 * deliberately excluded.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_common_practice_minimal.c.
 */

/*
 * Returns the single shared common-practice-minimal-v1 Framework
 * instance, lazily constructed on first call. Owned by this module;
 * callers must not destroy it. Returns NULL only if the one-time
 * construction failed (allocation failure).
 */
const Framework *common_practice_minimal_framework(void);

#endif