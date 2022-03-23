#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/subject_character.h"

static void test_validity(void) {
    for (int i = 0; i < SUBJECT_CHARACTER_COUNT; i++) {
        assert(subject_character_valid((SubjectCharacter)i));
    }
    assert(!subject_character_valid(SUBJECT_CHARACTER_COUNT));
    assert(!subject_character_valid((SubjectCharacter)-1));
}

static void test_names(void) {
    assert(strcmp(subject_character_name(SUBJECT_CHARACTER_TOCCATA), "toccata") == 0);
    assert(strcmp(subject_character_name(SUBJECT_CHARACTER_RICERCAR), "ricercar") == 0);
    assert(subject_character_name(SUBJECT_CHARACTER_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_subject_character: ok\n");
    return 0;
}
