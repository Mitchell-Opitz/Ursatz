#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/key_change_extent.h"

static void test_validity(void) {
    for (int i = 0; i < KEY_CHANGE_EXTENT_COUNT; i++) {
        assert(key_change_extent_valid((KeyChangeExtent)i));
    }
    assert(!key_change_extent_valid(KEY_CHANGE_EXTENT_COUNT));
    assert(!key_change_extent_valid((KeyChangeExtent)-1));
}

static void test_names(void) {
    assert(strcmp(key_change_extent_name(KEY_CHANGE_EXTENT_TONICIZATION), "tonicization") == 0);
    assert(strcmp(key_change_extent_name(KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION), "extended-tonicization") == 0);
    assert(strcmp(key_change_extent_name(KEY_CHANGE_EXTENT_WEAK_MODULATION), "weak-modulation") == 0);
    assert(strcmp(key_change_extent_name(KEY_CHANGE_EXTENT_MODULATION), "modulation") == 0);
    assert(key_change_extent_name(KEY_CHANGE_EXTENT_COUNT) == NULL);
}

static void test_is_modulation(void) {
    assert(!key_change_extent_is_modulation(KEY_CHANGE_EXTENT_TONICIZATION));
    assert(!key_change_extent_is_modulation(KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION));
    assert(key_change_extent_is_modulation(KEY_CHANGE_EXTENT_WEAK_MODULATION));
    assert(key_change_extent_is_modulation(KEY_CHANGE_EXTENT_MODULATION));
    assert(!key_change_extent_is_modulation(KEY_CHANGE_EXTENT_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_is_modulation();
    printf("test_key_change_extent: ok\n");
    return 0;
}
