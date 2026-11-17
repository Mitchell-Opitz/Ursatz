#include "theory/form/phrase_rules.h"

bool standard_form_cadence_is_phrase_goal(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
           type == CADENCE_TYPE_HALF;
}

