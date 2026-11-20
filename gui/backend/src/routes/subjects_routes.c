#include "subjects_routes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "materials_util.h"


/* --- subjects ------------------------------------------------------------ */

static int handle_list_subjects(struct mg_connection *conn) {
  return QUERY(conn, "subject.list", ursatz_gui_failed, false, NULL);
}

static int handle_subject_groups(struct mg_connection *conn) {
  return QUERY(conn, "subject.groups", ursatz_gui_failed, false, NULL);
}

static int handle_get_subject(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "subject.get", ursatz_gui_failed, false, "id", id);
}

#define ARG(args, i) ((args)->values[i])

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_subjects(conn); }
static int route_groups(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_subject_groups(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_subject(conn, ARG(args, 0)); }
static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/subjects", route_list},
    {"GET", "/api/subjects/groups", route_groups},
    {"GET", "/api/subjects/<id>", route_get},
};

const UrsatzGuiRoute *ursatz_gui_subjects_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
