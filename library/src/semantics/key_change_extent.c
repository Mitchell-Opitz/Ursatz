#include "semantics/key_change_extent.h"

#include <stddef.h>

static const char *const NAMES[KEY_CHANGE_EXTENT_COUNT] = {"tonicization", "extended-tonicization", "weak-modulation",
                                                           "modulation"};

bool key_change_extent_valid(KeyChangeExtent extent) {
    return extent >= 0 && extent < KEY_CHANGE_EXTENT_COUNT;
}

const char *key_change_extent_name(KeyChangeExtent extent) {
    return key_change_extent_valid(extent) ? NAMES[extent] : NULL;
}

bool key_change_extent_is_modulation(KeyChangeExtent extent) {
    return extent == KEY_CHANGE_EXTENT_WEAK_MODULATION || extent == KEY_CHANGE_EXTENT_MODULATION;
}
