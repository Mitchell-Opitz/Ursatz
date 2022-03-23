#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/ostinato_type.h"

static void test_validity(void) {
    for (int i = 0; i < OSTINATO_TYPE_COUNT; i++) {
        assert(ostinato_type_valid((OstinatoType)i));
    }
    assert(!ostinato_type_valid(OSTINATO_TYPE_COUNT));
    assert(!ostinato_type_valid((OstinatoType)-1));
}

static void test_names(void) {
    assert(strcmp(ostinato_type_name(OSTINATO_TYPE_OSTINATO), "ostinato") == 0);
    assert(strcmp(ostinato_type_name(OSTINATO_TYPE_GROUND_BASS), "ground-bass") == 0);
    assert(strcmp(ostinato_type_name(OSTINATO_TYPE_CHACONNE), "chaconne") == 0);
    assert(strcmp(ostinato_type_name(OSTINATO_TYPE_PASSACAGLIA), "passacaglia") == 0);
    assert(ostinato_type_name(OSTINATO_TYPE_COUNT) == NULL);
}

static void test_is_ground_bass(void) {
    assert(!ostinato_type_is_ground_bass(OSTINATO_TYPE_OSTINATO));
    assert(ostinato_type_is_ground_bass(OSTINATO_TYPE_GROUND_BASS));
    assert(ostinato_type_is_ground_bass(OSTINATO_TYPE_CHACONNE));
    assert(ostinato_type_is_ground_bass(OSTINATO_TYPE_PASSACAGLIA));
    assert(!ostinato_type_is_ground_bass(OSTINATO_TYPE_COUNT));
}

static void test_is_triple_meter_form(void) {
    assert(!ostinato_type_is_triple_meter_form(OSTINATO_TYPE_OSTINATO));
    assert(!ostinato_type_is_triple_meter_form(OSTINATO_TYPE_GROUND_BASS));
    assert(ostinato_type_is_triple_meter_form(OSTINATO_TYPE_CHACONNE));
    assert(ostinato_type_is_triple_meter_form(OSTINATO_TYPE_PASSACAGLIA));
    assert(!ostinato_type_is_triple_meter_form(OSTINATO_TYPE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_is_ground_bass();
    test_is_triple_meter_form();
    printf("test_ostinato_type: ok\n");
    return 0;
}
