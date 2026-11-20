#include "route_table.h"

#include <stdio.h>
#include <string.h>

#include <civetweb.h>

#include "collections_routes.h"
#include "composer_routes.h"
#include "events_routes.h"
#include "jobs_routes.h"
#include "library_routes.h"
#include "links_routes.h"
#include "materials_routes.h"
#include "motifs_routes.h"
#include "route_util.h"
#include "styles_routes.h"
#include "subjects_routes.h"
#include "workshop_routes.h"

#define SEGMENTS_MAX 10

/* Splits text (modified) at '/' into segments; false when there are too many, an empty one, or one of 256 bytes or more. */
static int split(char *text, char *segments[SEGMENTS_MAX], size_t *count) {
  *count = 0;
  if (*text != '/') {
    return 0;
  }
  text++;
  while (*text != '\0') {
    char *slash = strchr(text, '/');
    size_t length = slash ? (size_t)(slash - text) : strlen(text);
    if (length == 0 || length >= URSATZ_GUI_ROUTE_ARG_BYTES || *count == SEGMENTS_MAX) {
      return 0;
    }
    segments[(*count)++] = text;
    if (!slash) {
      return 1;
    }
    *slash = '\0';
    text = slash + 1;
    if (*text == '\0') {
      return 0; /* a trailing slash */
    }
  }
  return *count > 0;
}

int ursatz_gui_route_match(const char *pattern, const char *path, UrsatzGuiRouteArgs *args) {
  char pattern_text[512];
  char path_text[1024];
  char *pattern_segments[SEGMENTS_MAX];
  char *path_segments[SEGMENTS_MAX];
  size_t pattern_count = 0;
  size_t path_count = 0;
  if (strlen(pattern) >= sizeof(pattern_text) || strlen(path) >= sizeof(path_text)) {
    return 0;
  }
  memcpy(pattern_text, pattern, strlen(pattern) + 1);
  memcpy(path_text, path, strlen(path) + 1);
  if (!split(pattern_text, pattern_segments, &pattern_count) || !split(path_text, path_segments, &path_count) || pattern_count != path_count) {
    return 0;
  }
  args->count = 0;
  for (size_t i = 0; i < pattern_count; i++) {
    if (pattern_segments[i][0] == '<') {
      if (args->count == URSATZ_GUI_ROUTE_ARGS_MAX) {
        return 0;
      }
      memcpy(args->values[args->count], path_segments[i], strlen(path_segments[i]) + 1);
      args->count++;
    } else if (strcmp(pattern_segments[i], path_segments[i]) != 0) {
      return 0;
    }
  }
  return 1;
}

typedef const UrsatzGuiRoute *(*RouteFile)(size_t *count);

static const RouteFile kRouteFiles[] = {
    /* the motif rows first: "/items/validate" is a word, and a pattern with a placeholder must not take it */
    ursatz_gui_motifs_routes,  ursatz_gui_library_routes, ursatz_gui_collections_routes, ursatz_gui_composer_routes,
    ursatz_gui_styles_routes,  ursatz_gui_links_routes,   ursatz_gui_jobs_routes,        ursatz_gui_subjects_routes,
    ursatz_gui_materials_routes,    ursatz_gui_events_routes,   ursatz_gui_workshop_routes,
};

/* civetweb's one handler for "/api": the row that fits the method and the path, else 405 when another method fits it, else 404. */
static int api_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  int path_matched = 0;
  for (size_t file = 0; file < sizeof(kRouteFiles) / sizeof(kRouteFiles[0]); file++) {
    size_t count = 0;
    const UrsatzGuiRoute *rows = kRouteFiles[file](&count);
    for (size_t i = 0; i < count; i++) {
      UrsatzGuiRouteArgs args;
      if (!ursatz_gui_route_match(rows[i].pattern, info->local_uri, &args)) {
        continue;
      }
      if (strcmp(rows[i].method, info->request_method) == 0) {
        return rows[i].handler(conn, &args);
      }
      path_matched = 1;
    }
  }
  return path_matched ? ursatz_gui_send_failure(conn, 405, "method not allowed") : ursatz_gui_send_failure(conn, 404, "not found");
}

void ursatz_gui_routes_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, "/api", api_handler, NULL);
}
