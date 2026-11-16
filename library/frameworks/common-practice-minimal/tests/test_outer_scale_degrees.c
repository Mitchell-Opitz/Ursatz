#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "outer_scale_degrees.h"

/*
 * Scale degree label tests: the label of a pitch class under a tonic and a mode (outer_scale_degrees.h), in a major and a
 * minor key, and what it refuses.
 */

static void test_the_label_function_directly(void) {
    assert(strcmp(common_practice_scale_degree_label(0, 0, COMMON_PRACTICE_KEY_MAJOR), "1") == 0);
    assert(strcmp(common_practice_scale_degree_label(6, 0, COMMON_PRACTICE_KEY_MAJOR), "#4") == 0);
    assert(strcmp(common_practice_scale_degree_label(2, 9, COMMON_PRACTICE_KEY_MINOR), "4") == 0);
    assert(common_practice_scale_degree_label(12, 0, COMMON_PRACTICE_KEY_MAJOR) == NULL);
    assert(common_practice_scale_degree_label(0, -1, COMMON_PRACTICE_KEY_MAJOR) == NULL);
    assert(common_practice_scale_degree_label(0, 0, (CommonPracticeKeyMode)7) == NULL);
}

int main(void) {
    test_the_label_function_directly();
    printf("test_outer_scale_degrees: ok\n");
    return 0;
}
