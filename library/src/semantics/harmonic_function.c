#include "semantics/harmonic_function.h"

#include <stddef.h>

static const char *const NAMES[HARMONIC_FUNCTION_COUNT] = {"tonic", "predominant", "dominant"};

bool harmonic_function_valid(HarmonicFunction function) {
    return function >= 0 && function < HARMONIC_FUNCTION_COUNT;
}

const char *harmonic_function_name(HarmonicFunction function) {
    return harmonic_function_valid(function) ? NAMES[function] : NULL;
}
