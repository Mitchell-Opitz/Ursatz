#include "consonance_rules.h"

#include <stddef.h>

#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT] = {
    "consonance-perfect", "consonance-imperfect", "consonance-dissonance", "consonance-perfect-fourth",
    "consonance-melodic"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    NULL, NAMES, STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT, NULL, g_pairs, g_built};

const Rule *standard_counterpoint_consonance_rule(StandardCounterpointConsonanceRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_consonance_constraint(StandardCounterpointConsonanceRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

bool standard_counterpoint_classify_interval(const StandardCounterpointInterval *interval,
                                             StandardCounterpointIntervalCategory *out) {
    if (interval == NULL || out == NULL) {
        return false;
    }
    StandardCounterpointIntervalCategory category = STANDARD_COUNTERPOINT_DISSONANCE;
    bool perfect = interval->quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT;
    bool major_or_minor = interval->quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR ||
                          interval->quality == STANDARD_COUNTERPOINT_QUALITY_MINOR;
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
                                            StandardCounterpointIntervalCategory *out) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(a, b, &interval)) {
        return false;
    }
    return standard_counterpoint_classify_interval(&interval, out);
}

bool standard_counterpoint_pitches_consonant(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_classify_pitches(a, b, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

bool standard_counterpoint_interval_melodic_consonant(const StandardCounterpointInterval *interval) {
    if (interval == NULL) {
        return false;
    }
    switch (interval->quality) {
        case STANDARD_COUNTERPOINT_QUALITY_PERFECT:
            return true;
        case STANDARD_COUNTERPOINT_QUALITY_MAJOR:
        case STANDARD_COUNTERPOINT_QUALITY_MINOR:
            return interval->simple_size != 7;
        default:
            return false;
    }
}
