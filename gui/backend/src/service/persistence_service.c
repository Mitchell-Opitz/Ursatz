#include "persistence_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <persistence/persistence.h>

static const char *result_string(PersistenceResult result) {
  switch (result) {
    case PERSISTENCE_OK:
      return "ok";
    case PERSISTENCE_ERR_NULL_ARG:
      return "null argument";
    case PERSISTENCE_ERR_OPEN_FAILED:
      return "open failed";
    case PERSISTENCE_ERR_SCHEMA_FAILED:
      return "schema creation failed";
    case PERSISTENCE_ERR_ALLOC:
      return "allocation failed";
    default:
      return "unknown error";
  }
}

bool ursatz_gui_persistence_check(const char *db_path, char **out_message) {
  PersistenceHandle *handle = NULL;
  PersistenceResult result = persistence_open(db_path, &handle);

  if (result != PERSISTENCE_OK) {
    const char *reason = result_string(result);
    size_t len = strlen(reason) + 64;
    *out_message = malloc(len);
    snprintf(*out_message, len, "persistence_open failed: %s", reason);
    return false;
  }

  persistence_close(handle);

  const char *ok_text = "persistence_open succeeded";
  *out_message = malloc(strlen(ok_text) + 1);
  strcpy(*out_message, ok_text);
  return true;
}
