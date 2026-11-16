/* Writes the two Composer answers the frontend tests load, vocabulary.json and forms.json, into the folder given as
 * the argument: the exact strings the vocabulary and forms routes serve. The build runs it (gui_frontend_fixtures), so
 * the frontend tests always read what the in-tree Composer says. Returns 1 with a message on any failure. */

#include <stdio.h>
#include <string.h>

#include "service/composer_client.h"

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

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: write_frontend_fixtures <output folder>\n");
    return 1;
  }
  int failed = write_fixture(argv[1], "vocabulary.json", ursatz_gui_composer_vocabulary());
  failed |= write_fixture(argv[1], "forms.json", ursatz_gui_composer_forms());
  return failed ? 1 : 0;
}
