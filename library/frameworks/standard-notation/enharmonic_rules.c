#include "enharmonic_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "semitone.h"

static const char *const NAMES[STANDARD_NOTATION_ENHARMONIC_RULE_COUNT] = {"enharmonic-equivalence"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_ENHARMONIC_RULE_COUNT);

static bool valid_rule(StandardNotationEnharmonicRule which) {
    return which >= 0 && which < STANDARD_NOTATION_ENHARMONIC_RULE_COUNT;
}

const Rule *standard_notation_enharmonic_rule(StandardNotationEnharmonicRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_enharmonic_constraint(StandardNotationEnharmonicRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_notation_sounds_the_same(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return standard_notation_semitone_distance(a, b) == 0;
}

bool standard_notation_enharmonic_equivalent(const PitchSpelling *a, const PitchSpelling *b) {
    return standard_notation_sounds_the_same(a, b) && !pitch_spelling_equals(a, b);
}
