#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-texture-v1: Theory Framework for musical texture, the density
 * of and interaction between a work's voices: monophony, heterophony,
 * homophony (homorhythm, melody and accompaniment), and polyphony, and
 * the way a work moves between them. Built as a Framework extension
 * module physically outside src/theory/, the same shape as
 * standard-tonality, standard-rhythm, and standard-meter; this module
 * never modifies src/theory/ or src/rules/, and does not depend on any
 * other framework module.
 *
 * Texture is the Interpretation-family type this framework's names are
 * the vocabulary for (src/interpretation/texture.h). Classification takes
 * features the caller has already measured; reading them from events is
 * analysis (src/analysis), not a rule. See README.md for the full scope
 * statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_texture.c.
 */

/*
 * Returns the single shared standard-texture-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_texture_framework(void);

#endif
