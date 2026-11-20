#ifndef URSATZ_GUI_ROUTE_TABLE_H
#define URSATZ_GUI_ROUTE_TABLE_H

/*
 * The one route table of the GUI backend (R-647): every route is a row of {method, pattern, handler}. A pattern is the path with "<id>", "<n>" and
 * "<name>" in the place of one segment each; one parser matches the request's path against the rows and hands the handler the segments the
 * placeholders took, in order. civetweb has one handler for "/api", which dispatches here; a path no row matches answers 404 and a path a row
 * matches under another method answers 405.
 */

#include <stddef.h>

struct mg_connection;
struct mg_context;

#define URSATZ_GUI_ROUTE_ARGS_MAX 6
#define URSATZ_GUI_ROUTE_ARG_BYTES 256

typedef struct {
  char values[URSATZ_GUI_ROUTE_ARGS_MAX][URSATZ_GUI_ROUTE_ARG_BYTES]; /* the segments of the placeholders, in order */
  size_t count;
} UrsatzGuiRouteArgs;

typedef int (*UrsatzGuiRouteHandler)(struct mg_connection *conn, const UrsatzGuiRouteArgs *args);

typedef struct {
  const char *method;
  const char *pattern;
  UrsatzGuiRouteHandler handler;
} UrsatzGuiRoute;

/* Registers the one "/api" handler of ctx; it dispatches through the rows of every route file. */
void ursatz_gui_routes_register(struct mg_context *ctx);

/* Matches path (no query) against pattern: true, and the placeholders' segments in args, when it fits. Exposed for the route table test. */
int ursatz_gui_route_match(const char *pattern, const char *path, UrsatzGuiRouteArgs *args);

#endif /* URSATZ_GUI_ROUTE_TABLE_H */
