#include "semantics/motive_occurrence_kind.h"

#include <stddef.h>

static const char *const NAMES[MOTIVE_OCCURRENCE_KIND_COUNT] = {
    "exact", "real-transposed", "tonal-transposed", "inverted", "retrograde", "augmented", "diminished", "fragmented", "varied"};

bool motive_occurrence_kind_valid(MotiveOccurrenceKind kind) {
    return (int)kind >= 0 && (int)kind < MOTIVE_OCCURRENCE_KIND_COUNT;
}

const char *motive_occurrence_kind_name(MotiveOccurrenceKind kind) {
    return motive_occurrence_kind_valid(kind) ? NAMES[kind] : NULL;
}

int motive_occurrence_kind_rank(MotiveOccurrenceKind kind) {
    switch (kind) {
        case MOTIVE_OCCURRENCE_EXACT:
            return 0;
        case MOTIVE_OCCURRENCE_REAL:
        case MOTIVE_OCCURRENCE_TONAL:
            return 1;
        case MOTIVE_OCCURRENCE_INVERTED:
        case MOTIVE_OCCURRENCE_RETROGRADE:
            return 2;
        case MOTIVE_OCCURRENCE_AUGMENTED:
        case MOTIVE_OCCURRENCE_DIMINISHED:
            return 3;
        case MOTIVE_OCCURRENCE_FRAGMENTED:
            return 4;
        case MOTIVE_OCCURRENCE_VARIED:
            return 5;
        default:
            return -1;
    }
}
