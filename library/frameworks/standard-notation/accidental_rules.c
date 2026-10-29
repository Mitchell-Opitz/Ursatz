#include "accidental_rules.h"

#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT] = {
    "accidental-sharp-raises", "accidental-flat-lowers", "accidental-natural-cancels",
    "accidental-double-whole-step"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT);

static bool valid_rule(StandardNotationAccidentalRule which) {
    return which >= 0 && which < STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT;
}

const Rule *standard_notation_accidental_rule(StandardNotationAccidentalRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_accidental_constraint(StandardNotationAccidentalRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

int standard_notation_accidental_half_steps(const Accidental *accidental) {
    if (accidental == NULL) {
        return 0;
    }
    return accidental_semitone_offset(accidental);
}

bool standard_notation_accidental_raises(const Accidental *accidental) {
    return accidental != NULL && accidental_semitone_offset(accidental) > 0;
}

bool standard_notation_accidental_lowers(const Accidental *accidental) {
    return accidental != NULL && accidental_semitone_offset(accidental) < 0;
}

bool standard_notation_accidental_cancels(const Accidental *accidental) {
    return accidental != NULL && accidental_kind(accidental) == ACCIDENTAL_NATURAL;
}

int standard_notation_effective_offset(const Accidental *written, int prior_offset) {
    if (written == NULL) {
        return prior_offset;
    }
    return accidental_semitone_offset(written);
}

bool standard_notation_spelling_accidental_valid(const PitchSpelling *spelling) {
    if (spelling == NULL) {
        return false;
    }
    int offset = pitch_spelling_accidental(spelling);
    return offset >= ACCIDENTAL_MIN_OFFSET && offset <= ACCIDENTAL_MAX_OFFSET;
}

bool standard_notation_spelling_matches_accidental(const PitchSpelling *spelling, const Accidental *accidental) {
    if (spelling == NULL || accidental == NULL) {
        return false;
    }
    return pitch_spelling_accidental(spelling) == accidental_semitone_offset(accidental);
}
