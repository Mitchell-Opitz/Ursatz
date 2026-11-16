#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "six_four_type.h"

static void test_validity(void) {
    for (int i = 0; i < SIX_FOUR_TYPE_COUNT; i++) {
        assert(six_four_type_valid((SixFourType)i));
    }
    assert(!six_four_type_valid(SIX_FOUR_TYPE_COUNT));
    assert(!six_four_type_valid((SixFourType)-1));
}

static void test_names(void) {
    assert(strcmp(six_four_type_name(SIX_FOUR_TYPE_CADENTIAL), "cadential") == 0);
    assert(strcmp(six_four_type_name(SIX_FOUR_TYPE_PASSING), "passing") == 0);
    assert(strcmp(six_four_type_name(SIX_FOUR_TYPE_NEIGHBOR), "neighbor") == 0);
    assert(strcmp(six_four_type_name(SIX_FOUR_TYPE_ARPEGGIATING), "arpeggiating") == 0);
    assert(six_four_type_name(SIX_FOUR_TYPE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_six_four_type: ok\n");
    return 0;
}
