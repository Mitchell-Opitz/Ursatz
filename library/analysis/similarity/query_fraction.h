#ifndef MUSIC_QUERY_QUERY_FRACTION_H
#define MUSIC_QUERY_QUERY_FRACTION_H

/*
 * Internal to query/*.c: exact-rational arithmetic for similarity scores and
 * weights (numerator/denominator over unsigned long, never float/double --
 * Law 19). Results are reduced to lowest terms with a positive denominator.
 * Nothing wraps around: an operation whose exact result does not fit in
 * unsigned long returns QUERY_FRACTION_ERR_OVERFLOW and leaves its output
 * untouched; a fraction with a zero denominator is
 * QUERY_FRACTION_ERR_ZERO_DENOMINATOR.
 *
 * query/ keeps its own small copy, as theory/ does (theory/style_fraction.h):
 * it may not depend on rules/ or theory/.
 */

typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} QueryFraction;

typedef enum {
    QUERY_FRACTION_OK = 0,
    QUERY_FRACTION_ERR_ZERO_DENOMINATOR,
    QUERY_FRACTION_ERR_OVERFLOW
} QueryFractionResult;

/* Lowest terms: 2/4 -> 1/2, 0/5 -> 0/1. Needs a non-zero denominator. */
QueryFractionResult query_fraction_reduce(QueryFraction fraction, QueryFraction *out);
QueryFractionResult query_fraction_add(QueryFraction a, QueryFraction b, QueryFraction *out);
QueryFractionResult query_fraction_multiply(QueryFraction a, QueryFraction b, QueryFraction *out);

/*
 * Orders two fractions exactly: -1, 0 or 1 as a is less than, equal to or
 * greater than b. Never overflows (continued-fraction comparison). Both need
 * a non-zero denominator.
 */
QueryFractionResult query_fraction_compare(QueryFraction a, QueryFraction b, int *out_comparison);

#endif
