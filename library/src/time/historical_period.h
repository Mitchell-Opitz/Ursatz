#ifndef MUSIC_TIME_HISTORICAL_PERIOD_H
#define MUSIC_TIME_HISTORICAL_PERIOD_H

#include <stdbool.h>

/*
 * HistoricalPeriod: a named stretch of calendar years, such as the
 * Baroque (c. 1600-1750). This is historical time, not musical time --
 * it has nothing to do with MusicalTime/Duration, which measure position
 * and length inside a piece.
 *
 * Years are inclusive on both ends. The end year is optional: a period
 * that is still going ("post-tonal, c. 1900-present") is open-ended and
 * never needs updating. Periods are conventions and their boundaries are
 * flexible, so two HistoricalPeriods may overlap; nothing here prevents or
 * resolves that -- which periods a year falls in is the question
 * historical_period_contains_year answers one period at a time.
 *
 * Dates are approximate by nature ("c."); this type records the
 * conventional boundary, not a claim of precision.
 *
 * Value type: immutable, value equality on (name, start year, end year,
 * open-endedness).
 */

typedef struct HistoricalPeriod HistoricalPeriod;

typedef enum {
    HISTORICAL_PERIOD_OK = 0,
    HISTORICAL_PERIOD_ERR_NULL_NAME,
    HISTORICAL_PERIOD_ERR_END_BEFORE_START,
    HISTORICAL_PERIOD_ERR_ALLOC
} HistoricalPeriodResult;

/* name is copied, required, non-empty. */
HistoricalPeriodResult historical_period_create(const char *name, long start_year, long end_year,
                                                HistoricalPeriod **out);
/* An open-ended period: no end year. */
HistoricalPeriodResult historical_period_create_open_ended(const char *name, long start_year,
                                                           HistoricalPeriod **out);
void historical_period_destroy(HistoricalPeriod *period);

const char *historical_period_name(const HistoricalPeriod *period);
long historical_period_start_year(const HistoricalPeriod *period);
bool historical_period_is_open_ended(const HistoricalPeriod *period);
/* Meaningful only when the period is not open-ended; 0 otherwise. */
long historical_period_end_year(const HistoricalPeriod *period);

/* True when start <= year, and year <= end unless open-ended. */
bool historical_period_contains_year(const HistoricalPeriod *period, long year);

bool historical_period_equals(const HistoricalPeriod *a, const HistoricalPeriod *b);

#endif
