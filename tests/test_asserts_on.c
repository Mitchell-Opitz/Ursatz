/* Guard: ursatz_test() compiles every test with -UNDEBUG, so assert() checks in any build type. This test fails when
 * NDEBUG reaches a test executable (a Release build without the guard turns every assert into nothing). */
#include <assert.h>
#include <stdio.h>

int main(void) {
#ifdef NDEBUG
    fprintf(stderr, "test_asserts_on: NDEBUG is defined, so assert() checks nothing\n");
    return 1;
#else
    int checked = 0;
    assert((checked = 1) == 1);
    if (!checked) {
        fprintf(stderr, "test_asserts_on: assert() did not evaluate its argument\n");
        return 1;
    }
    printf("test_asserts_on: ok\n");
    return 0;
#endif
}
