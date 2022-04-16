#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/schema_function.h"

static void test_validity(void) {
    for (int i = 0; i < SCHEMA_FUNCTION_COUNT; i++) {
        assert(schema_function_valid((SchemaFunction)i));
    }
    assert(!schema_function_valid(SCHEMA_FUNCTION_COUNT));
    assert(!schema_function_valid((SchemaFunction)-1));
}

static void test_names(void) {
    assert(strcmp(schema_function_name(SCHEMA_FUNCTION_OPENING_GAMBIT), "opening-gambit") == 0);
    assert(strcmp(schema_function_name(SCHEMA_FUNCTION_CONTINUATION), "continuation") == 0);
    assert(strcmp(schema_function_name(SCHEMA_FUNCTION_CADENCE), "cadence") == 0);
    assert(strcmp(schema_function_name(SCHEMA_FUNCTION_POST_CADENTIAL), "post-cadential") == 0);
    assert(schema_function_name(SCHEMA_FUNCTION_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_schema_function: ok\n");
    return 0;
}
