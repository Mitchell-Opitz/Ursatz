#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

static void test_null_in_null_out(void) {
    assert(ursatz_string_copy(NULL) == NULL);
}

static void test_copy_is_independent(void) {
    char source[] = "standard-form";
    char *copy = ursatz_string_copy(source);
    assert(copy != NULL);
    assert(copy != source);
    assert(strcmp(copy, "standard-form") == 0);
    source[0] = 'X';
    assert(strcmp(copy, "standard-form") == 0);
    free(copy);
}

static void test_empty_string(void) {
    char *copy = ursatz_string_copy("");
    assert(copy != NULL);
    assert(copy[0] == '\0');
    free(copy);
}

int main(void) {
    test_null_in_null_out();
    test_copy_is_independent();
    test_empty_string();
    return 0;
}
