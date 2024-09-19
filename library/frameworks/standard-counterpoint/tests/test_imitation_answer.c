#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "imitation_answer.h"

static void test_validity(void) {
    for (int i = 0; i < IMITATION_ANSWER_COUNT; i++) {
        assert(imitation_answer_valid((ImitationAnswer)i));
    }
    assert(!imitation_answer_valid(IMITATION_ANSWER_COUNT));
    assert(!imitation_answer_valid((ImitationAnswer)-1));
}

static void test_names(void) {
    assert(strcmp(imitation_answer_name(IMITATION_ANSWER_REAL), "real") == 0);
    assert(strcmp(imitation_answer_name(IMITATION_ANSWER_TONAL), "tonal") == 0);
    assert(strcmp(imitation_answer_name(IMITATION_ANSWER_FREE), "free") == 0);
    assert(imitation_answer_name(IMITATION_ANSWER_COUNT) == NULL);
}

static void test_keeps_intervals(void) {
    assert(imitation_answer_keeps_intervals(IMITATION_ANSWER_REAL));
    assert(!imitation_answer_keeps_intervals(IMITATION_ANSWER_TONAL));
    assert(!imitation_answer_keeps_intervals(IMITATION_ANSWER_FREE));
    assert(!imitation_answer_keeps_intervals(IMITATION_ANSWER_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_keeps_intervals();
    printf("test_imitation_answer: ok\n");
    return 0;
}
