#include "voicing_rules.h"

#include <stdbool.h>
#include <stddef.h>

#include "construction_rules.h"
#include "crossing_rules.h"
#include "doubling_rules.h"
#include "range_rules.h"
#include "spacing_rules.h"

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

