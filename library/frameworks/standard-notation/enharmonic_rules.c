#include "enharmonic_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "semitone.h"

static const char *const NAMES[STANDARD_NOTATION_ENHARMONIC_RULE_COUNT] = {"enharmonic-equivalence"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_ENHARMONIC_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_ENHARMONIC_RULE_COUNT];

static bool valid_rule(StandardNotationEnharmonicRule which) {
    return which >= 0 && which < STANDARD_NOTATION_ENHARMONIC_RULE_COUNT;
}

static void ensure_built(StandardNotationEnharmonicRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_enharmonic_rule(StandardNotationEnharmonicRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_enharmonic_constraint(StandardNotationEnharmonicRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
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
