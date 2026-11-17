#include "knowledge_service.h"

#include <stdio.h>

static KnowledgeSnapshot *g_snapshot = NULL;

bool ursatz_gui_knowledge_init(void) {
  if (g_snapshot) {
    return true;
  }
  char message[256];
  if (knowledge_snapshot_build(NULL, 0, &g_snapshot, message, sizeof(message)) != KNOWLEDGE_OK) {
    fprintf(stderr, "ursatz-gui-backend: cannot read the knowledge: %s\n", message);
    g_snapshot = NULL;
    return false;
  }
  return true;
}

void ursatz_gui_knowledge_shutdown(void) {
  knowledge_snapshot_destroy(g_snapshot);
  g_snapshot = NULL;
}

const KnowledgeSnapshot *ursatz_gui_knowledge(void) {
  return g_snapshot;
}
