#include "time/historical_period.h"

#include <stdlib.h>
#include <string.h>

struct HistoricalPeriod {
    char *name;
    long start_year;
    bool open_ended;
    long end_year;
};

static HistoricalPeriodResult build(const char *name, long start_year, bool open_ended, long end_year,
                                    HistoricalPeriod **out) {
    if (name == NULL || name[0] == '\0') {
        return HISTORICAL_PERIOD_ERR_NULL_NAME;
    }
    if (!open_ended && end_year < start_year) {
        return HISTORICAL_PERIOD_ERR_END_BEFORE_START;
    }

    HistoricalPeriod *period = (HistoricalPeriod *)malloc(sizeof(HistoricalPeriod));
    if (period == NULL) {
        return HISTORICAL_PERIOD_ERR_ALLOC;
    }

    size_t len = strlen(name);
    period->name = (char *)malloc(len + 1);
    if (period->name == NULL) {
        free(period);
        return HISTORICAL_PERIOD_ERR_ALLOC;
    }
    memcpy(period->name, name, len + 1);

    period->start_year = start_year;
    period->open_ended = open_ended;
    period->end_year = open_ended ? 0 : end_year;

    *out = period;
    return HISTORICAL_PERIOD_OK;
}

HistoricalPeriodResult historical_period_create(const char *name, long start_year, long end_year,
                                                HistoricalPeriod **out) {
    return build(name, start_year, false, end_year, out);
}

HistoricalPeriodResult historical_period_create_open_ended(const char *name, long start_year,
                                                           HistoricalPeriod **out) {
    return build(name, start_year, true, 0, out);
}

void historical_period_destroy(HistoricalPeriod *period) {
    if (period == NULL) {
        return;
    }
    free(period->name);
    free(period);
}

const char *historical_period_name(const HistoricalPeriod *period) {
    return period->name;
}

long historical_period_start_year(const HistoricalPeriod *period) {
    return period->start_year;
}

bool historical_period_is_open_ended(const HistoricalPeriod *period) {
    return period->open_ended;
}

long historical_period_end_year(const HistoricalPeriod *period) {
    return period->end_year;
}

bool historical_period_contains_year(const HistoricalPeriod *period, long year) {
    if (period == NULL || year < period->start_year) {
        return false;
    }
    return period->open_ended || year <= period->end_year;
}

bool historical_period_equals(const HistoricalPeriod *a, const HistoricalPeriod *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return strcmp(a->name, b->name) == 0 && a->start_year == b->start_year && a->open_ended == b->open_ended &&
           a->end_year == b->end_year;
}
