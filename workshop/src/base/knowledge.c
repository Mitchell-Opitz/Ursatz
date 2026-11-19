#include "knowledge.h"

#include <stdio.h>

static KnowledgeSnapshot *g_snapshot = NULL;
static ComposerContext g_composer_context = {NULL};

bool workshop_knowledge_init(void) {
  if (g_snapshot) {
    return true;
  }
  char message[256];
  if (knowledge_snapshot_build(NULL, 0, &g_snapshot, message, sizeof(message)) != KNOWLEDGE_OK) {
    fprintf(stderr, "ursatz-workshop: cannot read the knowledge: %s\n", message);
    g_snapshot = NULL;
    return false;
  }
  g_composer_context.knowledge = g_snapshot;
  return true;
}

void workshop_knowledge_shutdown(void) {
  g_composer_context.knowledge = NULL;
  knowledge_snapshot_destroy(g_snapshot);
  g_snapshot = NULL;
}

const KnowledgeSnapshot *workshop_knowledge_snapshot(void) {
  return g_snapshot;
}

const ComposerContext *workshop_composer_context_global(void) {
  return &g_composer_context;
}
