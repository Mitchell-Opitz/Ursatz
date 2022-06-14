#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_FRAMEWORK_H

#include "theory/framework.h"

/*
 * scale-harmonization-v1: Theory Framework for harmonizing a bass scale in the
 * schema and partimento tradition: the Rule of the Octave, a cheat sheet for
 * harmonizing a bass line with one chord for each scale degree, and the
 * sequential harmonizations of a scale (parallel chords, 5-6, 7-6, and so on),
 * patterns of harmonies that repeat in the direction of the scale. Built as a
 * Framework extension module physically outside src/theory/, the same shape as
 * standard-counterpoint; this module never modifies src/theory/ or src/rules/.
 *
 * Its dependencies on other frameworks are one way: this module reads the
 * pitch arithmetic and intervals of frameworks/standard-counterpoint and the
 * scale degrees of frameworks/galant-schemas, and neither knows anything of
 * this one. See README.md for the full scope statement.
 *
 * Dependency ceiling: canonical L0-L6 interfaces, theory/framework.h,
 * rules/rule.h, rules/constraint.h, standard-counterpoint/, and
 * galant-schemas/. Enforced by test_architecture_scale_harmonization.c.
 */

/*
 * Returns the single shared scale-harmonization-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not destroy
 * it. Returns NULL only if the one-time construction failed (allocation
 * failure).
 */
const Framework *scale_harmonization_framework(void);

#endif
