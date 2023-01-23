#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/alternative_path_kind.h"

static void test_validity(void) {
    for (int i = 0; i < ALTERNATIVE_PATH_KIND_COUNT; i++) {
        assert(alternative_path_kind_valid((AlternativePathKind)i));
    }
    assert(!alternative_path_kind_valid(ALTERNATIVE_PATH_KIND_COUNT));
    assert(!alternative_path_kind_valid((AlternativePathKind)-1));
}

static void test_names(void) {
    assert(strcmp(alternative_path_kind_name(ALTERNATIVE_PATH_KIND_DETOUR), "detour") == 0);
    assert(strcmp(alternative_path_kind_name(ALTERNATIVE_PATH_KIND_REROUTE), "reroute") == 0);
    assert(alternative_path_kind_name(ALTERNATIVE_PATH_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_alternative_path_kind: ok\n");
    return 0;
}
