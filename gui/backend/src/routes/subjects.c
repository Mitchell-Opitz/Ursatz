#include "subjects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "materials_util.h"

static const char *kSubjectsPrefix = "/api/subjects";

/* --- subjects ------------------------------------------------------------ */

static int send_subject_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "subject store failure");
}


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
  if (!post && strcmp(method, "DELETE") != 0) {
    return send_method_not_allowed(conn);
  }
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

static int route_subjects(struct mg_connection *conn, const char *method, char segments[][256], int count) {
  bool get = strcmp(method, "GET") == 0;
  if (count == 0) {
    if (get) {
      return handle_list_subjects(conn);
    }
    return strcmp(method, "POST") == 0 ? handle_create_subject(conn) : send_method_not_allowed(conn);
  }
  if (count == 1) {
    if (strcmp(segments[0], "groups") == 0) {
      return get ? handle_subject_groups(conn) : send_method_not_allowed(conn);
    }
    if (get) {
      return handle_get_subject(conn, segments[0]);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_subject(conn, segments[0]);
    }
    return strcmp(method, "DELETE") == 0 ? handle_delete_subject(conn, segments[0]) : send_method_not_allowed(conn);
  }
  if (count == 3 && (strcmp(segments[1], "items") == 0 || strcmp(segments[1], "pieces") == 0 ||
                     strcmp(segments[1], "related") == 0)) {
    return handle_subject_link(conn, method, segments[0], segments[1], segments[2]);
  }
  return send_not_found(conn);
}

/*
 * Handler registered at "/api/subjects"; same dispatch as the materials handler.
 */
static int subjects_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  char segments[8][256];
  int count = split_path(info->local_uri + strlen(kSubjectsPrefix), segments);
  if (count < 0) {
    return send_not_found(conn);
  }
  return route_subjects(conn, info->request_method, segments, count);
}

void ursatz_gui_subjects_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kSubjectsPrefix, subjects_handler, NULL);
}
