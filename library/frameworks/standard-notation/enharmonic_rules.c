#include "enharmonic_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "semitone.h"

static const char *const NAMES[STANDARD_NOTATION_ENHARMONIC_RULE_COUNT] = {"enharmonic-equivalence"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_ENHARMONIC_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_enharmonic, StandardNotationEnharmonicRule, STANDARD_NOTATION_ENHARMONIC_RULE_COUNT, g_table)

bool standard_notation_sounds_the_same(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return standard_notation_semitone_distance(a, b) == 0;
}

bool standard_notation_enharmonic_equivalent(const PitchSpelling *a, const PitchSpelling *b) {
    return standard_notation_sounds_the_same(a, b) && !pitch_spelling_equals(a, b);
}
