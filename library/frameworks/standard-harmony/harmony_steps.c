#include "harmony_steps.h"

static int mod7(int n) {
    return ((n % 7) + 7) % 7;
}

int harmony_step_degree(int step) {
    return mod7(step) + 1;
}

int harmony_step_direction(int from, int to) {
    return to > from ? 1 : (to < from ? -1 : 0);
}

HarmonyPerfectInterval harmony_step_perfect_interval(int a, int b) {
    int lower = a < b ? a : b;
    int span = (a < b ? b : a) - lower;
    if (mod7(span) == 0) {
        return HARMONY_PERFECT_OCTAVE;
    }
    if (mod7(span) == 4 && harmony_step_degree(lower) != 7) {
        return HARMONY_PERFECT_FIFTH;
    }
    return HARMONY_PERFECT_NONE;
}

bool harmony_steps_parallel(int a_from, int a_to, int b_from, int b_to) {
    int direction = harmony_step_direction(a_from, a_to);
    if (direction == 0 || direction != harmony_step_direction(b_from, b_to)) {
        return false;
    }
    HarmonyPerfectInterval before = harmony_step_perfect_interval(a_from, b_from);
    return before != HARMONY_PERFECT_NONE && before == harmony_step_perfect_interval(a_to, b_to);
}
