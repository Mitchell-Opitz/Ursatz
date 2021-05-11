#include "voicing_rules.h"

#include <stdbool.h>
#include <stddef.h>

#include "construction_rules.h"
#include "crossing_rules.h"
#include "doubling_rules.h"
#include "range_rules.h"
#include "rule_pair.h"
#include "spacing_rules.h"

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

bool standard_voicing_check(const StandardVoicingInput *input, unsigned *violations) {
    if (input == NULL || violations == NULL || input->chord == NULL) {
        return false;
    }
    StandardVoicingConstruction construction =
        standard_voicing_check_construction(input->chord, input->tones, input->tone_count, input->inversion);
    if (construction == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT) {
        return false;
    }

    unsigned mask = 0;
    if (input->stem_directions != NULL && !standard_voicing_stems_valid(input->stem_directions)) {
        mask |= 1u << STANDARD_VOICING_RULE_STEM_DIRECTION;
    }
    if (construction != STANDARD_VOICING_CONSTRUCTION_OK) {
        mask |= 1u << STANDARD_VOICING_RULE_CHORD_CONSTRUCTION;
    }
    if (!standard_voicing_chord_in_range(input->chord)) {
        mask |= 1u << STANDARD_VOICING_RULE_RANGE;
    }
    if (!standard_voicing_chord_spacing_valid(input->chord)) {
        mask |= 1u << STANDARD_VOICING_RULE_SPACING;
    }
    if (!standard_voicing_chord_uncrossed(input->chord)) {
        mask |= 1u << STANDARD_VOICING_RULE_VOICE_CROSSING;
    }
    if (!standard_voicing_doubling_valid(input->chord, input->tones, input->tone_count, input->leading_tone)) {
        mask |= 1u << STANDARD_VOICING_RULE_DOUBLING;
    }

    *violations = mask;
    return true;
}

bool standard_voicing_valid(const StandardVoicingInput *input) {
    unsigned violations;
    return standard_voicing_check(input, &violations) && violations == 0;
}
