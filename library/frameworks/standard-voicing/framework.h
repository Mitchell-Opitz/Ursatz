#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-voicing-v1: Theory Framework for writing a chord in strict
 * SATB style -- the six rules a four-part chord follows: stem direction,
 * chord construction, range, spacing, voice crossing, and doubling.
 * Built as a Framework extension module physically outside src/theory/,
 * the same shape as standard-tonality, standard-notation, and
 * standard-texture; this module never modifies src/theory/ or
 * src/rules/, and does not depend on any other framework module.
 *
 * What chord a voicing realizes (its tones, its inversion, its leading
 * tone) is supplied by the caller -- from standard-tonality's chord and
 * Roman numeral rules, or any other source. See README.md for the full
 * scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_voicing.c.
 */

/*
 * Returns the single shared standard-voicing-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_voicing_framework(void);

#endif
