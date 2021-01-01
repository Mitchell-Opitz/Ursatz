#include "interval_size_rules.h"

#include <stddef.h>

#include "pitch/generic_interval.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT] = {
    "interval-size-counting", "interval-size-names", "interval-size-generic", "interval-size-compound"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT];

static bool valid_rule(StandardTonalityIntervalSizeRule which) {
    return which >= 0 && which < STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT;
}

static void ensure_built(StandardTonalityIntervalSizeRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_interval_size_rule(StandardTonalityIntervalSizeRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_interval_size_constraint(StandardTonalityIntervalSizeRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static const char *const SIZE_NAMES[13] = {"unison", "second", "third",  "fourth",   "fifth",
                                           "sixth",  "seventh", "octave", "ninth",    "tenth",
                                           "eleventh", "twelfth", "thirteenth"};

const char *standard_tonality_interval_size_name(int size) {
    if (size < 1 || size > 13) {
        return NULL;
    }
    return SIZE_NAMES[size - 1];
}

bool standard_tonality_interval_generic_size(const PitchSpelling *lower, const PitchSpelling *upper, int *size) {
    if (size == NULL) {
        return false;
    }
    GenericInterval *interval;
    if (generic_interval_between(lower, upper, &interval) != GENERIC_INTERVAL_OK) {
        return false;
    }
    *size = generic_interval_size(interval);
    generic_interval_destroy(interval);
    return true;
}

bool standard_tonality_interval_is_simple(int size) {
    return size >= 1 && size <= STANDARD_TONALITY_INTERVAL_OCTAVE;
}

bool standard_tonality_interval_is_compound(int size) {
    return size > STANDARD_TONALITY_INTERVAL_OCTAVE;
}

int standard_tonality_interval_simple_size(int size) {
    if (size < 1) {
        return 0;
    }
    while (size > STANDARD_TONALITY_INTERVAL_OCTAVE) {
        size -= 7;
    }
    return size;
}

int standard_tonality_interval_compound_size(int size) {
    if (!standard_tonality_interval_is_simple(size)) {
        return 0;
    }
    return size + 7;
}
