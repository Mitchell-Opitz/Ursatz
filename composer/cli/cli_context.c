#include "cli_context.h"

#include <stdio.h>
#include <stdlib.h>

#include <theory/knowledge/knowledge_snapshot.h>

/* The process's one snapshot and the context over it: a command line process has no caller to hold them. */
static KnowledgeSnapshot *g_snapshot = NULL;
static ComposerContext g_context = {NULL};
static bool g_opened = false;

static void release(void) {
    knowledge_snapshot_destroy(g_snapshot);
    g_snapshot = NULL;
    g_context.knowledge = NULL;
}

const ComposerContext *cli_context(void) {
    if (!g_opened) {
        char message[256];
        g_opened = true;
        if (knowledge_snapshot_build(NULL, 0, &g_snapshot, message, sizeof(message)) != KNOWLEDGE_OK) {
            fprintf(stderr, "Error: cannot read the knowledge: %s\n", message);
        } else {
            g_context.knowledge = g_snapshot;
            atexit(release);
        }
    }
    return &g_context;
}
