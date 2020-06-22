#include "accidental_rules.h"

#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT] = {
    "accidental-sharp-raises", "accidental-flat-lowers", "accidental-natural-cancels",
    "accidental-double-whole-step"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT];

static bool valid_rule(StandardNotationAccidentalRule which) {
    return which >= 0 && which < STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT;
}

static void ensure_built(StandardNotationAccidentalRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_accidental_rule(StandardNotationAccidentalRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_accidental_constraint(StandardNotationAccidentalRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
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
