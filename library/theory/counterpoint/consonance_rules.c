#include "theory/counterpoint/consonance_rules.h"

#include <stddef.h>

#include "theory/rules/rule_table.h"

bool standard_counterpoint_classify_interval(const StandardTonalityInterval *interval,
                                             StandardTonalityIntervalCategory *out) {
    if (interval == NULL || out == NULL) {
        return false;
    }
    StandardTonalityIntervalCategory category = STANDARD_COUNTERPOINT_DISSONANCE;
    bool perfect = interval->quality == STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT;
    bool major_or_minor = interval->quality == STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR ||
                          interval->quality == STANDARD_TONALITY_INTERVAL_QUALITY_MINOR;
    int simple = interval->simple_size;

    if (perfect && (simple == 1 || simple == 5 || simple == 8)) {
        category = STANDARD_COUNTERPOINT_PERFECT_CONSONANCE;
    } else if (major_or_minor && (simple == 3 || simple == 6)) {
        category = STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE;
    }
    *out = category;
    return true;
}

bool standard_counterpoint_classify_pitches(const PitchSpelling *a, const PitchSpelling *b,
                                            StandardTonalityIntervalCategory *out) {
    StandardTonalityInterval interval;
    if (!standard_tonality_interval_between(a, b, &interval)) {
        return false;
    }
    return standard_counterpoint_classify_interval(&interval, out);
}

bool standard_counterpoint_pitches_consonant(const PitchSpelling *a, const PitchSpelling *b) {
    StandardTonalityIntervalCategory category;
    return standard_counterpoint_classify_pitches(a, b, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

bool standard_counterpoint_interval_melodic_consonant(const StandardTonalityInterval *interval) {
    if (interval == NULL) {
        return false;
    }
    switch (interval->quality) {
        case STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT:
            return true;
        case STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR:
        case STANDARD_TONALITY_INTERVAL_QUALITY_MINOR:
            return interval->simple_size != 7;
        default:
            return false;
    }
}
