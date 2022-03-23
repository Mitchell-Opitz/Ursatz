#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/subject_part.h"

static void test_validity(void) {
    for (int i = 0; i < SUBJECT_PART_COUNT; i++) {
        assert(subject_part_valid((SubjectPart)i));
    }
    assert(!subject_part_valid(SUBJECT_PART_COUNT));
    assert(!subject_part_valid((SubjectPart)-1));
}

static void test_names(void) {
    assert(strcmp(subject_part_name(SUBJECT_PART_KOPFMOTIV), "kopfmotiv") == 0);
    assert(strcmp(subject_part_name(SUBJECT_PART_FORTSPINNUNG), "fortspinnung") == 0);
    assert(strcmp(subject_part_name(SUBJECT_PART_CADENCE), "cadence") == 0);
    assert(subject_part_name(SUBJECT_PART_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_subject_part: ok\n");
    return 0;
}
