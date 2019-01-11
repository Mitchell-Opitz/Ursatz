#include "containers/container.h"

#include <assert.h>

/*
 * Invariant test: Container is a pure grouping base type. It defines
 * no fields, no create/destroy, and no membership accessors -- it
 * makes no assumption about what a concrete container groups. There is
 * nothing to construct or tear down; this test's only job is to
 * confirm the header compiles standalone with no such assumptions
 * baked in.
 */

int main(void) {
    assert(1);
    return 0;
}