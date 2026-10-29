#include "time/musical_time.h"

#include <limits.h>
#include <stdlib.h>
#include "core/rational.h"
#include "core/value.h"

struct MusicalTime {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} MusicalTimeContent;

static unsigned long abs_ul(long n) {
    return n < 0 ? (unsigned long)(-n) : (unsigned long)n;
}

MusicalTimeResult musical_time_create(long numerator, unsigned long denominator, MusicalTime **out) {
    if (denominator == 0) {
        return MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR;
    }

    unsigned long divisor = (unsigned long)rational_gcd_unsigned(numerator < 0 ? 0UL - (unsigned long)numerator : (unsigned long)numerator, denominator);
    if (divisor == 0) {
        divisor = 1;
    }

    MusicalTimeContent content;
    content.numerator = numerator / (long)divisor;
    content.denominator = denominator / divisor;

    MusicalTime *t = (MusicalTime *)malloc(sizeof(MusicalTime));
    if (t == NULL) {
        return MUSICAL_TIME_ERR_ALLOC;
    }

    if (value_create("MusicalTime", &content, sizeof(content), &t->value) != VALUE_OK) {
        free(t);
        return MUSICAL_TIME_ERR_ALLOC;
    }

    *out = t;
    return MUSICAL_TIME_OK;
}

void musical_time_destroy(MusicalTime *time) {
    if (time == NULL) {
        return;
    }
    value_destroy(time->value);
    free(time);
}

bool musical_time_equals(const MusicalTime *a, const MusicalTime *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

static const MusicalTimeContent *musical_time_content(const MusicalTime *time) {
    return (const MusicalTimeContent *)value_content(time->value, NULL);
}

long musical_time_numerator(const MusicalTime *time) {
    return musical_time_content(time)->numerator;
}

unsigned long musical_time_denominator(const MusicalTime *time) {
    return musical_time_content(time)->denominator;
}

static bool rational_of_content(const MusicalTimeContent *content, Rational *out) {
    return content->denominator <= (unsigned long)LLONG_MAX &&
           rational_make(content->numerator, (long long)content->denominator, out) == RATIONAL_OK;
}

static bool rational_of_duration_parts(const Duration *duration, Rational *out) {
    unsigned long denominator = duration_denominator(duration);
    return denominator <= (unsigned long)LLONG_MAX &&
           rational_make(duration_numerator(duration), (long long)denominator, out) == RATIONAL_OK;
}

MusicalTimeResult musical_time_add(const MusicalTime *time, const Duration *duration, MusicalTime **out) {
    Rational a;
    Rational b;
    Rational sum;
    if (!rational_of_content(musical_time_content(time), &a) || !rational_of_duration_parts(duration, &b) ||
        rational_add(a, b, &sum) != RATIONAL_OK || sum.num < LONG_MIN || sum.num > LONG_MAX ||
        (unsigned long long)sum.den > ULONG_MAX) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    return musical_time_create((long)sum.num, (unsigned long)sum.den, out);
}

DurationResult musical_time_subtract(const MusicalTime *a, const MusicalTime *b, Duration **out) {
    Rational left;
    Rational right;
    Rational difference;
    if (!rational_of_content(musical_time_content(a), &left) ||
        !rational_of_content(musical_time_content(b), &right) ||
        rational_sub(left, right, &difference) != RATIONAL_OK || difference.num > LONG_MAX ||
        (unsigned long long)difference.den > ULONG_MAX) {
        return DURATION_ERR_OVERFLOW;
    }
    if (difference.num < 0) {
        return DURATION_ERR_NEGATIVE;
    }
    return duration_create((long)difference.num, (unsigned long)difference.den, out);
}
