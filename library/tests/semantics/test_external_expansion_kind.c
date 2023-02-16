#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/external_expansion_kind.h"

static void test_validity(void) {
    for (int i = 0; i < EXTERNAL_EXPANSION_KIND_COUNT; i++) {
        assert(external_expansion_kind_valid((ExternalExpansionKind)i));
    }
    assert(!external_expansion_kind_valid(EXTERNAL_EXPANSION_KIND_COUNT));
    assert(!external_expansion_kind_valid((ExternalExpansionKind)-1));
}

static void test_names(void) {
    assert(strcmp(external_expansion_kind_name(EXTERNAL_EXPANSION_KIND_PREFIX), "prefix") == 0);
    assert(strcmp(external_expansion_kind_name(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION), "post-cadential-extension") == 0);
    assert(strcmp(external_expansion_kind_name(EXTERNAL_EXPANSION_KIND_CODETTA), "codetta") == 0);
    assert(strcmp(external_expansion_kind_name(EXTERNAL_EXPANSION_KIND_CODA), "coda") == 0);
    assert(external_expansion_kind_name(EXTERNAL_EXPANSION_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_external_expansion_kind: ok\n");
    return 0;
}
