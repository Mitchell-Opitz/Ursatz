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
/* The caps of the one body reader (R-647): JSON 1 MB, an upload 64 MB, a MIDI motif 256 KB. */
#define URSATZ_GUI_UPLOAD_MAX_BYTES (64 * 1024 * 1024)
#define URSATZ_GUI_MIDI_MAX_BYTES (256 * 1024)

/* Reads the request body up to cap bytes into *data (NUL-terminated, the caller frees) and its length into *size (may be NULL). Over the cap is REQUEST at
 * "/body/size" (413), an unreadable body REQUEST at "/body" (400). */
UrsatzStatus ursatz_gui_read_body(struct mg_connection *conn, size_t cap, char **data, size_t *size, UrsatzError *error);

/* The HTTP status of a failed workshop call (R-639): 200 for OK, 400 for a request refused (422 when the error names the request member at fault),
 * 422 for UNSATISFIABLE, 404 for NOT_FOUND, 409 for CONFLICT, 499 for CANCELLED, 500 for every other failure. error may be NULL (no path). */
int ursatz_gui_http_status(UrsatzStatus status, const UrsatzError *error);

/* Whether text can name a stored item: 1 to 255 letters, digits, '-' and '_'. The workshop checks the same again; a route uses it to answer a
 * path no item can have as not found without a call. */
bool ursatz_gui_valid_id(const char *text);

/* Sends body as application/json with the given status line. Returns status. */
int ursatz_gui_send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body);

/* The reason phrase of an HTTP status: the ones the routes send, "Bad Request" for any other below 500 and "Internal
 * Server Error" for any other. */
const char *ursatz_gui_reason_phrase(int status);

/* Sends body as a 200 application/json answer. */
void ursatz_gui_send_json(struct mg_connection *conn, const char *body);

/* The failure envelope with status and the message as its error. Returns status. */
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

/* The failure envelope (R-647): {"ok": false, "error": <message>, "code": <status name>, "path": <pointer, when set>}. code is the name of the status
 * a failure with that HTTP status stands for when NULL; path may be NULL. Returns status. */
int ursatz_gui_send_failure_at(struct mg_connection *conn, int status, const char *message, const char *code, const char *path);

/* A failed workshop call as the envelope, with the HTTP status of ursatz_gui_http_status (a request refused at a member is 422). */
int ursatz_gui_failed(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);

/* The request's query variables as params with the path parameters given as name/value pairs after them (NULL ends the list). */
cJSON *ursatz_gui_request_params(struct mg_connection *conn, ...);

/* Sends the reply's JSON answer with status, "ok": true in front of its members (kept when it has one); frees the reply. Returns status. */
int ursatz_gui_send_reply_ok(struct mg_connection *conn, int status, WorkshopReply *reply);

/* Sends the reply's JSON as the whole answer with status; frees the reply. Returns status. */
int ursatz_gui_send_reply_json(struct mg_connection *conn, int status, WorkshopReply *reply);

/* Sends the reply's bytes with its content type as a 200 answer; frees the reply. Returns 200. */
int ursatz_gui_send_reply_bytes(struct mg_connection *conn, WorkshopReply *reply);

#endif
