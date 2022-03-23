#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/fugue_role.h"

static void test_validity(void) {
    for (int i = 0; i < FUGUE_ROLE_COUNT; i++) {
        assert(fugue_role_valid((FugueRole)i));
    }
    assert(!fugue_role_valid(FUGUE_ROLE_COUNT));
    assert(!fugue_role_valid((FugueRole)-1));
}

static void test_names(void) {
    assert(strcmp(fugue_role_name(FUGUE_ROLE_SUBJECT), "subject") == 0);
    assert(strcmp(fugue_role_name(FUGUE_ROLE_ANSWER), "answer") == 0);
    assert(strcmp(fugue_role_name(FUGUE_ROLE_COUNTERSUBJECT), "countersubject") == 0);
    assert(strcmp(fugue_role_name(FUGUE_ROLE_FREE_COUNTERPOINT), "free-counterpoint") == 0);
    assert(strcmp(fugue_role_name(FUGUE_ROLE_LINK), "link") == 0);
    assert(strcmp(fugue_role_name(FUGUE_ROLE_EPISODE), "episode") == 0);
    assert(fugue_role_name(FUGUE_ROLE_COUNT) == NULL);
}

static void test_is_entry(void) {
    assert(fugue_role_is_entry(FUGUE_ROLE_SUBJECT));
    assert(fugue_role_is_entry(FUGUE_ROLE_ANSWER));
    assert(!fugue_role_is_entry(FUGUE_ROLE_COUNTERSUBJECT));
    assert(!fugue_role_is_entry(FUGUE_ROLE_FREE_COUNTERPOINT));
    assert(!fugue_role_is_entry(FUGUE_ROLE_LINK));
    assert(!fugue_role_is_entry(FUGUE_ROLE_EPISODE));
    assert(!fugue_role_is_entry(FUGUE_ROLE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_is_entry();
    printf("test_fugue_role: ok\n");
    return 0;
}
