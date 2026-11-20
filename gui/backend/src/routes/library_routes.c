#include "library_routes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"

static const size_t kReadChunkSize = 65536;
static const size_t kMaxUploadSize = 64 * 1024 * 1024;

/* Reads the full request body into a newly allocated buffer (caller
 * frees). Grows in kReadChunkSize steps since a raw binary upload's
 * Content-Length is not required to be trusted ahead of read. Returns
 * NULL on allocation failure or if the body exceeds kMaxUploadSize. */
static unsigned char *read_request_body(struct mg_connection *conn, size_t *out_size) {
  size_t capacity = kReadChunkSize;
  unsigned char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total == capacity) {
      if (capacity >= kMaxUploadSize) {
        free(buffer);
        return NULL;
      }
      size_t new_capacity = capacity * 2;
      unsigned char *grown = realloc(buffer, new_capacity);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
      capacity = new_capacity;
    }

    int n = mg_read(conn, buffer + total, capacity - total);
    if (n < 0) {
      free(buffer);
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
  }

  *out_size = total;
  return buffer;
}

static char *empty_string_copy(void) {
  char *copy = malloc(1);
  if (copy) {
    copy[0] = '\0';
  }
  return copy;
}

/* Reads the X-Filename request header (URL-encoded by the frontend
 * since header values can't safely carry arbitrary filename bytes) and
 * decodes it into a newly allocated, caller-owned string. Returns an
 * allocated empty string if the header is absent, NULL only on
 * allocation failure. */
static char *read_filename_header(struct mg_connection *conn) {
  const char *raw = mg_get_header(conn, "X-Filename");
  if (!raw) {
    return empty_string_copy();
  }
  size_t raw_len = strlen(raw);
  char *decoded = malloc(raw_len + 1);
  if (!decoded) {
    return NULL;
  }
  int decoded_len = mg_url_decode(raw, (int)raw_len, decoded, (int)(raw_len + 1), 0);
  if (decoded_len < 0) {
    free(decoded);
    return empty_string_copy();
  }
  decoded[decoded_len] = '\0';
  return decoded;
}

/* The failure of a command that queues a job: no queue is a 503, no memory the route's 200 with {"ok": false}. */
static int failed_job_command(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  if (status == URSATZ_ERR_INTERNAL) {
    return ursatz_gui_send_failure(conn, 503, error->message);
  }
  return ursatz_gui_send_failure(conn, 200, error->message);
}

/* The failures of the Library's writes: a missing piece is a 404, a refused request a 400 (but a refused filename keeps the route's 200 with
 * {"ok": false}), every other failure the route's 200 with {"ok": false, "message"}. */
static int failed_library_write(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  if (status == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (status == URSATZ_ERR_REQUEST && strcmp(error->path, "filename") != 0) {
    return ursatz_gui_send_failure(conn, 400, error->message);
  }
  return ursatz_gui_send_failure(conn, 200, error->message);
}

static int handle_post(struct mg_connection *conn) {
  size_t body_size = 0;
  unsigned char *body_data = read_request_body(conn, &body_size);
  if (!body_data) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 400;
  }

  char *filename = read_filename_header(conn);
  if (!filename) {
    free(body_data);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  /* The import is an "analysis" job (R-645): the answer is 202 with the job, and the job answers what this route used to. */
  cJSON *params = cJSON_CreateObject();
  if (params) {
    cJSON_AddStringToObject(params, "filename", filename);
  }
  WorkshopBytes file = {body_data, body_size, filename};
  int status = ursatz_gui_answer_command(conn, "library.import", "", params, &file, failed_job_command, false);
  free(body_data);
  free(filename);
  return status;
}

/* Library failures: every one answers 200 with {"ok": false, "message"} (the old answers, until B12). */
static int send_library_failure(struct mg_connection *conn, const UrsatzError *error) {
  return ursatz_gui_send_failure(conn, 200, error->message);
}

/* Runs a Library query for the route's id and query variables and answers {"ok": true, ...} or the failure through failed. */
static int run_library_query(struct mg_connection *conn, const char *name, const char *id, UrsatzGuiFailureFn failed) {
  return ursatz_gui_answer_query(conn, name, ursatz_gui_request_params(conn, id ? "id" : NULL, id, NULL), NULL, failed, false);
}

static int failed_200(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  (void)status;
  return send_library_failure(conn, error);
}

/* The failures of a read of one piece's document, export, score or playback: 404 for a missing piece, 422 for a source that no longer
 * imports, 400 for a refused request, 500 for the rest. */
static int failed_piece_read(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  switch (status) {
    case URSATZ_ERR_NOT_FOUND:
      mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
      return 404;
    case URSATZ_ERR_FORMAT:
      return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity", "{\"ok\":false,\"message\":\"could not import the stored source\"}");
    case URSATZ_ERR_REQUEST:
      if (strcmp(error->message, "failed to export piece") != 0) {
        return ursatz_gui_send_failure(conn, 400, error->message);
      }
      /* fall through: the score export of a source the Analyzer refuses is a 500 naming the code */
    default: {
      char body[192];
      if (strcmp(error->message, "failed to export piece") == 0 && (status == URSATZ_ERR_REQUEST || status == URSATZ_ERR_INTERNAL)) {
        snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"failed to export piece\",\"code\":\"%s\"}", ursatz_status_name(status));
        return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", body);
      }
      return ursatz_gui_send_failure(conn, 500, error->message);
    }
  }
}

