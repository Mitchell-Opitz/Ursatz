#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_FRAMEWORK_H

#include "theory/framework.h"

/*
 * high-baroque-fugue-v1: Theory Framework for the fugal exposition of the
 * high Baroque (Bach, Handel, and their contemporaries): the order of the
 * voice entries, the answer to the subject, and the countersubject. Built
 * as a Framework extension module physically outside src/theory/, the same
 * shape as standard-counterpoint; this module never modifies src/theory/ or
 * src/rules/.
 *
 * The one dependency on another framework is one way: this module reads the
 * pitch arithmetic, intervals, and slot types of
 * frameworks/standard-counterpoint, and that framework knows nothing of this
 * one. See README.md for the full scope statement.
 *
 * Dependency ceiling: canonical L0-L6 interfaces, theory/framework.h,
 * rules/rule.h, rules/constraint.h, and standard-counterpoint/. Enforced by
 * test_architecture_high_baroque_fugue.c.
 */

/*
 * Returns the single shared high-baroque-fugue-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not destroy
 * it. Returns NULL only if the one-time construction failed (allocation
 * failure).
 */
const Framework *high_baroque_fugue_framework(void);

#endif
