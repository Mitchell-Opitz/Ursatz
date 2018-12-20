#include "time/time_instant.h"

#include <stdlib.h>

struct TimeInstant {
    MusicalTime *time;
};

TimeInstantResult time_instant_create(long numerator, unsigned long denominator, TimeInstant **out) {
    if (denominator == 0) {
        return TIME_INSTANT_ERR_NON_POSITIVE_DENOMINATOR;
    }

    TimeInstant *instant = (TimeInstant *)malloc(sizeof(TimeInstant));
    if (instant == NULL) {
        return TIME_INSTANT_ERR_ALLOC;
    }

    if (musical_time_create(numerator, denominator, &instant->time) != MUSICAL_TIME_OK) {
        free(instant);
        return TIME_INSTANT_ERR_ALLOC;
    }

    *out = instant;
    return TIME_INSTANT_OK;
}

void time_instant_destroy(TimeInstant *instant) {
    if (instant == NULL) {
        return;
    }
    musical_time_destroy(instant->time);
    free(instant);
}

bool time_instant_equals(const TimeInstant *a, const TimeInstant *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return musical_time_equals(a->time, b->time);
}

long time_instant_numerator(const TimeInstant *instant) {
    return musical_time_numerator(instant->time);
}

unsigned long time_instant_denominator(const TimeInstant *instant) {
    return musical_time_denominator(instant->time);
}

const MusicalTime *time_instant_time(const TimeInstant *instant) {
    return instant->time;
}