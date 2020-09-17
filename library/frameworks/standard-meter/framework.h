#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_FRAMEWORK_H

#include "theory/framework.h"

/*
 * standard-meter-v1: Theory Framework for how Western staff notation
 * organizes rhythm into meter -- beats, beat groupings, time signatures,
 * measures, pickups, counting, and beaming. Built as a Framework
 * extension module physically outside src/theory/, the same shape as
 * standard-rhythm and standard-notation; this module never modifies
 * src/theory/ or src/rules/. Note and rest values themselves are
 * standard-rhythm-v1's job.
 *
 * The TimeSignature type (time/time_signature.h) carries only a beat
 * count and a beat unit; this framework gives it its meaning. This
 * module depends on time/ types (NoteValue, Duration, TimeSignature) and
 * not on standard-rhythm, so each framework stays independently
 * replaceable. See README.md for the full scope statement.
 *
 * Dependency ceiling: same as Framework itself (theory/framework.h) --
 * canonical L0-L6 interfaces only. Enforced by
 * test_architecture_standard_meter.c.
 */

/*
 * Returns the single shared standard-meter-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not
 * destroy it. Returns NULL only if the one-time construction failed
 * (allocation failure).
 */
const Framework *standard_meter_framework(void);

#endif
