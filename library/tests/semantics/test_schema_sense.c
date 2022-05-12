#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/schema_sense.h"

static void test_validity(void) {
    for (int i = 0; i < SCHEMA_SENSE_COUNT; i++) {
        assert(schema_sense_valid((SchemaSense)i));
    }
    assert(!schema_sense_valid(SCHEMA_SENSE_COUNT));
    assert(!schema_sense_valid((SchemaSense)-1));
}

static void test_names(void) {
    assert(strcmp(schema_sense_name(SCHEMA_SENSE_PROTOTYPE), "prototype") == 0);
    assert(strcmp(schema_sense_name(SCHEMA_SENSE_EXEMPLAR), "exemplar") == 0);
    assert(strcmp(schema_sense_name(SCHEMA_SENSE_THEORY), "theory") == 0);
    assert(schema_sense_name(SCHEMA_SENSE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_schema_sense: ok\n");
    return 0;
}
