#include "periods.h"

#include <stdbool.h>
#include <string.h>

#include "core/once.h"
#include "rule_pair.h"

static const char *const NAMES[WESTERN_PERIODS_RULE_COUNT] = {"period-boundaries", "period-overlap"};

static WesternPeriodsRulePair g_pairs[WESTERN_PERIODS_RULE_COUNT];
static bool g_built[WESTERN_PERIODS_RULE_COUNT];

static bool valid_rule(WesternPeriodsRule which) {
    return which >= 0 && which < WESTERN_PERIODS_RULE_COUNT;
}

static void ensure_built(WesternPeriodsRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    western_periods_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *western_periods_rule(WesternPeriodsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *western_periods_constraint(WesternPeriodsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

typedef struct {
    const char *name;
    long start_year;
    long end_year; /* ignored when open_ended */
    bool open_ended;
} PeriodSpec;

static const PeriodSpec SPECS[WESTERN_PERIODS_COUNT] = {
    {"Medieval", 600, 1400, false},   {"Renaissance", 1400, 1600, false}, {"Baroque", 1600, 1750, false},
    {"Classical", 1750, 1820, false}, {"Romantic", 1800, 1910, false},    {"Post-tonal", 1900, 0, true},
};

static HistoricalPeriod *g_periods[WESTERN_PERIODS_COUNT];
static UrsatzOnce g_once = URSATZ_ONCE_INIT;

static void create_periods(void *arg) {
    (void)arg;
    for (size_t i = 0; i < WESTERN_PERIODS_COUNT; i++) {
        if (SPECS[i].open_ended) {
            historical_period_create_open_ended(SPECS[i].name, SPECS[i].start_year, &g_periods[i]);
        } else {
            historical_period_create(SPECS[i].name, SPECS[i].start_year, SPECS[i].end_year, &g_periods[i]);
        }
    }
}

const HistoricalPeriod *western_periods_at(size_t index) {
    ursatz_once(&g_once, create_periods, NULL);
    if (index >= WESTERN_PERIODS_COUNT) {
        return NULL;
    }
    return g_periods[index];
}

const HistoricalPeriod *western_periods_find(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    ursatz_once(&g_once, create_periods, NULL);
    for (size_t i = 0; i < WESTERN_PERIODS_COUNT; i++) {
        if (g_periods[i] != NULL && strcmp(historical_period_name(g_periods[i]), name) == 0) {
            return g_periods[i];
        }
    }
    return NULL;
}

size_t western_periods_containing_year(long year, const HistoricalPeriod **out, size_t capacity) {
    ursatz_once(&g_once, create_periods, NULL);
    size_t found = 0;
    for (size_t i = 0; i < WESTERN_PERIODS_COUNT; i++) {
        if (g_periods[i] != NULL && historical_period_contains_year(g_periods[i], year)) {
            if (out != NULL && found < capacity) {
                out[found] = g_periods[i];
            }
            found++;
        }
    }
    return found;
}
