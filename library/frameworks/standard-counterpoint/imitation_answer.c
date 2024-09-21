#include "imitation_answer.h"

#include <stddef.h>

static const char *const NAMES[IMITATION_ANSWER_COUNT] = {"real", "tonal", "free"};

bool imitation_answer_valid(ImitationAnswer answer) {
    return answer >= 0 && answer < IMITATION_ANSWER_COUNT;
}

const char *imitation_answer_name(ImitationAnswer answer) {
    return imitation_answer_valid(answer) ? NAMES[answer] : NULL;
}

bool imitation_answer_keeps_intervals(ImitationAnswer answer) {
    return answer == IMITATION_ANSWER_REAL;
}
