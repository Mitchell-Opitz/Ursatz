#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_FRAMEWORK_H

#include "theory/framework.h"

/*
 * sixteenth-century-style-v1: Theory Framework for the contrapuntal style
 * of 16th-century imitative choral polyphony (Palestrina and his
 * contemporaries): the melodic, harmonic, dissonance, and rhythm rules of
 * the style, and imitation between the parts. Built as a Framework
 * extension module physically outside src/theory/, the same shape as
 * standard-counterpoint; this module never modifies src/theory/ or
 * src/rules/.
 *
 * Unlike the species of standard-counterpoint, which are style-independent
 * training rules for two parts, these rules describe one repertoire and
 * work on any number of parts. The one dependency on another framework is
 * one way: this module reads the pitch arithmetic, intervals, and slot
 * types of frameworks/standard-counterpoint, and that framework knows
 * nothing of this one. See README.md for the full scope statement.
 *
 * Dependency ceiling: canonical L0-L6 interfaces, theory/framework.h,
 * rules/rule.h, rules/constraint.h, and standard-counterpoint/. Enforced by
 * test_architecture_sixteenth_century_style.c.
 */

/*
 * Returns the single shared sixteenth-century-style-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must not
 * destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *sixteenth_century_framework(void);

#endif
