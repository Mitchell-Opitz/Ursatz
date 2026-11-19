#include "base/command_context.h"

#include <string.h>

static _Thread_local WorkshopCommandContext t_context;
static _Thread_local bool t_has_context = false;
static _Thread_local long long t_last_seq = 0;

void workshop_command_context_set(const WorkshopCommandContext *context) {
    if (context) {
        t_context = *context;
        t_has_context = true;
    } else {
        memset(&t_context, 0, sizeof(t_context));
        t_has_context = false;
    }
}

const WorkshopCommandContext *workshop_command_context(void) {
    return t_has_context ? &t_context : NULL;
}

void workshop_command_context_set_last_seq(long long seq) {
    t_last_seq = seq;
}

long long workshop_command_context_last_seq(void) {
    return t_last_seq;
}
