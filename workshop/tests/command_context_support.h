#ifndef URSATZ_WORKSHOP_TESTS_COMMAND_CONTEXT_SUPPORT_H
#define URSATZ_WORKSHOP_TESTS_COMMAND_CONTEXT_SUPPORT_H

#include <stdio.h>
#include <string.h>

#include "base/command_context.h"

/*
 * A write needs a command (items/txn.h): a test that calls a service directly, with no command around the call, runs it inside a context
 * with the actor "michelle", the one every service wrote before. The context names no command, so a record keeps the name its transaction
 * gives it, and no base version. A command that is run through the workshop sets and clears its own context, so use the block for the
 * direct calls only:
 *
 *     IN_COMMAND_CONTEXT { status = workshop_subject_create(...); }
 */

static inline void command_context_support_begin(void) {
    WorkshopCommandContext context;
    memset(&context, 0, sizeof(context));
    snprintf(context.actor, sizeof(context.actor), "%s", "michelle");
    context.base_version = -1;
    workshop_command_context_set(&context);
}

static inline void command_context_support_end(void) {
    workshop_command_context_set(NULL);
}

#define IN_COMMAND_CONTEXT \
    for (int command_context_once = (command_context_support_begin(), 1); command_context_once; command_context_once = (command_context_support_end(), 0))

#endif
