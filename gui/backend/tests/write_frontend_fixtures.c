/* Writes the Composer answers the frontend tests load into the folder given as the argument: vocabulary.json and forms.json, the
 * exact strings the vocabulary and forms routes serve, and piece-<form>.json, the summary the Composer composes for a piece of each
 * form with sizes (P63 B16: the labels, positions and seam rules the pages show are the Composer's, so a test reads them from it). The
 * build runs it (gui_frontend_fixtures), so the frontend tests always read what the in-tree Composer says. Returns 1 with a message on
 * any failure. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "coordinator/services.h"

static int write_fixture(const char *folder, const char *name, const char *text) {
  if (!text) {
    fprintf(stderr, "write_frontend_fixtures: the Composer gave no %s\n", name);
    return 1;
  }
  char path[4096];
  int length = snprintf(path, sizeof path, "%s/%s", folder, name);
  if (length < 0 || (size_t)length >= sizeof path) {
    fprintf(stderr, "write_frontend_fixtures: the path for %s is too long\n", name);
    return 1;
  }
  FILE *file = fopen(path, "wb");
  if (!file) {
    fprintf(stderr, "write_frontend_fixtures: cannot open %s\n", path);
    return 1;
  }
  size_t size = strlen(text);
  int failed = fwrite(text, 1, size, file) != size;
  failed = (fclose(file) != 0) || failed;
  if (failed) {
    fprintf(stderr, "write_frontend_fixtures: cannot write %s\n", path);
    return 1;
  }
  return 0;
}

/* The summary of a piece of a form with sizes composed from seed 1 in C major, as `piece --format json` prints it (no plans, no files). */
static int write_piece(const char *folder, const char *form, const char *meter) {
  char request[512];
  snprintf(request, sizeof request,
           "{\"request\":{\"schema\":\"ursatz-piece-request\",\"schema_version\":2,\"form\":\"%s\",\"key\":\"C\",\"material\":\"omt.material.major\","
           "\"meter\":\"%s\",\"seed\":1}}",
           form, meter);
  ComposerResponse response;
  memset(&response, 0, sizeof response);
  if (composer_api_piece(workshop_composer_context_global(), request, NULL, &response) != URSATZ_OK || !response.json) {
    fprintf(stderr, "write_frontend_fixtures: the Composer cannot compose a %s piece: %s\n", form, response.error.message);
    composer_response_destroy(&response);
    return 1;
  }
  cJSON *document = cJSON_Parse(response.json);
  composer_response_destroy(&response);
  cJSON_DeleteItemFromObjectCaseSensitive(document, "plans");
  char *text = document ? cJSON_Print(document) : NULL;
  char name[96];
  snprintf(name, sizeof name, "piece-%s.json", form);
  int failed = write_fixture(folder, name, text);
  free(text);
  cJSON_Delete(document);
  return failed;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: write_frontend_fixtures <output folder>\n");
    return 1;
  }
  /* The Composer answers through the call context, which holds the backend's knowledge snapshot. */
  if (!workshop_knowledge_init()) {
    fprintf(stderr, "write_frontend_fixtures: the knowledge snapshot cannot be built\n");
    return 1;
  }
  int failed = write_fixture(argv[1], "vocabulary.json", workshop_composer_vocabulary());
  failed |= write_fixture(argv[1], "forms.json", workshop_composer_forms());
  static const char *const kForms[][2] = {{"ternary", "4/4"},   {"rounded-binary", "4/4"}, {"minuet", "3/4"},    {"scherzo", "3/4"},
                                          {"variations", "4/4"}, {"figured-variations", "2/4"}, {"sonatina", "4/4"}, {"small-rondo-ababa", "4/4"}};
  for (size_t i = 0; i < sizeof kForms / sizeof kForms[0]; i++) {
    failed |= write_piece(argv[1], kForms[i][0], kForms[i][1]);
  }
  workshop_knowledge_shutdown();
  return failed ? 1 : 0;
}
