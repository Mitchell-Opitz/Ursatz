#include "semantics/continuation_trait.h"

#include <stddef.h>

static const char *const NAMES[CONTINUATION_TRAIT_COUNT] = {"fragmentation", "increased-rhythmic-activity", "sequence", "increased-harmonic-rhythm"};

bool continuation_trait_valid(ContinuationTrait value) {
    return value >= 0 && value < CONTINUATION_TRAIT_COUNT;
}

const char *continuation_trait_name(ContinuationTrait value) {
    return continuation_trait_valid(value) ? NAMES[value] : NULL;
}
