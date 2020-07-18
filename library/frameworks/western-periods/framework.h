#ifndef URSATZ_FRAMEWORKS_WESTERN_PERIODS_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_WESTERN_PERIODS_FRAMEWORK_H

#include "theory/framework.h"

/*
 * western-periods-v1: Theory Framework holding the conventional
 * stylistic periods of Western classical music -- Medieval, Renaissance,
 * Baroque, Classical, Romantic, Post-tonal. Built as a Framework
 * extension module physically outside src/theory/, the same shape as
 * the other frameworks/ modules; this module never modifies src/theory/
 * or src/rules/.
 *
 * Periods are a convention for grouping works with similar style, and
 * their boundaries are flexible. The framework records the generally
 * agreed years and lets them overlap; it does not assign any year to
 * exactly one period. See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_western_periods.c.
 */

/*
 * Returns the single shared western-periods-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *western_periods_framework(void);

#endif
