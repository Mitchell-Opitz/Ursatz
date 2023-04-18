#include "time/metrical_position.h"

#include <stdlib.h>

struct MetricalPosition {
    int measure;
    Duration *offset;
};

MetricalPositionResult metrical_position_create(int measure, const Duration *offset, MetricalPosition **out) {
    if (offset == NULL || out == NULL) {
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }

    MetricalPosition *position = (MetricalPosition *)malloc(sizeof(MetricalPosition));
    if (position == NULL) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    if (duration_create(duration_numerator(offset), duration_denominator(offset), &position->offset) != DURATION_OK) {
        free(position);
        return METRICAL_POSITION_ERR_ALLOC;
    }
    position->measure = measure;

    *out = position;
    return METRICAL_POSITION_OK;
}

void metrical_position_destroy(MetricalPosition *position) {
    if (position == NULL) {
        return;
    }
    duration_destroy(position->offset);
    free(position);
}

int metrical_position_measure(const MetricalPosition *position) {
    return position->measure;
}

const Duration *metrical_position_offset(const MetricalPosition *position) {
    return position->offset;
}

bool metrical_position_equals(const MetricalPosition *a, const MetricalPosition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->measure == b->measure && duration_equals(a->offset, b->offset);
}
