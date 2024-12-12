#ifndef URSATZ_COMPOSER_REGENERATE_H
#define URSATZ_COMPOSER_REGENERATE_H

#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Locking and regenerating (R-072).
 *
 * plan_lock makes material Given: a node and every node under it and every lane span they own, or one lane span
 * alone. Given material is never changed by the Composer, and nothing leaves Given, so locking is permanent in the
 * plan; unlocking is reading an earlier plan file.
 *
 * Only what is written can be locked: a lane span still open (Constrained) has nothing to keep, and locking it would
 * leave a hole nothing may fill. The call is then refused with PLAN_ERR_BAD_PAYLOAD and nothing is changed.
 * PLAN_ERR_NOT_FOUND for a target that does not exist.
 */
PlanResult plan_lock(PiecePlan *plan, FillTarget target);

#endif
