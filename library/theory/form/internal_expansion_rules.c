#include "theory/form/internal_expansion_rules.h"

#include <stddef.h>

unsigned standard_form_internal_technique_bit(InternalExpansionTechnique technique) {
    return internal_expansion_technique_valid(technique) ? 1u << (unsigned)technique : 0u;
}

bool standard_form_repetition_creates_expansion(bool unexpected, bool is_presentation_basic_idea_repeat) {
    return unexpected && !is_presentation_basic_idea_repeat;
}

bool standard_form_is_one_more_time(bool tried, bool evaded, bool retried) {
    return tried && evaded && retried;
}

