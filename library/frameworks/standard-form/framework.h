#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-form-v1: Theory Framework for how Western tonal music is organized into
 * phrase-level form -- the hierarchy of form, motives, phrases, and the segmentation analysis
 * that divides a phrase into ideas. Built as a Framework extension module physically outside
 * src/theory/, the same shape as standard-meter and standard-harmony; this module never
 * modifies src/theory/ or src/rules/. The names it uses (FormLevel, MotiveKind) are plain
 * value types in src/semantics/; this framework gives them their conventions. See README.md
 * for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) -- canonical L0-L6
 * interfaces only. Enforced by test_architecture_standard_form.c.
 */

/*
 * Returns the single shared standard-form-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not
 * destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_form_framework(void);

#endif
