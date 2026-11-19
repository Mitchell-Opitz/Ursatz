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
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *kind = optional_string(body, "kind", &bad);
  const char *note = optional_string(body, "note", &bad);
  if (bad || !name || !kind) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "name and kind are required; name, kind and note are strings");
  }
  cJSON *subject = NULL;
  UrsatzStatus result = workshop_subject_create(name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_get_subject(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "subject.get", failed_material, false, "id", id);
}

static int handle_patch_subject(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *kind = optional_string(body, "kind", &bad);
  const char *note = optional_string(body, "note", &bad);
  if (bad || (!name && !kind && !note)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give name, kind and/or note as strings");
  }
  cJSON *subject = NULL;
  UrsatzStatus result = workshop_subject_update(id, name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_delete_subject(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_subject_delete(id);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok(conn);
}

/* {"ok":true,"subject":...} plus "already" for a link or relation that was in place. */
static int send_subject_after(struct mg_connection *conn, cJSON *subject, bool already, bool with_already) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !subject || !cJSON_AddBoolToObject(object, "ok", 1) ||
      (with_already && !cJSON_AddBoolToObject(object, "already", already ? 1 : 0))) {
    cJSON_Delete(object);
    cJSON_Delete(subject);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddItemToObject(object, "subject", subject);
  return ursatz_gui_send_object(conn, 200, object);
}

/* kind is "items", "pieces" or "related"; POST adds the link, DELETE removes it. */
static int handle_subject_link(struct mg_connection *conn, const char *method, const char *id, const char *kind,
                               const char *other) {
  bool post = strcmp(method, "POST") == 0;
  if (!post && strcmp(method, "DELETE") != 0) {
    return send_method_not_allowed(conn);
  }
  bool already = false;
  cJSON *subject = NULL;
  UrsatzStatus result;
  if (post && strcmp(kind, "items") == 0) {
    /* A variant belongs to its motif and has no subjects (R-187). An absent item is left to the subject service. */
    cJSON *body = NULL;
    bool variant = workshop_material_item_get(other, NULL, &body) == URSATZ_OK &&
                   workshop_material_item_is_variant(body);
    cJSON_Delete(body);
    if (variant) {
      return send_failure(conn, 400, "variants have no subjects; save it as a new motif first");
    }
  }
  if (strcmp(kind, "items") == 0) {
    result = post ? workshop_subject_link_item(id, other, &already, &subject)
                  : workshop_subject_unlink_item(id, other, &subject);
  } else if (strcmp(kind, "pieces") == 0) {
    result = post ? workshop_subject_link_piece(id, other, &already, &subject)
                  : workshop_subject_unlink_piece(id, other, &subject);
  } else {
    result = post ? workshop_subject_relate(id, other, &already, &subject)
                  : workshop_subject_unrelate(id, other, &subject);
  }
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_subject_after(conn, subject, already, post);
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
