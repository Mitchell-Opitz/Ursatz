#include "http_status.h"

#include <string.h>

int workshop_http_status(UrsatzStatus status, const UrsatzError *error) {
  switch (status) {
    case URSATZ_OK:
      return 200;
    case URSATZ_ERR_REQUEST:
      return error && error->path[0] != '\0' ? 422 : 400;
    case URSATZ_ERR_UNSATISFIABLE:
      return 422;
    case URSATZ_ERR_NOT_FOUND:
      return 404;
    case URSATZ_ERR_CONFLICT:
      return 409;
    case URSATZ_ERR_CANCELLED:
      return 499;
    case URSATZ_ERR_FORMAT:
    case URSATZ_ERR_IO:
    case URSATZ_ERR_ALLOC:
    case URSATZ_ERR_INTERNAL:
    default:
      return 500;
  }
}

bool workshop_is_call_path(const char *path) {
  static const char *const kCallPaths[] = {"/params", "/body", "/target", "/bytes", "/base_version", "/command", "/actor", "/name", "/query"};
  for (size_t i = 0; path && i < sizeof(kCallPaths) / sizeof(kCallPaths[0]); i++) {
    if (strncmp(path, kCallPaths[i], strlen(kCallPaths[i])) == 0) {
      return true;
    }
  }
  return false;
}
