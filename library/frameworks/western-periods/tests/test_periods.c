#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "periods.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < WESTERN_PERIODS_RULE_COUNT; i++) {
        const Rule *rule = western_periods_rule((WesternPeriodsRule)i);
        const Constraint *constraint = western_periods_constraint((WesternPeriodsRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), western_periods_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(western_periods_rule(WESTERN_PERIODS_RULE_COUNT) == NULL);
}

static void expect_period(size_t index, const char *name, long start, long end, bool open_ended) {
    const HistoricalPeriod *period = western_periods_at(index);
    assert(period != NULL);
    assert(strcmp(historical_period_name(period), name) == 0);
    assert(historical_period_start_year(period) == start);
    assert(historical_period_is_open_ended(period) == open_ended);
    if (!open_ended) {
        assert(historical_period_end_year(period) == end);
    }
}

static void test_the_six_periods(void) {
    assert(WESTERN_PERIODS_COUNT == 6);
    expect_period(0, "Medieval", 600, 1400, false);
    expect_period(1, "Renaissance", 1400, 1600, false);
    expect_period(2, "Baroque", 1600, 1750, false);
    expect_period(3, "Classical", 1750, 1820, false);
    expect_period(4, "Romantic", 1800, 1910, false);
    expect_period(5, "Post-tonal", 1900, 0, true);
    assert(western_periods_at(6) == NULL);
}

static void test_find_by_name(void) {
    const HistoricalPeriod *baroque = western_periods_find("Baroque");
    assert(baroque != NULL);
    assert(baroque == western_periods_at(2));
    assert(western_periods_find("Impressionist") == NULL);
    assert(western_periods_find("baroque") == NULL); /* exact match */
    assert(western_periods_find(NULL) == NULL);
}

/* Returns the names of the periods containing year, joined by '/'. */
static void names_for(long year, char *buffer, size_t size, size_t *out_count) {
    const HistoricalPeriod *found[WESTERN_PERIODS_COUNT];
    size_t count = western_periods_containing_year(year, found, WESTERN_PERIODS_COUNT);
    buffer[0] = '\0';
    for (size_t i = 0; i < count && i < WESTERN_PERIODS_COUNT; i++) {
        if (i > 0) {
            strncat(buffer, "/", size - strlen(buffer) - 1);
        }
        strncat(buffer, historical_period_name(found[i]), size - strlen(buffer) - 1);
    }
    *out_count = count;
}

static void expect_year(long year, const char *expected_names, size_t expected_count) {
    char buffer[128];
    size_t count = 0;
    names_for(year, buffer, sizeof(buffer), &count);
    assert(count == expected_count);
    assert(strcmp(buffer, expected_names) == 0);
}

static void test_single_period_years(void) {
    expect_year(500, "", 0);   /* before the Medieval period */
    expect_year(600, "Medieval", 1);
    expect_year(1200, "Medieval", 1);
    expect_year(1500, "Renaissance", 1);
    expect_year(1685, "Baroque", 1);       /* Bach */
    expect_year(1790, "Classical", 1);     /* Mozart, Haydn */
    expect_year(1850, "Romantic", 1);
    expect_year(1950, "Post-tonal", 1);
    expect_year(2026, "Post-tonal", 1);    /* open-ended */
}

/* The boundaries are flexible: overlapping years belong to both periods. */
static void test_overlaps(void) {
    expect_year(1799, "Classical", 1);
    expect_year(1800, "Classical/Romantic", 2);
    expect_year(1810, "Classical/Romantic", 2);
    expect_year(1820, "Classical/Romantic", 2);
    expect_year(1821, "Romantic", 1);

    expect_year(1899, "Romantic", 1);
    expect_year(1900, "Romantic/Post-tonal", 2);
    expect_year(1910, "Romantic/Post-tonal", 2);
    expect_year(1911, "Post-tonal", 1);
}

/* Years shared as an inclusive end and start (1400, 1600, 1750) count for both. */
static void test_shared_boundary_years(void) {
    expect_year(1400, "Medieval/Renaissance", 2);
    expect_year(1600, "Renaissance/Baroque", 2);
    expect_year(1750, "Baroque/Classical", 2);
}

/* No year is ever in more than the stated maximum. */
static void test_max_overlap(void) {
    for (long year = 500; year <= 2100; year++) {
        const HistoricalPeriod *found[WESTERN_PERIODS_COUNT];
        size_t count = western_periods_containing_year(year, found, WESTERN_PERIODS_COUNT);
        assert(count <= WESTERN_PERIODS_MAX_OVERLAP);
        if (year >= 600) {
            assert(count >= 1); /* continuous coverage from 600 on */
        }
    }
}

/* The count can exceed the capacity; only the first `capacity` are written. */
static void test_capacity(void) {
    const HistoricalPeriod *found[1] = {NULL};
    size_t count = western_periods_containing_year(1800, found, 1);
    assert(count == 2);
    assert(strcmp(historical_period_name(found[0]), "Classical") == 0);
    assert(western_periods_containing_year(1800, NULL, 0) == 2);
}

int main(void) {
    test_rules_reference_framework();
    test_the_six_periods();
    test_find_by_name();
    test_single_period_years();
    test_overlaps();
    test_shared_boundary_years();
    test_max_overlap();
    test_capacity();
    printf("test_periods (western-periods): ok\n");
    return 0;
}
