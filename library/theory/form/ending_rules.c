#include "theory/form/ending_rules.h"

static bool is_authentic(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
}

bool standard_form_classify_ending(CadenceType cadence, bool bass_is_cadential_pattern,
                                   bool basic_then_contrasting_ideas, bool has_fragmentation,
                                   PhraseEnding *out) {
    if (!standard_form_cadence_is_phrase_goal(cadence)) {
        return false;
    }
    if (bass_is_cadential_pattern && is_authentic(cadence) && !has_fragmentation) {
        *out = PHRASE_ENDING_CADENTIAL;
    } else if (basic_then_contrasting_ideas && is_authentic(cadence)) {
        *out = PHRASE_ENDING_CONSEQUENT;
    } else {
        *out = PHRASE_ENDING_CONTINUATION;
    }
    return true;
}
