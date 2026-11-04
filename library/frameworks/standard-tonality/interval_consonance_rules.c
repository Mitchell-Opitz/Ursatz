#include "interval_consonance_rules.h"

#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT] = {"interval-consonance-melodic",
                                                                                     "interval-consonance-harmonic"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_interval_consonance, StandardTonalityIntervalConsonanceRule, STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT, g_table)

static bool valid_type(StandardTonalityIntervalType type) {
    return type == STANDARD_TONALITY_INTERVAL_MELODIC || type == STANDARD_TONALITY_INTERVAL_HARMONIC;
}

bool standard_tonality_interval_is_consonant(StandardTonalityIntervalType type, int size,
                                             StandardTonalityIntervalQuality quality) {
    if (!valid_type(type) || !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }
    if (standard_tonality_interval_quality_is_augmented(quality) ||
        standard_tonality_interval_quality_is_diminished(quality)) {
        return false;
    }

    int simple = standard_tonality_interval_simple_size(size);
    if (type == STANDARD_TONALITY_INTERVAL_MELODIC) {
        /* Perfect intervals, and major and minor 2nds, 3rds, 6ths. Only
           the 7ths are left out. */
        return simple != 7;
    }
    /* Harmonic: major and minor 3rds and 6ths, and P1, P5, P8. Not 2nds,
       7ths, or the perfect 4th. */
    return simple == 3 || simple == 6 || simple == 1 || simple == 5 || simple == 8;
}

bool standard_tonality_interval_is_dissonant(StandardTonalityIntervalType type, int size,
                                             StandardTonalityIntervalQuality quality) {
    if (!valid_type(type) || !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }
    return !standard_tonality_interval_is_consonant(type, size, quality);
}
