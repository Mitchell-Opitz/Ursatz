#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-rhythm-v1: Theory Framework for how Western staff notation
 * writes rhythm -- note and rest values, their names, dots, and ties.
 * Built as a Framework extension module physically outside src/theory/,
 * the same shape as standard-notation and standard-markings; this module
 * never modifies src/theory/ or src/rules/. Pitch placement is
 * standard-notation-v1's job.
 *
 * The NoteValue type (time/note_value.h) carries only an exponent and a
 * dot count; this framework gives it its notation meaning -- names,
 * hierarchy, noteheads, flags, rests, dots, ties.
 * See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_rhythm.c.
 */

/*
 * Returns the single shared standard-rhythm-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_rhythm_framework(void);

#endif
