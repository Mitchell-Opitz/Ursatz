#include "persistence_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz/persistence.h>

bool ursatz_gui_persistence_check(const char *db_path, char **out_message) {
  ursatz_persistence_handle *handle = NULL;
  ursatz_status status = ursatz_persistence_open(db_path, &handle);

  if (status != URSATZ_OK) {
    const char *reason = ursatz_status_string(status);
    size_t len = strlen(reason) + 64;
    *out_message = malloc(len);
    snprintf(*out_message, len, "persistence_open failed: %s", reason);
    return false;
  }

  ursatz_persistence_close(handle);

  const char *ok_text = "persistence_open succeeded";
  *out_message = malloc(strlen(ok_text) + 1);
  strcpy(*out_message, ok_text);
  return true;
}
