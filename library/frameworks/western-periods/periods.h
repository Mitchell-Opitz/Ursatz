#ifndef URSATZ_FRAMEWORKS_WESTERN_PERIODS_PERIODS_H
#define URSATZ_FRAMEWORKS_WESTERN_PERIODS_PERIODS_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/historical_period.h"

/*
 * Stylistic periods for western-periods-v1. Musicologists periodize
 * Western classical music into eras whose works tend to share style
 * characteristics. The generally agreed (approximate, "c.") years:
 *
 *   Medieval      c. 600-1400
 *   Renaissance   c. 1400-1600
 *   Baroque       c. 1600-1750
 *   Classical     c. 1750-1820
 *   Romantic      c. 1800-1910
 *   Post-tonal    c. 1900-present   (open-ended)
 *
 * Rules:
 *   PERIOD_BOUNDARIES  the six periods and their conventional years
 *   PERIOD_OVERLAP     the boundaries are flexible and neighbouring
 *                      periods overlap (Classical/Romantic 1800-1820,
 *                      Romantic/Post-tonal 1900-1910); a year can
 *                      belong to more than one period, and this
 *                      framework never picks a winner
 *
 * Boundaries shared by two periods (1400, 1600, 1750) count for both,
 * since years are inclusive on both ends -- in line with the flexibility
 * of the convention.
 */

typedef enum {
    WESTERN_PERIODS_RULE_PERIOD_BOUNDARIES = 0,
    WESTERN_PERIODS_RULE_PERIOD_OVERLAP,
    WESTERN_PERIODS_RULE_COUNT
} WesternPeriodsRule;

#define WESTERN_PERIODS_COUNT 6
/* The most periods any one year can belong to. */
#define WESTERN_PERIODS_MAX_OVERLAP 2

const Rule *western_periods_rule(WesternPeriodsRule which);
const Constraint *western_periods_constraint(WesternPeriodsRule which);

/*
 * PERIOD_BOUNDARIES: the period at index 0..5, in chronological order
 * (Medieval first, Post-tonal last). Owned by this module; callers must
 * not destroy it. NULL for an out-of-range index or if construction
 * failed.
 */
const HistoricalPeriod *western_periods_at(size_t index);

/* The period called `name` (exact match), or NULL. */
const HistoricalPeriod *western_periods_find(const char *name);

/*
 * PERIOD_OVERLAP: writes up to `capacity` periods containing `year` to
 * `out` in chronological order and returns how many contain it (which may
 * exceed capacity; only the first `capacity` are written). 0 for a year
 * before the Medieval period starts.
 */
size_t western_periods_containing_year(long year, const HistoricalPeriod **out, size_t capacity);

#endif
