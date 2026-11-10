#include "composer_client.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

void ursatz_gui_composer_reply_take(ComposerStatus status, ComposerResponse *response, UrsatzGuiComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  reply->status = status;
  if (!response) {
    return;
  }
  reply->json = response->json;
  reply->plan_json = response->plan_json;
  reply->midi = response->midi;
  reply->midi_size = response->midi_size;
  reply->musicxml = response->musicxml;
  response->json = NULL;
  response->plan_json = NULL;
  response->midi = NULL;
  response->midi_size = 0;
  response->musicxml = NULL;
  memcpy(reply->message, response->error.message, sizeof(reply->message));
  memcpy(reply->path, response->error.path, sizeof(reply->path));
  reply->message[sizeof(reply->message) - 1] = '\0';
  reply->path[sizeof(reply->path) - 1] = '\0';
  composer_response_destroy(response);
}

void ursatz_gui_composer_reply_destroy(UrsatzGuiComposerReply *reply) {
  if (!reply) {
    return;
  }
  free(reply->json);
  free(reply->plan_json);
  free(reply->midi);
  free(reply->musicxml);
  memset(reply, 0, sizeof(*reply));
}

static pthread_once_t g_vocabulary_once = PTHREAD_ONCE_INIT;
static char *g_vocabulary = NULL;
static cJSON *g_vocabulary_tree = NULL;

static void vocabulary_build(void) {
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_vocabulary(&response);
  UrsatzGuiComposerReply reply;
  ursatz_gui_composer_reply_take(status, &response, &reply);
  if (reply.status == COMPOSER_OK && reply.json) {
    g_vocabulary = reply.json;
    reply.json = NULL;
    g_vocabulary_tree = cJSON_Parse(g_vocabulary);
  }
  ursatz_gui_composer_reply_destroy(&reply);
}

const char *ursatz_gui_composer_vocabulary(void) {
  pthread_once(&g_vocabulary_once, vocabulary_build);
  return g_vocabulary;
}

static pthread_once_t g_forms_once = PTHREAD_ONCE_INIT;
static char *g_forms = NULL;
static cJSON *g_forms_tree = NULL;

static void forms_build(void) {
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_forms(&response);
  UrsatzGuiComposerReply reply;
  ursatz_gui_composer_reply_take(status, &response, &reply);
  if (reply.status == COMPOSER_OK && reply.json) {
    g_forms = reply.json;
    reply.json = NULL;
    g_forms_tree = cJSON_Parse(g_forms);
  }
  ursatz_gui_composer_reply_destroy(&reply);
}

const char *ursatz_gui_composer_forms(void) {
  pthread_once(&g_forms_once, forms_build);
  return g_forms;
}

const cJSON *ursatz_gui_composer_form(const char *name) {
  if (!name || !ursatz_gui_composer_forms() || !g_forms_tree) {
    return NULL;
  }
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(g_forms_tree, "forms")) {
    const cJSON *entry_name = cJSON_GetObjectItemCaseSensitive(entry, "name");
    if (cJSON_IsString(entry_name) && strcmp(entry_name->valuestring, name) == 0) {
      return entry;
    }
  }
  return NULL;
}

static pthread_once_t g_build_once = PTHREAD_ONCE_INIT;
static char g_build_api[32];
static char g_build_composer[64];
static char g_build_ursatz[64];

static void build_copy(char *out, size_t out_size, const cJSON *item) {
  if (cJSON_IsString(item) && strlen(item->valuestring) < out_size) {
    memcpy(out, item->valuestring, strlen(item->valuestring) + 1);
  }
}

static void build_read(void) {
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_versions(&response);
  UrsatzGuiComposerReply reply;
  ursatz_gui_composer_reply_take(status, &response, &reply);
  cJSON *versions = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (versions) {
    build_copy(g_build_api, sizeof(g_build_api), cJSON_GetObjectItemCaseSensitive(versions, "api_version"));
    build_copy(g_build_composer, sizeof(g_build_composer), cJSON_GetObjectItemCaseSensitive(versions, "composer_version"));
    build_copy(g_build_ursatz, sizeof(g_build_ursatz), cJSON_GetObjectItemCaseSensitive(versions, "ursatz_commit"));
  }
  cJSON_Delete(versions);
  ursatz_gui_composer_reply_destroy(&reply);
}

UrsatzGuiComposerBuild ursatz_gui_composer_build(void) {
  pthread_once(&g_build_once, build_read);
  UrsatzGuiComposerBuild build = {g_build_api, g_build_composer, g_build_ursatz};
  return build;
}

double ursatz_gui_composer_seed_max(const char *kind) {
  if (!kind || !ursatz_gui_composer_vocabulary() || !g_vocabulary_tree) {
    return -1;
  }
  const cJSON *limits = cJSON_GetObjectItemCaseSensitive(g_vocabulary_tree, "limits");
  const cJSON *table = cJSON_GetObjectItemCaseSensitive(limits, "seed_max");
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(table, kind);
  return cJSON_IsNumber(value) ? value->valuedouble : -1;
}

double ursatz_gui_composer_limit(const char *name) {
  if (!name || !ursatz_gui_composer_vocabulary() || !g_vocabulary_tree) {
    return -1;
  }
  const cJSON *limits = cJSON_GetObjectItemCaseSensitive(g_vocabulary_tree, "limits");
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(limits, name);
  return cJSON_IsNumber(value) ? value->valuedouble : -1;
}

int ursatz_gui_composer_vocabulary_index(const char *list, const char *value) {
  if (!list || !value || !ursatz_gui_composer_vocabulary() || !g_vocabulary_tree) {
    return -1;
  }
  int index = 0;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(g_vocabulary_tree, list)) {
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "value");
    if (cJSON_IsString(name) && strcmp(name->valuestring, value) == 0) {
      return index;
    }
    index++;
  }
  return -1;
}

bool ursatz_gui_composer_vocabulary_has(const char *list, const char *value) {
  return ursatz_gui_composer_vocabulary_index(list, value) >= 0;
}

bool ursatz_gui_composer_tonic_name(int pitch_class, const char *mode, char *out, size_t out_size) {
  if (!ursatz_gui_composer_vocabulary() || !g_vocabulary_tree || !mode) {
    return false;
  }
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(g_vocabulary_tree, "pitch_class_names")) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(entry, "value");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, mode);
    if (cJSON_IsNumber(value) && value->valueint == pitch_class && cJSON_IsString(name)) {
      snprintf(out, out_size, "%s", name->valuestring);
      return true;
    }
  }
  return false;
}

bool ursatz_gui_composer_reply_refused(const UrsatzGuiComposerReply *reply) {
  return reply->status == COMPOSER_ERR_REQUEST || reply->status == COMPOSER_ERR_UNSATISFIABLE;
}

int ursatz_gui_composer_reply_failure_status(const UrsatzGuiComposerReply *reply) {
  switch (reply->status) {
    case COMPOSER_ERR_UNSATISFIABLE:
      return 422;
    case COMPOSER_ERR_REQUEST:
      return 400;
    case COMPOSER_ERR_CANCELLED:
      return 499;
    default:
      return 500;
  }
}