static int handle_get(struct mg_connection *conn) {
  return run_library_query(conn, "library.list", NULL, failed_200);
}

/* GET /api/analyzer/groups: the distinct composers, sources and tags with piece counts. */
static int handle_get_groups(struct mg_connection *conn) {
  return run_library_query(conn, "library.groups", NULL, failed_200);
}

static int handle_get_piece(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.piece", id, failed_200);
}

static char *read_patch_body(struct mg_connection *conn) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total == capacity) {
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
      capacity *= 2;
    }

    int n = mg_read(conn, buffer + total, capacity - total - 1);
    if (n < 0) {
      free(buffer);
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
  }

  buffer[total] = '\0';
  return buffer;
}

/* PATCH /api/analyzer/pieces/<id>: filename, composer, source, tags, add_tags, remove_tags, notation (an object, or null to clear; all
 * optional, unknown members ignored). A changed notation analyzes the piece again before the answer. */
static int handle_patch_piece(struct mg_connection *conn, const char *id) {
  char *body = read_patch_body(conn);
  if (!body) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 200;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsObject(root)) {
    cJSON_Delete(root);
    return ursatz_gui_send_json_status(conn, 400, "Bad Request", "{\"ok\":false,\"message\":\"invalid request body\"}");
  }
  return ursatz_gui_answer_command(conn, "library.update", id, root, NULL, failed_library_write, false);
}

static int handle_delete_piece(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "library.delete", id, NULL, NULL, failed_library_write, false);
}


/* POST /api/analyzer/pieces/<id>/reanalyze: an "analysis" job; the answer is 202 with the job. */
static int handle_reanalyze_piece(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "library.reanalyze", id, NULL, NULL, failed_job_command, false);
}


/* GET /api/analyzer/pieces/<id>/document?sections=a,b: only those sections of the stored analysis document, read without importing the
   source. */
static int handle_get_piece_document(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.document", id, failed_piece_read);
}

/* GET /api/analyzer/pieces/<id>/export[?from=<m>&to=<m>]: the Analyzer's text export of the piece (text/plain). */
static int handle_get_piece_export(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.export", id, failed_piece_read);
}

static int handle_get_piece_musicxml(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.musicxml", id, failed_piece_read);
}

static int handle_get_piece_musicxml_meta(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.meter-defaulted", id, failed_piece_read);
}

/* The playback document is the answer as the Analyzer made it (it holds its own "ok"). */
static int handle_get_piece_playback(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.playback", id, failed_piece_read);
}

#define ARG(args, i) ((args)->values[i])

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_get(conn); }
static int route_groups(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_get_groups(conn); }
static int route_upload(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_post(conn); }
static int route_piece(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece(conn, ARG(args, 0)); }
static int route_patch(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_patch_piece(conn, ARG(args, 0)); }
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_delete_piece(conn, ARG(args, 0)); }
static int route_reanalyze(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_reanalyze_piece(conn, ARG(args, 0)); }
static int route_document(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_document(conn, ARG(args, 0)); }
static int route_export(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_export(conn, ARG(args, 0)); }
static int route_musicxml(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_musicxml(conn, ARG(args, 0)); }
static int route_meter(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_musicxml_meta(conn, ARG(args, 0)); }
static int route_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_playback(conn, ARG(args, 0)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"POST", "/api/analyzer/pieces", route_upload},
    {"GET", "/api/analyzer/pieces", route_list},
    {"GET", "/api/analyzer/groups", route_groups},
    {"GET", "/api/analyzer/pieces/<id>", route_piece},
    {"PATCH", "/api/analyzer/pieces/<id>", route_patch},
    {"DELETE", "/api/analyzer/pieces/<id>", route_delete},
    {"POST", "/api/analyzer/pieces/<id>/reanalyze", route_reanalyze},
    {"GET", "/api/analyzer/pieces/<id>/document", route_document},
    {"GET", "/api/analyzer/pieces/<id>/export", route_export},
    {"GET", "/api/analyzer/pieces/<id>/musicxml", route_musicxml},
    {"GET", "/api/analyzer/pieces/<id>/musicxml/meta", route_meter},
    {"GET", "/api/analyzer/pieces/<id>/playback", route_playback},
};

const UrsatzGuiRoute *ursatz_gui_library_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
