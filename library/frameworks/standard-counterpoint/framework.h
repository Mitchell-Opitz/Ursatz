#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-counterpoint-v1: Theory Framework for strict two-part
 * counterpoint in the species tradition (Fux, Gradus ad Parnassum): the
 * consonance categories, the types of two-part motion, the cantus
 * firmus, and the melodic and harmonic rules every species shares. Built
 * as a Framework extension module physically outside src/theory/, the
 * same shape as standard-tonality, standard-texture, and
 * standard-voicing; this module never modifies src/theory/ or src/rules/,
 * and does not depend on any other framework module.
 *
 * The rules are style-independent training rules, not a claim about any
 * one repertoire. The notes of a line, the tonic of a cantus firmus, and
 * the limits a caller wants applied are supplied by the caller. See
 * README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_counterpoint.c.
 */

/*
 * Returns the single shared standard-counterpoint-v1 Framework instance,
 * lazily constructed on first call. Owned by this module; callers must
 * not destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_counterpoint_framework(void);

#endif
