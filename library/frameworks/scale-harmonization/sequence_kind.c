#include "sequence_kind.h"

#include <stddef.h>

static const char *const NAMES[SEQUENCE_KIND_COUNT] = {"parallel-chords", "five-six", "seven-six",
                                                       "cycle-of-fifths", "two-three"};

bool sequence_kind_valid(SequenceKind kind) {
    return kind >= 0 && kind < SEQUENCE_KIND_COUNT;
}

const char *sequence_kind_name(SequenceKind kind) {
    return sequence_kind_valid(kind) ? NAMES[kind] : NULL;
}

bool sequence_kind_is_suspension_chain(SequenceKind kind) {
    return kind == SEQUENCE_KIND_SEVEN_SIX || kind == SEQUENCE_KIND_TWO_THREE;
}
