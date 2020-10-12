#include "time/tuplet.h"

#include <stdlib.h>

struct Tuplet {
    unsigned actual;
    unsigned normal;
};

TupletResult tuplet_create(unsigned actual, unsigned normal, Tuplet **out) {
    if (actual < TUPLET_MIN_ACTUAL || actual > TUPLET_MAX_COUNT) {
        return TUPLET_ERR_ACTUAL_OUT_OF_RANGE;
    }
    if (normal < 1 || normal > TUPLET_MAX_COUNT) {
        return TUPLET_ERR_NORMAL_OUT_OF_RANGE;
    }
    if (actual == normal) {
        return TUPLET_ERR_EQUAL_COUNTS;
    }

    Tuplet *tuplet = (Tuplet *)malloc(sizeof(Tuplet));
    if (tuplet == NULL) {
        return TUPLET_ERR_ALLOC;
    }
    tuplet->actual = actual;
    tuplet->normal = normal;

    *out = tuplet;
    return TUPLET_OK;
}

void tuplet_destroy(Tuplet *tuplet) {
    free(tuplet);
}

unsigned tuplet_actual(const Tuplet *tuplet) {
    return tuplet->actual;
}

unsigned tuplet_normal(const Tuplet *tuplet) {
    return tuplet->normal;
}

DurationResult tuplet_note_duration(const Tuplet *tuplet, const Duration *nominal, Duration **out) {
    return duration_create(duration_numerator(nominal) * (long)tuplet->normal,
                           duration_denominator(nominal) * tuplet->actual, out);
}

DurationResult tuplet_span(const Tuplet *tuplet, const Duration *nominal, Duration **out) {
    return duration_create(duration_numerator(nominal) * (long)tuplet->normal, duration_denominator(nominal), out);
}

bool tuplet_equals(const Tuplet *a, const Tuplet *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->actual == b->actual && a->normal == b->normal;
}
