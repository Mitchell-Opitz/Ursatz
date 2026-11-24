#include "knowledge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>

#define USER_FILES_MAX 64

static KnowledgeSnapshot *g_snapshot = NULL;
static char *g_user_paths[USER_FILES_MAX];
static char *g_user_texts[USER_FILES_MAX];
static size_t g_user_count = 0;
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

static void free_user_files(void) {
  for (size_t i = 0; i < g_user_count; i++) {
    free(g_user_paths[i]);
    free(g_user_texts[i]);
  }
  g_user_count = 0;
}

void workshop_knowledge_shutdown(void) {
  g_composer_context.knowledge = NULL;
  knowledge_snapshot_destroy(g_snapshot);
  g_snapshot = NULL;
  free_user_files();
}

/* Every .json file of `<data_dir>/knowledge`, in name order, as the snapshot's user files. False with a message on stderr when one cannot be read. */
static bool read_user_files(const char *data_dir, KnowledgeUserFile *files, size_t *count) {
  *count = 0;
  char *folder = common_path_join_alloc(data_dir, "knowledge");
  CommonNames names;
  UrsatzError error;
  bool ok = folder != NULL;
  if (!ok || !common_path_is_dir(folder) || common_dir_list(folder, &names, &error) != URSATZ_OK) {
    free(folder);
    return ok; /* no folder, no user files */
  }
  for (size_t i = 0; ok && i < names.count && g_user_count < USER_FILES_MAX; i++) {
    size_t length = strlen(names.items[i]);
    if (length <= 5 || strcmp(names.items[i] + length - 5, ".json") != 0 || names.items[i][0] == '.') {
      continue;
    }
    char *path = common_path_join_alloc(folder, names.items[i]);
    char *text = NULL;
    if (path == NULL || common_file_read(path, &text, NULL, &error) != URSATZ_OK) {
      fprintf(stderr, "ursatz-workshop: cannot read the knowledge file %s\n", path ? path : names.items[i]);
      free(path);
      ok = false;
      break;
    }
    g_user_paths[g_user_count] = path;
    g_user_texts[g_user_count++] = text;
  }
  common_names_free(&names);
  free(folder);
  for (size_t i = 0; ok && i < g_user_count; i++) {
    files[i].path = g_user_paths[i];
    files[i].text = g_user_texts[i];
  }
  *count = g_user_count;
  if (!ok) {
    free_user_files();
  }
  return ok;
}

bool workshop_knowledge_init_folder(const char *data_dir) {
  if (g_snapshot) {
    return true;
  }
  KnowledgeUserFile files[USER_FILES_MAX];
  size_t count = 0;
  if (data_dir == NULL || !read_user_files(data_dir, files, &count)) {
    return data_dir == NULL ? workshop_knowledge_init() : false;
  }
  char message[512];
  if (knowledge_snapshot_build(files, count, &g_snapshot, message, sizeof(message)) != KNOWLEDGE_OK) {
    fprintf(stderr, "ursatz-workshop: cannot read the knowledge: %s\n", message);
    g_snapshot = NULL;
    free_user_files();
    return false;
  }
  g_composer_context.knowledge = g_snapshot;
  return true;
}

const KnowledgeSnapshot *workshop_knowledge_snapshot(void) {
  return g_snapshot;
}

const ComposerContext *workshop_composer_context_global(void) {
  return &g_composer_context;
}
