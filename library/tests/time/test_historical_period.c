#include "time/historical_period.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_create_rejects_invalid(void) {
    HistoricalPeriod *p = NULL;
    assert(historical_period_create(NULL, 1600, 1750, &p) == HISTORICAL_PERIOD_ERR_NULL_NAME);
    assert(historical_period_create("", 1600, 1750, &p) == HISTORICAL_PERIOD_ERR_NULL_NAME);
    assert(historical_period_create_open_ended(NULL, 1900, &p) == HISTORICAL_PERIOD_ERR_NULL_NAME);
    assert(historical_period_create("Backwards", 1750, 1600, &p) == HISTORICAL_PERIOD_ERR_END_BEFORE_START);
    assert(p == NULL);
}

static void test_accessors(void) {
    HistoricalPeriod *baroque = NULL;
    assert(historical_period_create("Baroque", 1600, 1750, &baroque) == HISTORICAL_PERIOD_OK);
    assert(strcmp(historical_period_name(baroque), "Baroque") == 0);
    assert(historical_period_start_year(baroque) == 1600);
    assert(!historical_period_is_open_ended(baroque));
    assert(historical_period_end_year(baroque) == 1750);
    historical_period_destroy(baroque);

    HistoricalPeriod *post_tonal = NULL;
    assert(historical_period_create_open_ended("Post-tonal", 1900, &post_tonal) == HISTORICAL_PERIOD_OK);
    assert(historical_period_is_open_ended(post_tonal));
    assert(historical_period_end_year(post_tonal) == 0);
    historical_period_destroy(post_tonal);
}

/* Years are inclusive on both ends. */
static void test_contains_year(void) {
    HistoricalPeriod *baroque = NULL;
    assert(historical_period_create("Baroque", 1600, 1750, &baroque) == HISTORICAL_PERIOD_OK);
    assert(!historical_period_contains_year(baroque, 1599));
    assert(historical_period_contains_year(baroque, 1600));
    assert(historical_period_contains_year(baroque, 1685));
    assert(historical_period_contains_year(baroque, 1750));
    assert(!historical_period_contains_year(baroque, 1751));
    assert(!historical_period_contains_year(NULL, 1685));
    historical_period_destroy(baroque);
}

static void test_open_ended_never_ends(void) {
    HistoricalPeriod *post_tonal = NULL;
    assert(historical_period_create_open_ended("Post-tonal", 1900, &post_tonal) == HISTORICAL_PERIOD_OK);
    assert(!historical_period_contains_year(post_tonal, 1899));
    assert(historical_period_contains_year(post_tonal, 1900));
    assert(historical_period_contains_year(post_tonal, 2026));
    assert(historical_period_contains_year(post_tonal, 3000));
    historical_period_destroy(post_tonal);
}

static void test_single_year_period_and_equality(void) {
    HistoricalPeriod *one = NULL, *same = NULL, *different_end = NULL, *open = NULL, *renamed = NULL;
    assert(historical_period_create("Moment", 1800, 1800, &one) == HISTORICAL_PERIOD_OK);
    assert(historical_period_contains_year(one, 1800));
    assert(!historical_period_contains_year(one, 1801));

    assert(historical_period_create("Moment", 1800, 1800, &same) == HISTORICAL_PERIOD_OK);
    assert(historical_period_create("Moment", 1800, 1801, &different_end) == HISTORICAL_PERIOD_OK);
    assert(historical_period_create_open_ended("Moment", 1800, &open) == HISTORICAL_PERIOD_OK);
    assert(historical_period_create("Instant", 1800, 1800, &renamed) == HISTORICAL_PERIOD_OK);

    assert(historical_period_equals(one, same));
    assert(!historical_period_equals(one, different_end));
    assert(!historical_period_equals(one, open));
    assert(!historical_period_equals(one, renamed));
    assert(!historical_period_equals(one, NULL));

    historical_period_destroy(one);
    historical_period_destroy(same);
    historical_period_destroy(different_end);
    historical_period_destroy(open);
    historical_period_destroy(renamed);
}

/* The name is copied: mutating the caller's buffer does not change the period. */
static void test_name_is_copied(void) {
    char name[] = "Romantic";
    HistoricalPeriod *p = NULL;
    assert(historical_period_create(name, 1800, 1910, &p) == HISTORICAL_PERIOD_OK);
    name[0] = 'X';
    assert(strcmp(historical_period_name(p), "Romantic") == 0);
    historical_period_destroy(p);
}

int main(void) {
    test_create_rejects_invalid();
    test_accessors();
    test_contains_year();
    test_open_ended_never_ends();
    test_single_year_period_and_equality();
    test_name_is_copied();
    return 0;
}
