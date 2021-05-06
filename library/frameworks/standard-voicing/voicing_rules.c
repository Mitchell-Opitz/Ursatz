#include "voicing_rules.h"

#include <stdbool.h>
#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_VOICING_RULE_COUNT] = {"stem-direction", "chord-construction", "range",
                                                                "spacing",        "voice-crossing",     "doubling"};

static StandardVoicingRulePair g_pairs[STANDARD_VOICING_RULE_COUNT];
static bool g_built[STANDARD_VOICING_RULE_COUNT];

static bool valid_rule(StandardVoicingRule which) {
    return which >= 0 && which < STANDARD_VOICING_RULE_COUNT;
}

static void ensure_built(StandardVoicingRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_voicing_build_rule_pair(NAMES[which], CONSTRAINT_TYPE_HARD, &g_pairs[which]);
}

const Rule *standard_voicing_rule(StandardVoicingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_voicing_constraint(StandardVoicingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_voicing_rule_name(StandardVoicingRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}
