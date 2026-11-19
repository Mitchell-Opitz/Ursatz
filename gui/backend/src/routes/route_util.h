#ifndef URSATZ_GUI_ROUTE_UTIL_H
#define URSATZ_GUI_ROUTE_UTIL_H

#include <stddef.h>

#include <cJSON.h>
#include <civetweb.h>

#include <common/status.h>

/* The largest request body ursatz_gui_read_json_body accepts. */
#define URSATZ_GUI_JSON_BODY_MAX_BYTES (1024 * 1024)

/*
 * The HTTP status of a service status (R-639): OK 200; REQUEST 400, or 422 when the error names a path (a Composer refusal);
 * UNSATISFIABLE 422; NOT_FOUND 404; CONFLICT 409; CANCELLED 499; FORMAT, IO, ALLOC and INTERNAL 500. error may be NULL.
 */
int ursatz_gui_http_status(UrsatzStatus status, const UrsatzError *error);

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

#endif
