#include "semantics/form_level.h"

bool form_level_valid(FormLevel level) {
    return level >= 0 && level < FORM_LEVEL_COUNT;
}
