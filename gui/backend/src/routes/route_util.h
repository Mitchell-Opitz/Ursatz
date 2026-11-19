#ifndef URSATZ_GUI_ROUTE_UTIL_H
#define URSATZ_GUI_ROUTE_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <civetweb.h>

#include <common/status.h>
#include <ursatz_workshop/workshop_api.h>

/* The largest request body ursatz_gui_read_json_body accepts. */
#define URSATZ_GUI_JSON_BODY_MAX_BYTES (1024 * 1024)

/* The message of this thread's last service failure (service/last_error.h), or fallback when the failing call gave none. */
const char *ursatz_gui_failure_message(const char *fallback);

/* Answers a failed service call: the HTTP status of the table for status and the last error, with its message (or fallback). */
int ursatz_gui_send_service_failure(struct mg_connection *conn, UrsatzStatus status, const char *fallback);

/* Sends body as application/json with the given status line. Returns status. */
int ursatz_gui_send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body);

/* The reason phrase of an HTTP status: the ones the routes send, "Bad Request" for any other below 500 and "Internal
 * Server Error" for any other. */
const char *ursatz_gui_reason_phrase(int status);

/* Sends body as a 200 application/json answer. */
void ursatz_gui_send_json(struct mg_connection *conn, const char *body);

/* Sends {"ok":false,"message":...} with status. The message goes through cJSON, so it is escaped. Returns status. */
int ursatz_gui_send_failure(struct mg_connection *conn, int status, const char *message);

/* Sends object as the answer with status and frees it (a failure 500 when it cannot be printed). Returns status. */
int ursatz_gui_send_object(struct mg_connection *conn, int status, cJSON *object);

/* The request body as a JSON object (caller frees), up to max_bytes. NULL when it cannot be read, is too large, is not
 * JSON, or is not an object. With status_or_null NULL nothing is sent and the caller answers. Otherwise the failure is
 * sent as {"ok":false,"error":...} (400, 413 or 500) and *status is what the caller returns; on success *status is 200. */
cJSON *ursatz_gui_read_json_body_max(struct mg_connection *conn, size_t max_bytes, int *status_or_null);

/* ursatz_gui_read_json_body_max with URSATZ_GUI_JSON_BODY_MAX_BYTES. */
cJSON *ursatz_gui_read_json_body(struct mg_connection *conn, int *status_or_null);

/* The query string of a request as a params object: every variable by name, a variable that repeats as an array of its values. */
cJSON *ursatz_gui_query_params(const struct mg_request_info *info);

/* Runs the workshop query name with params (a params object, consumed; NULL for none) and the file a few queries read. The workshop is the
 * one the server runs. */
UrsatzStatus ursatz_gui_run_query(const char *name, cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply, UrsatzError *error);

/* How a route answers a failed call: the failure's status and the error it holds. */
typedef int (*UrsatzGuiFailureFn)(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);

/* Runs the query name with params (consumed) and answers it: bytes as a file, JSON as {"ok": true, ...members} (bare: as it is), a
 * failure through failed. Returns the HTTP status sent. */
int ursatz_gui_answer_query(struct mg_connection *conn, const char *name, cJSON *params, const WorkshopBytes *bytes,
                            UrsatzGuiFailureFn failed, bool bare);

/* Runs the command name on target ("" for none) as the GUI's one person (actor "michelle") with params (consumed; NULL for none) and the file a
 * few commands take, and answers it as ursatz_gui_answer_query does (the journal_seq of the reply is not sent). With success_json, the
 * reply is sent bare. */
int ursatz_gui_answer_command(struct mg_connection *conn, const char *name, const char *target, cJSON *params, const WorkshopBytes *bytes,
                              UrsatzGuiFailureFn failed, bool bare);

/* Runs the command name as ursatz_gui_answer_command does, without answering: the reply (its journal_seq removed) and the failure are the
 * caller's. */
UrsatzStatus ursatz_gui_run_command(const char *name, const char *target, cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply,
                                    UrsatzError *error);

/* The usual failures: the status of the table (the path of a request failure ignored) with {"ok": false, "message"} (message) or
 * {"ok": false, "error", "path"?} (error). B12 gives every route the one envelope. */
int ursatz_gui_failed_message(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);
int ursatz_gui_failed_error(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);

/* The request's query variables as params with the path parameters given as name/value pairs after them (NULL ends the list). */
cJSON *ursatz_gui_request_params(struct mg_connection *conn, ...);

/* Sends the reply's JSON answer with status, "ok": true in front of its members (kept when it has one); frees the reply. Returns status. */
int ursatz_gui_send_reply_ok(struct mg_connection *conn, int status, WorkshopReply *reply);

/* Sends the reply's JSON as the whole answer with status; frees the reply. Returns status. */
int ursatz_gui_send_reply_json(struct mg_connection *conn, int status, WorkshopReply *reply);

/* Sends the reply's bytes with its content type as a 200 answer; frees the reply. Returns 200. */
int ursatz_gui_send_reply_bytes(struct mg_connection *conn, WorkshopReply *reply);

#endif
