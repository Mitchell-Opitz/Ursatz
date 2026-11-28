#include "knowledge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <pthread.h>

#define USER_FILES_MAX 64

static KnowledgeSnapshot *g_snapshot = NULL;
static char *g_user_paths[USER_FILES_MAX];
static char *g_user_texts[USER_FILES_MAX];
static size_t g_user_count = 0;
static ComposerContext g_composer_context = {NULL};
static char *g_data_dir = NULL;
#define RETIRED_MAX 256
static KnowledgeSnapshot *g_retired[RETIRED_MAX]; /* snapshots a rebuild replaced: a running job may hold one, so they are freed at the shutdown */
static size_t g_retired_count = 0;
static pthread_mutex_t g_rebuild_lock = PTHREAD_MUTEX_INITIALIZER;

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
  for (size_t i = 0; i < g_retired_count; i++) {
    knowledge_snapshot_destroy(g_retired[i]);
    g_retired[i] = NULL;
  }
  g_retired_count = 0;
  free(g_data_dir);
  g_data_dir = NULL;
  free_user_files();
}

/* Every .json file of `<data_dir>/knowledge`, in name order, read into paths[] and texts[] (up to capacity; the caller frees each). False with
   a message on stderr when one cannot be read, nothing then left in the arrays. */
static bool read_folder(const char *data_dir, char **paths, char **texts, size_t capacity, size_t *count) {
  *count = 0;
  char *folder = common_path_join_alloc(data_dir, "knowledge");
  CommonNames names;
  UrsatzError error;
  bool ok = folder != NULL;
  if (!ok || !common_path_is_dir(folder) || common_dir_list(folder, &names, &error) != URSATZ_OK) {
    free(folder);
    return ok; /* no folder, no user files */
  }
  for (size_t i = 0; ok && i < names.count && *count < capacity; i++) {
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
    paths[*count] = path;
    texts[(*count)++] = text;
  }
  common_names_free(&names);
  free(folder);
  if (!ok) {
    for (size_t i = 0; i < *count; i++) {
      free(paths[i]);
      free(texts[i]);
    }
    *count = 0;
  }
  return ok;
}

/* The data folder's files as the snapshot's user files, kept in the globals. */
static bool read_user_files(const char *data_dir, KnowledgeUserFile *files, size_t *count) {
  *count = 0;
  if (!read_folder(data_dir, g_user_paths, g_user_texts, USER_FILES_MAX, &g_user_count)) {
    return false;
  }
  for (size_t i = 0; i < g_user_count; i++) {
    files[i].path = g_user_paths[i];
    files[i].text = g_user_texts[i];
  }
  *count = g_user_count;
  return true;
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
  g_data_dir = common_strdup(data_dir);
  return true;
}

UrsatzStatus workshop_knowledge_rebuild(void) {
  char *paths[USER_FILES_MAX];
  char *texts[USER_FILES_MAX];
  KnowledgeUserFile files[USER_FILES_MAX];
  size_t count = 0;
  UrsatzStatus status = URSATZ_OK;
  pthread_mutex_lock(&g_rebuild_lock);
  if (g_data_dir == NULL || g_snapshot == NULL || g_retired_count >= RETIRED_MAX) {
    pthread_mutex_unlock(&g_rebuild_lock);
    return URSATZ_ERR_INTERNAL;
  }
  if (!read_folder(g_data_dir, paths, texts, USER_FILES_MAX, &count)) {
    pthread_mutex_unlock(&g_rebuild_lock);
    return URSATZ_ERR_IO;
  }
  for (size_t i = 0; i < count; i++) {
    files[i].path = paths[i];
    files[i].text = texts[i];
  }
  KnowledgeSnapshot *built = NULL;
  char message[512];
  if (knowledge_snapshot_build(files, count, &built, message, sizeof(message)) != KNOWLEDGE_OK) {
    fprintf(stderr, "ursatz-workshop: cannot rebuild the knowledge: %s\n", message);
    status = URSATZ_ERR_INTERNAL;
  } else {
    g_retired[g_retired_count++] = g_snapshot;
    g_snapshot = built;
    g_composer_context.knowledge = built;
  }
  for (size_t i = 0; i < count; i++) {
    free(paths[i]);
    free(texts[i]);
  }
  pthread_mutex_unlock(&g_rebuild_lock);
  return status;
}

const KnowledgeSnapshot *workshop_knowledge_snapshot(void) {
  return g_snapshot;
}

const ComposerContext *workshop_composer_context_global(void) {
  return &g_composer_context;
}

bool workshop_knowledge_check(const char *data_dir, const char *skip_file, const char *document_text, char *message, size_t message_size) {
  char *paths[USER_FILES_MAX];
  char *texts[USER_FILES_MAX];
  KnowledgeUserFile files[USER_FILES_MAX + 1];
  size_t count = 0;
  if (message_size > 0) {
    message[0] = '\0';
  }
  if (data_dir != NULL && !read_folder(data_dir, paths, texts, USER_FILES_MAX, &count)) {
    snprintf(message, message_size, "the knowledge folder cannot be read");
    return false;
  }
  size_t used = 0;
  for (size_t i = 0; i < count; i++) {
    const char *slash = strrchr(paths[i], '/');
    if (skip_file != NULL && strcmp(slash ? slash + 1 : paths[i], skip_file) == 0) {
      continue; /* the file the document replaces */
    }
    files[used].path = paths[i];
    files[used++].text = texts[i];
  }
  files[used].path = skip_file != NULL ? skip_file : "file";
  files[used].text = document_text;
  KnowledgeSnapshot *snapshot = NULL;
  bool ok = knowledge_snapshot_build(files, used + 1, &snapshot, message, message_size) == KNOWLEDGE_OK;
  knowledge_snapshot_destroy(snapshot);
  for (size_t i = 0; i < count; i++) {
    free(paths[i]);
    free(texts[i]);
  }
  return ok;
}
