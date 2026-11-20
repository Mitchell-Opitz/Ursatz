#include "subjects_routes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "materials_util.h"


/* --- subjects ------------------------------------------------------------ */

static int handle_list_subjects(struct mg_connection *conn) {
  return QUERY(conn, "subject.list", failed_material, false, NULL);
}

static int handle_subject_groups(struct mg_connection *conn) {
  return QUERY(conn, "subject.groups", failed_material, false, NULL);
}

static int handle_create_subject(struct mg_connection *conn) {
  return command_with_body(conn, "subject.create", "", failed_material_cmd);
}

static int handle_get_subject(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "subject.get", failed_material, false, "id", id);
}

static int handle_patch_subject(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "subject.update", id, failed_material_cmd);
}

static int handle_delete_subject(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "subject.delete", id, NULL, NULL, failed_material_cmd, false);
}

/* kind is "items", "pieces" or "related"; POST adds the link, DELETE removes it. */
static int handle_subject_link(struct mg_connection *conn, const char *method, const char *id, const char *kind,
                               const char *other) {
  bool post = strcmp(method, "POST") == 0;
  const char *member = strcmp(kind, "items") == 0 ? "item_id" : strcmp(kind, "pieces") == 0 ? "piece_id" : "other_id";
  const char *action = strcmp(kind, "items") == 0 ? (post ? "subject.link-item" : "subject.unlink-item")
                       : strcmp(kind, "pieces") == 0 ? (post ? "subject.link-piece" : "subject.unlink-piece")
                                                     : (post ? "subject.relate" : "subject.unrelate");
  cJSON *params = cJSON_CreateObject();
  if (params) {
    cJSON_AddStringToObject(params, member, other);
  }
  return ursatz_gui_answer_command(conn, action, id, params, NULL, failed_material_cmd, false);
}

#define ARG(args, i) ((args)->values[i])

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_subjects(conn); }
static int route_create(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_create_subject(conn); }
static int route_groups(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_subject_groups(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_subject(conn, ARG(args, 0)); }
static int route_patch(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_patch_subject(conn, ARG(args, 0)); }
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_delete_subject(conn, ARG(args, 0)); }
static int route_link_item(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "POST", ARG(args, 0), "items", ARG(args, 1)); }
static int route_unlink_item(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "DELETE", ARG(args, 0), "items", ARG(args, 1)); }
static int route_link_piece(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "POST", ARG(args, 0), "pieces", ARG(args, 1)); }
static int route_unlink_piece(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "DELETE", ARG(args, 0), "pieces", ARG(args, 1)); }
static int route_relate(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "POST", ARG(args, 0), "related", ARG(args, 1)); }
static int route_unrelate(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_subject_link(conn, "DELETE", ARG(args, 0), "related", ARG(args, 1)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/subjects", route_list},
    {"POST", "/api/subjects", route_create},
    {"GET", "/api/subjects/groups", route_groups},
    {"GET", "/api/subjects/<id>", route_get},
    {"PATCH", "/api/subjects/<id>", route_patch},
    {"DELETE", "/api/subjects/<id>", route_delete},
    {"POST", "/api/subjects/<id>/items/<id>", route_link_item},
    {"DELETE", "/api/subjects/<id>/items/<id>", route_unlink_item},
    {"POST", "/api/subjects/<id>/pieces/<id>", route_link_piece},
    {"DELETE", "/api/subjects/<id>/pieces/<id>", route_unlink_piece},
    {"POST", "/api/subjects/<id>/related/<id>", route_relate},
    {"DELETE", "/api/subjects/<id>/related/<id>", route_unrelate},
};

const UrsatzGuiRoute *ursatz_gui_subjects_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
