#ifndef WORKSHOP_BASE_COMMAND_CONTEXT_H
#define WORKSHOP_BASE_COMMAND_CONTEXT_H

#include <stdbool.h>

/*
 * The command a thread is running (R-646): its name, its actor and the version the caller read. The dispatcher sets it for the length of a
 * command, a transaction (items/txn.h) takes the name, the actor and the base version from it for its journal record, and a job captures it
 * when it is queued and sets it again on the worker that runs it. A thread with no context (a test, the recovery) is the actor "workshop"
 * with each transaction's own name.
 */

typedef struct {
    char command[64];
    char actor[64];
    char target[256];  /* the item the command is about, "" for none */
    long base_version; /* the version of the target the caller read, -1 when it gave none */
} WorkshopCommandContext;

/* Sets this thread's context (a copy); NULL clears it. */
void workshop_command_context_set(const WorkshopCommandContext *context);

/* This thread's context, NULL when none. */
const WorkshopCommandContext *workshop_command_context(void);

/* After a version of the target is written, the version the command works from is that one: a command that writes more than one (a compose that
 * first moves the uses to their sources' newest versions) is not refused for its own first write. */
void workshop_command_context_set_base_version(long base_version);

/* The journal sequence number of the last transaction this thread committed (0 for none yet), and a way to forget it before a command. */
void workshop_command_context_set_last_seq(long long seq);
long long workshop_command_context_last_seq(void);

#endif
