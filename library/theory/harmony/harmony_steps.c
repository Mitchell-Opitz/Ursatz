#include "theory/harmony/harmony_steps.h"

static int mod7(int n) {
    return ((n % 7) + 7) % 7;
}

int harmony_step_degree(int step) {
    return mod7(step) + 1;
}

