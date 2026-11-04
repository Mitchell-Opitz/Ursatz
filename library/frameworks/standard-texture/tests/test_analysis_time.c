#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "texture_analysis_time.h"

/*
 * The standard-texture time module is a wrapper over the kernel's checked score time. Ordinary values read as before; a value
 * that does not fit a Rational is refused: the operators return 0/1 (their failure value) and the snap leaves the time alone.
 * Two coprime denominators near 2^62 make every cross product overflow.
 */

#define BIG 4000000000000000000LL

static bool is_zero(StandardTextureQ q) {
    return q.n == 0 && q.d == 1;
}

static void test_ordinary_values(void) {
    StandardTextureQ a = standard_texture_q(1, 4);
    StandardTextureQ b = standard_texture_q(1, 6);
    StandardTextureQ sum = standard_texture_q_add(a, b);
    assert(sum.n == 5 && sum.d == 12);
    StandardTextureQ difference = standard_texture_q_sub(a, b);
    assert(difference.n == 1 && difference.d == 12);
}

static void test_add_overflow_is_refused(void) {
    assert(is_zero(standard_texture_q_add(standard_texture_q(1, BIG), standard_texture_q(1, BIG + 1))));
}

static void test_sub_overflow_is_refused(void) {
    assert(is_zero(standard_texture_q_sub(standard_texture_q(1, BIG), standard_texture_q(1, BIG + 1))));
}

static void test_snap(void) {
    StandardTextureConfig config = standard_texture_config_default();
    StandardTextureQ on_grid = standard_texture_q(3, 32);
    StandardTextureQ same = standard_texture_snap(&config, on_grid, false);
    assert(standard_texture_q_compare(same, on_grid) == 0);
    /* time * grid does not fit: the time is returned as it is */
    StandardTextureQ huge = standard_texture_q(LLONG_MAX / 2, 1);
    StandardTextureQ out = standard_texture_snap(&config, huge, false);
    assert(out.n == huge.n && out.d == huge.d);
}

int main(void) {
    test_ordinary_values();
    test_add_overflow_is_refused();
    test_sub_overflow_is_refused();
    test_snap();
    printf("standard_texture_analysis_time: ok\n");
    return 0;
}
