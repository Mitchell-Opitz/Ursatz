#include "six_four_type.h"

#include <stddef.h>

static const char *const NAMES[SIX_FOUR_TYPE_COUNT] = {"cadential", "passing", "neighbor", "arpeggiating"};

bool six_four_type_valid(SixFourType type) {
    return type >= 0 && type < SIX_FOUR_TYPE_COUNT;
}

const char *six_four_type_name(SixFourType type) {
    return six_four_type_valid(type) ? NAMES[type] : NULL;
}

